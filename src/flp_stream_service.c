/**
 * @file flp_stream_service.c
 * @author Douglas Cuthbertson
 * @brief Platform side of the stream service implementation (Win32).
 * @version 0.1
 * @date 2026-07-22
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_stream_service.h>           // FL_STREAM_*_FN, FLStreamService
#include <faultline/fl_macros.h>                   // FL_ARRAY_COUNT
#include <faultline/fl_memory.h>                   // FL_MALLOC, FL_FREE
#include <faultline/fl_try.h>                      // FL_ASSERT_REASON_IMPL indirectly
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_NOT_NULL
#include <flp_stream_service.h>                    // FLP_INIT_STREAM_SERVICE_FN
#include <stdbool.h>                               // bool
#include <stddef.h>                                // NULL, size_t
#include <stdint.h>                                // UINT32_MAX
#include <string.h>                                // memcpy
#include <wchar.h>                                 // wcslen, wcsncmp

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // CreateFileW, GetStdHandle, MultiByteToWideChar, WriteFile
#endif

// An append-opened handle is the raw Win32 HANDLE itself, exactly like the
// synchronous file service: a live handle is never NULL, so a NULL return cleanly
// signals open failure without a separate wrapper allocation. Console handles are
// the two statics below instead -- their FLFile* identity is the slot's address,
// not the HANDLE value, so write()/close() can tell a console handle apart from an
// append-file handle without inspecting or assuming anything about a HANDLE's bit
// pattern. This keeps flp_stream_open allocation-free in the common (short-path)
// case, matching flp_file_open: an unconditional per-open allocation would break
// the platform host's bootstrap order, where the log service opens its default
// output file before the memory service exists.
typedef struct FlpConsoleHandle {
    HANDLE handle;
} FlpConsoleHandle;

static FlpConsoleHandle g_stdout_handle;
static FlpConsoleHandle g_stderr_handle;

static bool is_console_handle(FLFile *f) {
    return f == (FLFile *)&g_stdout_handle || f == (FLFile *)&g_stderr_handle;
}

// Clamp a transfer size to what a DWORD can express. A plain cast would truncate: a
// request that is an exact multiple of 4 GiB would become 0 and masquerade as EOF.
// Clamping produces a short count, which the contract already defines as the signal
// to continue with successive calls.
static DWORD stream_dword_transfer_size(size_t bytes) {
    DWORD n = (DWORD)bytes;
    if ((size_t)n != bytes) {
        n = UINT32_MAX;
    }
    return n;
}

/**
 * @brief Open a path whose UTF-16 form exceeds MAX_PATH, in append mode.
 *
 * CreateFileW rejects unprefixed paths longer than MAX_PATH; the extended-length
 * \\?\ form lifts that limit but bypasses the Win32 path parser, so the path must be
 * absolute, backslash-separated, and free of . and .. components. GetFullPathNameW
 * supplies that normalization. A network path's leading "\\" is replaced by the
 * \\?\UNC\ prefix; a path that already carries \\?\ is passed through unchanged.
 *
 * @param path the UTF-8 path from the caller
 * @param wlen the UTF-16 length of path in WCHARs, including the terminator
 * @return an open handle, or INVALID_HANDLE_VALUE on failure
 */
