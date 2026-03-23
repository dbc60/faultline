/**
 * @file flp_log_service.c
 * @author Douglas Cuthbertson
 * @brief Platform side of the log service implementation
 * @version 0.1
 * @date 2026-02-09
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_log_types.h> // for FLLogLevel, FL_WRITE_LOG_FN, FLLogS...
#include <faultline/fl_threads.h>   // for mtx_destroy, mtx_init, mtx_lock
#include <flp_log_service.h>        // for FLP_INIT_LOG_SERVICE_FN, flp_con...
#include <stdarg.h>                 // for va_end, va_list, va_start
#include <stdbool.h>                // for false, bool, true
#include <stdio.h>                  // for fprintf, NULL, fflush, FILE, stdout
#include <stdlib.h>                 // for abort
#include <string.h>                 // for strrchr, strerror_s
#include <time.h>                   // for localtime, strftime, time, time_t

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>           // IWYU pragma: keep - sets target architecture
#include <processthreadsapi.h> // for GetCurrentThreadId
#endif

#define __STDC_WANT_LIB_EXT1__ 1
#include <corecrt.h> // for errno_t
#if defined(__clang__) || defined(__GNUC__)
#include <sec_api/stdio_s.h> // for fopen_s
#endif

FL_WRITE_LOG_FN(flp_write_log);

// ---------------------------------------------------------------------------
// Private logger struct
// ---------------------------------------------------------------------------

typedef struct {
    bool       close_output;
    bool       enabled;
    FLLogLevel min_level;
    FILE      *output;
    mtx_t      mutex;
    bool       initialized;
} FLPLogger;

static FLPLogger g_logger;

static FLLogService g_log_service = {
    .write = flp_write_log,
};

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

// Keep these in sync with enum FLLogLevel in fl_log_types.h
static char const *level_names[]
    = {"FATAL", "ERROR", "WARN", "INFO", "VERBOSE", "DEBUG", "TRACE"};

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

#ifdef __STDC_LIB_EXT1__
static void constraint_handler(char const *restrict msg, void *restrict ptr,
                               errno_t error) {
    FL_UNUSED(ptr);
    fprintf(stderr, "%s: %d\n", msg ? msg : "unknown error", error);
}
#endif

// ---------------------------------------------------------------------------
// Platform lifecycle functions
// ---------------------------------------------------------------------------

void flp_log_init_custom(FLLogLevel level, char const *path) {
    if (!g_logger.initialized) {
        g_logger.enabled   = true;
        g_logger.min_level = level;
        if (path != NULL) {
            flp_log_set_output_path(path);
            g_logger.close_output = true;
        } else {
            g_logger.output       = stdout;
            g_logger.close_output = false;
        }

        if (mtx_init(&g_logger.mutex, mtx_plain) != thrd_success) {
            fprintf(stderr, "Fatal: Failed to initialize logging mutex\n");
            fflush(stderr);
            abort();
        }
        g_logger.initialized = true;
    }
}

void flp_log_init(void) {
    flp_log_init_custom(LOG_LEVEL_INFO, NULL);
}

void flp_log_cleanup(void) {
    if (g_logger.output != NULL) {
        fflush(g_logger.output);
    }

    if (g_logger.close_output && g_logger.output != NULL) {
        fclose(g_logger.output);
        g_logger.output       = NULL;
        g_logger.close_output = false;
    }

    mtx_destroy(&g_logger.mutex);
    g_logger.initialized = false;
}

void flp_log_set_level(FLLogLevel level) {
    g_logger.min_level = level;
}

void flp_log_set_output(FILE *file) {
    g_logger.output       = file ? file : stdout;
    g_logger.close_output = false;
}

void flp_log_set_output_path(char const *path) {
    FILE *file;
#ifdef __STDC_LIB_EXT1__
    constraint_handler_t previous_handler = set_constraint_handler_s(constraint_handler);
#endif
    errno_t error = fopen_s(&file, path, "a+");
    if (error != 0) {
        char buf[256] = {0};
        strerror_s(buf, sizeof buf, error);
        fprintf(stderr, "Error: failed to open %s (error: %s); falling back to stdout\n",
                path, buf);
        file = stdout;
    } else {
        g_logger.close_output = true;
    }
#ifdef __STDC_LIB_EXT1__
    set_constraint_handler_s(previous_handler);
#endif

    g_logger.output = file;
}

// ---------------------------------------------------------------------------
// Service write implementation
// ---------------------------------------------------------------------------

FL_WRITE_LOG_FN(flp_write_log) {
    if (!g_logger.enabled || level > g_logger.min_level) {
        return;
    }

    mtx_lock(&g_logger.mutex);

    char timestamp[32];
    format_timestamp(timestamp, sizeof(timestamp));

    unsigned long tid = get_thread_id();

    char const *filename = get_filename(file);

    // the longest log-level name is 7 characters ("VERBOSE"), so we align all log-levels
    // with a 7-character width.
    fprintf(g_logger.output, "[%s] [%-7s] [T:%lu] %s:%3d", timestamp, level_names[level],
            tid, filename, line);

    if (id && id[0] != '\0') {
        fprintf(g_logger.output, " [%s]", id);
    }

    fprintf(g_logger.output, " ");

    va_list args;
    va_start(args, format);
    vfprintf(g_logger.output, format, args);
    va_end(args);

    fprintf(g_logger.output, "\n");
    fflush(g_logger.output);

    mtx_unlock(&g_logger.mutex);
}

FLP_INIT_LOG_SERVICE_FN(flp_init_log_service) {
    fla_set(&g_log_service, sizeof g_log_service);
}
