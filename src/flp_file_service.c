/**
 * @file flp_file_service.c
 * @author Douglas Cuthbertson
 * @brief Platform side of the file service implementation (Win32).
 * @version 0.1
 * @date 2026-06-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_file_service.h>     // FL_FILE_*_FN, FLFileService
#include <faultline/fl_macros.h>           // FL_ARRAY_COUNT
#include <faultline/fl_memory.h>           // FL_MALLOC, FL_FREE
#include <faultline/fl_try.h>              // FL_ASSERT_REASON_IMPL indirectly
#include <faultline/fl_exception_assert.h> // FL_ASSERT_NOT_NULL
#include <flp_file_service.h>              // FLP_INIT_FILE_SERVICE_FN
#include <stdbool.h>                       // bool
#include <stddef.h>                        // NULL, size_t
#include <stdint.h>                        // uint64_t, UINT32_MAX
#include <string.h>                        // memcpy
#include <wchar.h>                         // wcslen, wcsncmp

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // CreateFileW, MultiByteToWideChar, ReadFile, WriteFile, OVERLAPPED
#endif

// The opaque FLFile is the Win32 HANDLE itself: a live handle is never NULL, so a NULL
// return cleanly signals open failure without a separate wrapper allocation.

static OVERLAPPED overlapped_at(uint64_t offset) {
    OVERLAPPED ov = {0};
    ov.Offset     = (DWORD)(offset & 0xFFFFFFFFu);
    ov.OffsetHigh = (DWORD)(offset >> 32);
    return ov;
}

// Clamp a transfer size to what a DWORD can express. A plain cast would truncate: a
// request that is an exact multiple of 4 GiB would become 0 and masquerade as EOF.
// Clamping produces a short count, which the contract already defines as the signal
// to continue with successive positional calls.
static DWORD dword_transfer_size(size_t bytes) {
    DWORD n = (DWORD)bytes;
    if ((size_t)n != bytes) {
        n = UINT32_MAX;
    }
    return n;
}

/**
 * @brief Open a path whose UTF-16 form exceeds MAX_PATH.
 *
 * CreateFileW rejects unprefixed paths longer than MAX_PATH; the extended-length
 * \\?\ form lifts that limit but bypasses the Win32 path parser, so the path must be
 * absolute, backslash-separated, and free of . and .. components. GetFullPathNameW
 * supplies that normalization. A network path's leading "\\" is replaced by the
 * \\?\UNC\ prefix; a path that already carries \\?\ is passed through unchanged.
 *
 * @param path the UTF-8 path from the caller
 * @param wlen the UTF-16 length of path in WCHARs, including the terminator
 * @param access the desired-access flags for CreateFileW
 * @param creation the creation-disposition flags for CreateFileW
 * @return an open handle, or INVALID_HANDLE_VALUE on failure
 */
static HANDLE open_long_path(char const *path, int wlen, DWORD access, DWORD creation) {
    HANDLE handle   = INVALID_HANDLE_VALUE;
    WCHAR *full     = NULL;
    WCHAR *ext      = NULL;
    DWORD  full_len = 0;

    WCHAR *wpath = (WCHAR *)FL_MALLOC((size_t)wlen * sizeof(WCHAR));
    if (wpath == NULL) {
        return INVALID_HANDLE_VALUE;
    }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, wlen);

    if (wcsncmp(wpath, L"\\\\?\\", 4) == 0) {
        // Already extended-length: the caller has taken responsibility for the form.
        handle = CreateFileW(wpath, access, FILE_SHARE_READ, NULL, creation,
                             FILE_ATTRIBUTE_NORMAL, NULL);
        goto cleanup;
    }

    full_len = GetFullPathNameW(wpath, 0, NULL, NULL);
    if (full_len == 0) {
        goto cleanup;
    }
    full = (WCHAR *)FL_MALLOC((size_t)full_len * sizeof(WCHAR));
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

        ext = (WCHAR *)FL_MALLOC((prefix_len + body_len + 1) * sizeof(WCHAR));
        if (ext == NULL) {
            goto cleanup;
        }
        memcpy(ext, prefix, prefix_len * sizeof(WCHAR));
        memcpy(ext + prefix_len, body, (body_len + 1) * sizeof(WCHAR));

        handle = CreateFileW(ext, access, FILE_SHARE_READ, NULL, creation,
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

FL_FILE_OPEN_FN(flp_file_open) {
    DWORD access;
    DWORD creation;
    switch (mode) {
    case FL_FILE_READ:
        access   = GENERIC_READ;
        creation = OPEN_EXISTING;
        break;
    case FL_FILE_WRITE:
        access   = GENERIC_WRITE;
        creation = CREATE_ALWAYS;
        break;
    default:
        return NULL;
    }

    // path is UTF-8 by contract. Convert to UTF-16 and open via CreateFileW so the
    // provider is correct regardless of the host's active code page or manifest.
    // MB_ERR_INVALID_CHARS: reject malformed UTF-8 instead of silently substituting,
    // so a bad path fails deterministically rather than opening the wrong file.
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (wlen == 0) {
        return NULL;
    }

    // Short paths convert on the stack; longer ones take the extended-length route.
    HANDLE handle;
    WCHAR  stackbuf[MAX_PATH];
    if (wlen <= (int)FL_ARRAY_COUNT(stackbuf)) {
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, stackbuf, wlen);
        handle = CreateFileW(stackbuf, access, FILE_SHARE_READ, NULL, creation,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    } else {
        handle = open_long_path(path, wlen, access, creation);
    }
    if (handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    return (FLFile *)handle;
}

// Passing an OVERLAPPED to ReadFile/WriteFile on a synchronously-opened handle names the
// start offset; the call still completes synchronously. A short count (including 0 at
// EOF) is the EOF/error/fault signal. bytes is clamped to a single DWORD, so a caller
// wanting >4 GiB issues successive positional calls.
FL_FILE_READ_FN(flp_file_read) {
    OVERLAPPED ov          = overlapped_at(offset);
    DWORD      transferred = 0;
    if (!ReadFile((HANDLE)f, buf, dword_transfer_size(bytes), &transferred, &ov)) {
        return 0;
    }

    return (size_t)transferred;
}

FL_FILE_WRITE_FN(flp_file_write) {
    OVERLAPPED ov          = overlapped_at(offset);
    DWORD      transferred = 0;
    if (!WriteFile((HANDLE)f, buf, dword_transfer_size(bytes), &transferred, &ov)) {
        return 0;
    }

    return (size_t)transferred;
}

FL_FILE_CLOSE_FN(flp_file_close) {
    if (f != NULL) {
        CloseHandle((HANDLE)f);
    }
}

static FLFileService g_file_service = {
    .open  = flp_file_open,
    .read  = flp_file_read,
    .write = flp_file_write,
    .close = flp_file_close,
};

FLP_INIT_FILE_SERVICE_FN(flp_init_file_service) {
    FL_ASSERT_NOT_NULL(fla_set);
    fla_set(&g_file_service, sizeof g_file_service);
}