static HANDLE stream_open_long_path(char const *path, int wlen) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    WCHAR *full   = NULL;
    WCHAR *ext    = NULL;

    WCHAR *wpath = FL_MALLOC((size_t)wlen * sizeof(WCHAR));
    if (wpath == NULL) {
        return INVALID_HANDLE_VALUE;
    }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, wlen);

    if (wcsncmp(wpath, L"\\\\?\\", 4) == 0) {
        // Already extended-length: the caller has taken responsibility for the form.
        handle = CreateFileW(wpath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
        goto cleanup;
    }

    DWORD full_len = GetFullPathNameW(wpath, 0, NULL, NULL);
    if (full_len == 0) {
        goto cleanup;
    }
    full = FL_MALLOC((size_t)full_len * sizeof(WCHAR));
    if (full == NULL) {
        goto cleanup;
    }
    if (GetFullPathNameW(wpath, full_len, full, NULL) == 0) {
        goto cleanup;
    }

    {
        bool         unc        = full[0] == L'\\' && full[1] == L'\\';
        WCHAR const *prefix     = unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
        WCHAR const *body       = unc ? full + 2 : full; // UNC prefix covers the "\\"
        size_t       prefix_len = wcslen(prefix);
        size_t       body_len   = wcslen(body);

        ext = FL_MALLOC((prefix_len + body_len + 1) * sizeof(WCHAR));
        if (ext == NULL) {
            goto cleanup;
        }
        memcpy(ext, prefix, prefix_len * sizeof(WCHAR));
        memcpy(ext + prefix_len, body, (body_len + 1) * sizeof(WCHAR));

        handle = CreateFileW(ext, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    }

cleanup:
    if (ext != NULL) {
        FL_FREE(ext);
    }
    if (full != NULL) {
        FL_FREE(full);
    }
    FL_FREE(wpath);
    return handle;
}

FL_STREAM_OPEN_FN(flp_stream_open) {
    // path is UTF-8 by contract. Convert to UTF-16 and open via CreateFileW so the
    // provider is correct regardless of the host's active code page or manifest.
    // MB_ERR_INVALID_CHARS: reject malformed UTF-8 instead of silently substituting,
    // so a bad path fails deterministically rather than opening the wrong file.
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (wlen == 0) {
        return NULL;
    }

    // Short paths convert on the stack and need no allocation at all; longer ones
    // take the extended-length route.
    HANDLE handle;
    WCHAR  stackbuf[MAX_PATH];
    if (wlen <= (int)FL_ARRAY_COUNT(stackbuf)) {
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, stackbuf, wlen);
        // FILE_APPEND_DATA makes every WriteFile append atomically at end of file.
        handle = CreateFileW(stackbuf, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    } else {
        handle = stream_open_long_path(path, wlen);
    }
    if (handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    return (FLFile *)handle;
}

FL_STREAM_WRITE_FN(flp_stream_write) {
    HANDLE h = is_console_handle(f) ? ((FlpConsoleHandle *)f)->handle : (HANDLE)f;

    DWORD transferred = 0;
    if (!WriteFile(h, buf, stream_dword_transfer_size(bytes), &transferred, NULL)) {
        return 0;
    }

    return (size_t)transferred;
}

FL_STREAM_CLOSE_FN(flp_stream_close) {
    if (f == NULL || is_console_handle(f)) {
        // stdout/stderr are process-wide and shared; never this service's to close.
        return;
    }
    CloseHandle((HANDLE)f);
}

FL_STREAM_CONSOLE_FN(flp_stream_console) {
    FlpConsoleHandle *slot   = (which == FL_STREAM_STDERR) ? &g_stderr_handle
                                                             : &g_stdout_handle;
    DWORD             std_id = (which == FL_STREAM_STDERR) ? STD_ERROR_HANDLE
                                                             : STD_OUTPUT_HANDLE;

    // Re-queried on every call (not cached at first use) so a caller that redirects
    // the process's std handle after this service has already been used still gets
    // the current target.
    HANDLE handle = GetStdHandle(std_id);
    if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    slot->handle = handle;
    return (FLFile *)slot;
}

static FLStreamService g_stream_service = {
    .open    = flp_stream_open,
    .write   = flp_stream_write,
    .close   = flp_stream_close,
    .console = flp_stream_console,
};

FLP_INIT_STREAM_SERVICE_FN(flp_init_stream_service) {
    FL_ASSERT_NOT_NULL(fla_set);
    fla_set(&g_stream_service, sizeof g_stream_service);
}
