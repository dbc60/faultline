#ifndef FL_FILE_SERVICE_H_
#define FL_FILE_SERVICE_H_

/**
 * @file fl_file_service.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-06-23
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_file_types.h> // FLFile, FLFileMode
#include <faultline/fl_macros.h>     // FL_STR
#include <stddef.h>                  // size_t
#include <stdint.h>                  // uint64_t

#if defined(__cplusplus)
extern "C" {
#endif

// path is UTF-8 on every platform: POSIX treats it as opaque bytes, and the Windows
// provider converts to UTF-16 internally, so a caller passes the same encoding
// everywhere and never has to match a host code page. Paths longer than the
// platform's conventional limit are accepted; the Windows provider opens them in
// extended-length (\\?\) form.
//
// open returns NULL on failure. read/write are positional: each names the byte
// offset to act on and keeps no implicit file pointer, so a handle is fully
// described by its arguments and safe to use from several threads at once. They
// return bytes transferred; a short count signals EOF or error (and is where the
// platform can inject I/O faults). Sequential streaming is a thin cursor over this
// primitive, built by the consumer rather than carried in the contract.
//
// Exception: a handle opened with FL_FILE_APPEND ignores the write offset and
// appends every write atomically at end of file (Windows FILE_APPEND_DATA
// semantics), so several writers can interleave whole records. Positional writes
// require an FL_FILE_WRITE handle.
#define FL_FILE_OPEN_FN(name) FLFile *name(char const *path, FLFileMode mode)
typedef FL_FILE_OPEN_FN(fl_file_open_fn);

#define FL_FILE_READ_FN(name) size_t name(FLFile *f, void *buf, size_t bytes, uint64_t offset)
typedef FL_FILE_READ_FN(fl_file_read_fn);

#define FL_FILE_WRITE_FN(name) size_t name(FLFile *f, void const *buf, size_t bytes, uint64_t offset)
typedef FL_FILE_WRITE_FN(fl_file_write_fn);

#define FL_FILE_CLOSE_FN(name) void name(FLFile *f)
typedef FL_FILE_CLOSE_FN(fl_file_close_fn);

typedef struct FLFileService {
    fl_file_open_fn  *open;
    fl_file_read_fn  *read;
    fl_file_write_fn *write;
    fl_file_close_fn *close;
} FLFileService;

// These definitions are common to both the platform and consumer implementations
#define FLA_SET_FILE_SERVICE_FN(name) void name(FLFileService *const svc, size_t size)
typedef FLA_SET_FILE_SERVICE_FN(fla_set_file_service_fn);
#define FLA_SET_FILE_SERVICE_STR FL_STR(fla_set_file_service)

#if defined(__cplusplus)
}
#endif

#endif // FL_FILE_SERVICE_H_
