/**
 * @file flp_log_service.c
 * @author Douglas Cuthbertson
 * @brief Platform side of the log service implementation
 * @version 0.2
 * @date 2026-02-09
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_log_service.h> // for FLLogLevel, FL_WRITE_LOG_FN, FLLogS...
#include <faultline/fl_try.h>         // FL_ASSERT_REASON_IMPL indirectly
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_NOT_NULL
#include <flp_stream_service.h> // for flp_stream_open/write/close/console, FLFile
#include <flp_log_service.h>    // for FLP_INIT_LOG_SERVICE_FN
#include "fl_lock.h"            // for FLLock, fl_lock_init, fl_lock_acquire
#include <stdarg.h>             // for va_end, va_list, va_start
#include <stdbool.h>            // for false, bool, true
#include <stdio.h>              // for fprintf, snprintf, NULL, fflush, FILE, stdout
#include <stdlib.h>             // for abort
#include <string.h>             // for strrchr
#include <time.h>               // for localtime, strftime, time, time_t

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>           // IWYU pragma: keep - sets target architecture
#include <processthreadsapi.h> // for GetCurrentThreadId
#else                          // POSIX
#include <pthread.h>           // for pthread_self
#include <stdint.h>            // for uintptr_t
#endif

FL_WRITE_LOG_FN(flp_write_log);

// ---------------------------------------------------------------------------
// Private logger struct
// ---------------------------------------------------------------------------

// stdout or a caller-supplied FILE* is never owned; a handle opened via the stream
// service always is safe to close unconditionally, because close() is a documented no-op
// on a console-obtained handle, so a path-append handle and a console handle share one
// output kind here. Ownership is therefore implied by which kind is active, rather than
// tracked with a separate flag that could in principle disagree with it.
typedef enum {
    FLP_LOG_OUTPUT_STDIO,  // stdio output is valid and handles are never closed by this
                           // module
    FLP_LOG_OUTPUT_STREAM, // stream output is valid; handles are opened and closed by
                           // this module
} FLPLogOutputKind;

typedef struct {
    bool             enabled;
    FLLogLevel       max_level;
    FLPLogOutputKind output_kind;
    FILE            *stdio_output;  // valid iff output_kind == FLP_LOG_OUTPUT_STDIO
    FLFile          *stream_output; // valid iff output_kind == FLP_LOG_OUTPUT_STREAM
    FLLock           mutex;
    bool             initialized;
} FLPLogger;

static FLPLogger g_logger;

static FLLogService g_log_service = {
    .write = flp_write_log,
};

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

// Keep these in sync with enum FLLogLevel in fl_log_service.h
static char const *level_names[]
    = {"FATAL", "ERROR", "WARN", "INFO", "VERBOSE", "DEBUG", "TRACE"};

// Bounds one formatted record (timestamp + level + thread id + file:line + optional
// id + message + newline). Every LOG_* call site in this codebase is well under a
// tenth of this; the headroom costs nothing on a non-recursive, mutex-serialized
// stack frame, and any finite bound is new relative to the previously-unbounded
// streamed fprintf output, so the generous choice is the safe one.
#define FLP_LOG_LINE_MAX 4096

static void format_timestamp(char *buffer, size_t size) {
    time_t     now     = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static unsigned long get_thread_id(void) {
#if defined(_WIN32) || defined(WIN32)
    return (unsigned long)GetCurrentThreadId();
#else
    return (unsigned long)(uintptr_t)pthread_self();
#endif
}

static char const *get_filename(char const *path) {
    char const *last_slash     = strrchr(path, '/');
    char const *last_backslash = strrchr(path, '\\');

    char const *last_sep = last_slash;
    if (last_backslash > last_sep) {
        last_sep = last_backslash;
    }

    return last_sep ? last_sep + 1 : path;
}

// Appends formatted text into buf at byte offset pos, clamping so pos never exceeds
// bufsize-1. vsnprintf's return value is the length that would have been written, which
// can exceed the space actually available once an earlier piece truncates; using that
// value unclamped would let "bytes remaining" (bufsize - pos) underflow on the next
// call. Clamping guarantees buf stays NUL-terminated and pos stays a valid offset after
// every partial append, so an oversized record truncates cleanly instead of corrupting
// the offset math.
static int appendv(char *buf, size_t bufsize, int pos, char const *fmt, va_list args) {
    if (bufsize == 0) {
        return 0;
    }
    size_t cap = bufsize - 1; // reserve room for vsnprintf's own NUL
    if (pos < 0 || (size_t)pos >= cap) {
        return (int)cap; // already full; skip the vsnprintf call entirely
    }

    int n = vsnprintf(buf + pos, bufsize - (size_t)pos, fmt, args);
    if (n < 0) {
        return pos; // encoding error: drop this piece, keep prior content
    }

    size_t new_pos = (size_t)pos + (size_t)n;
    return (int)(new_pos > cap ? cap : new_pos);
}

// Variadic convenience wrapper over appendv, for the fixed pieces of a record
// (prefix, optional id, separators, newline). flp_write_log's own va_list (from its
// message format/args) is forwarded to appendv directly instead, since a va_list can
// only be consumed once.
static int append(char *buf, size_t bufsize, int pos, char const *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = appendv(buf, bufsize, pos, fmt, args);
    va_end(args);
    return result;
}

// Release the currently active output before installing a new one. Only stream-service
// output is owned by the logger (stdout or a caller-supplied FILE* is never closed
// here). Must be called with g_logger.mutex held.
static void release_current_output_locked(void) {
    if (g_logger.output_kind == FLP_LOG_OUTPUT_STREAM
        && g_logger.stream_output != NULL) {
        flp_stream_close(g_logger.stream_output);
    }
    g_logger.stream_output = NULL;
}

// ---------------------------------------------------------------------------
// Platform lifecycle functions
// ---------------------------------------------------------------------------

void flp_log_init_custom(FLLogLevel level, char const *path) {
    if (!g_logger.initialized) {
        // Must precede any call that locks g_logger.mutex (flp_log_set_output[_path]
        // below, and flp_write_log from other threads once initialized is visible).
        if (!fl_lock_init(&g_logger.mutex)) {
            fprintf(stderr, "Fatal: Failed to initialize logging mutex\n");
            fflush(stderr);
            abort();
        }

        g_logger.enabled   = true;
        g_logger.max_level = level;
        if (path != NULL) {
            flp_log_set_output_path(path);
        } else {
            flp_log_set_output(stdout);
        }

        g_logger.initialized = true;
    }
}

void flp_log_init(void) {
    flp_log_init_custom(LOG_LEVEL_INFO, NULL);
}

void flp_log_cleanup(void) {
    fl_lock_acquire(&g_logger.mutex);

    if (g_logger.output_kind == FLP_LOG_OUTPUT_STDIO) {
        if (g_logger.stdio_output != NULL) {
            fflush(g_logger.stdio_output);
        }
    } else if (g_logger.stream_output != NULL) {
        // flp_stream_write is a direct synchronous WriteFile with no libc-side
        // buffering, so there is nothing to flush here -- just release the handle.
        flp_stream_close(g_logger.stream_output);
        g_logger.stream_output = NULL;
    }

    fl_lock_release(&g_logger.mutex);

    fl_lock_destroy(&g_logger.mutex);
    g_logger.initialized = false;
}

void flp_log_set_level(FLLogLevel level) {
    g_logger.max_level = level;
}

void flp_log_set_output(FILE *file) {
    fl_lock_acquire(&g_logger.mutex);
    release_current_output_locked();
    g_logger.output_kind  = FLP_LOG_OUTPUT_STDIO;
    g_logger.stdio_output = file ? file : stdout;
    fl_lock_release(&g_logger.mutex);
}

void flp_log_set_output_path(char const *path) {
    fl_lock_acquire(&g_logger.mutex);
    release_current_output_locked();

    // The stream service opens append-mode (Win32 FILE_APPEND_DATA), so every write
    // is atomic at end of file: several writers, including other processes appending to
    // the same path, interleave whole records without the log module needing to
    // implement that guarantee itself.
    FLFile *file = flp_stream_open(path);
    if (file == NULL) {
        // flp_stream_open reports failure as NULL only; the stream service's contract
        // carries no error code, so this message is necessarily less specific than a
        // fopen_s/errno-based one would be.
        fprintf(stderr, "Error: failed to open %s; falling back to stdout\n", path);
        g_logger.output_kind  = FLP_LOG_OUTPUT_STDIO;
        g_logger.stdio_output = stdout; // stdout is not ours to close
    } else {
        g_logger.output_kind   = FLP_LOG_OUTPUT_STREAM;
        g_logger.stream_output = file;
    }

    fl_lock_release(&g_logger.mutex);
}

// Opt-in only: nothing in this module calls this automatically. The default output
// (flp_log_init / flp_log_init_custom with no path) stays raw stdio via
// flp_log_set_output(stdout). A WriteFile-based console write here and a CRT-buffered
// fprintf(stdout,...) elsewhere in the process are two independently buffered paths to
// the same OS handle and can interleave out of order, so nothing switches to this path
// automatically. A caller that wants the log service's console output to be
// fault-injectable (or wants stderr instead of stdout) must call this explicitly.
void flp_log_set_output_console(FLConsoleStream which) {
    fl_lock_acquire(&g_logger.mutex);
    release_current_output_locked();

    FLFile *file = flp_stream_console(which);
    if (file == NULL) {
        fprintf(stderr, "Error: no console stream available; falling back to stdout\n");
        g_logger.output_kind  = FLP_LOG_OUTPUT_STDIO;
        g_logger.stdio_output = stdout;
    } else {
        g_logger.output_kind   = FLP_LOG_OUTPUT_STREAM;
        g_logger.stream_output = file;
    }

    fl_lock_release(&g_logger.mutex);
}

// ---------------------------------------------------------------------------
// Service write implementation
// ---------------------------------------------------------------------------

FL_WRITE_LOG_FN(flp_write_log) {
    if (!g_logger.enabled || level > g_logger.max_level) {
        return;
    }

    // The lock covers formatting and the single write together, not just the write.
    // The stream service's append semantics only make the write itself atomic. They
    // say nothing about the handle's lifetime. If the lock released after formatting
    // and reacquired only around flp_stream_write, a concurrent set_output,
    // set_output_path, set_output_console, or cleanup call could close
    // g_logger.stream_output in the gap, and this thread would then write through (or
    // close a second time) a handle that is no longer valid. Locking the whole section
    // prevents that handle swap from occurring mid-write.
    fl_lock_acquire(&g_logger.mutex);

    char timestamp[32];
    format_timestamp(timestamp, sizeof(timestamp));

    unsigned long tid = get_thread_id();

    char const *filename = get_filename(file);

    char record[FLP_LOG_LINE_MAX];
    int  pos = 0;

    // the longest log-level name is 7 characters ("VERBOSE"), so we align all log-levels
    // with a 7-character width.
    pos = append(record, sizeof record, pos, "[%s] [%-7s] [T:%lu] %s:%3d", timestamp,
                 level_names[level], tid, filename, line);

    if (id && id[0] != '\0') {
        pos = append(record, sizeof record, pos, " [%s]", id);
    }

    pos = append(record, sizeof record, pos, " ");

    va_list args;
    va_start(args, format);
    pos = appendv(record, sizeof record, pos, format, args);
    va_end(args);

    pos = append(record, sizeof record, pos, "\n");

    size_t len = (size_t)pos;
    if (g_logger.output_kind == FLP_LOG_OUTPUT_STREAM) {
        flp_stream_write(g_logger.stream_output, record, len);
    } else if (g_logger.stdio_output != NULL) {
        fwrite(record, 1, len, g_logger.stdio_output);
        fflush(g_logger.stdio_output);
    }

    fl_lock_release(&g_logger.mutex);
}

FLP_INIT_LOG_SERVICE_FN(flp_init_log_service) {
    FL_ASSERT_NOT_NULL(fla_set);
    fla_set(&g_log_service, sizeof g_log_service);
}
