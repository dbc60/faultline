#ifndef FL_STREAM_SERVICE_H_
#define FL_STREAM_SERVICE_H_

/**
 * @file fl_stream_service.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-07-22
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_file_types.h> // FLFile (opaque; reused for stream handles)
#include <faultline/fl_macros.h>     // FL_STR
#include <stddef.h>                  // size_t

#if defined(__cplusplus)
extern "C" {
#endif

// This service covers sequential, non-addressable I/O: appending to a file by path,
// and writing to the process's inherited console streams. Neither target has a
// meaningful byte offset, so unlike the synchronous file service (fl_file_service.h)
// write takes no offset parameter at all.
//
// Handles returned by this service reuse the FLFile type from fl_file_types.h but are
// not interchangeable with handles opened by the synchronous or asynchronous file
// services (see fl_file_types.h).

// Which of the process's inherited console handles console() returns.
typedef enum {
    FL_STREAM_STDOUT,
    FL_STREAM_STDERR,
} FLConsoleStream;

// open returns NULL on failure. Every write through the returned handle is an atomic
// append (Windows FILE_APPEND_DATA semantics), so several writers, including other
// processes appending to the same path, interleave whole records without the caller
// coordinating anything.
#define FL_STREAM_OPEN_FN(name) FLFile *name(char const *path)
typedef FL_STREAM_OPEN_FN(fl_stream_open_fn);

// write returns the count actually transferred; a short count signals an error (and
// is where the platform can inject I/O faults).
#define FL_STREAM_WRITE_FN(name) size_t name(FLFile *f, void const *buf, size_t bytes)
typedef FL_STREAM_WRITE_FN(fl_stream_write_fn);

// close releases a handle obtained from open(). It is a documented no-op on a handle
// obtained from console(): stdout/stderr are process-wide and shared, never this
// service's to close. Either way, treat the FLFile* as invalidated by close() -- call
// open()/console() again for a fresh handle rather than reusing one after closing it.
#define FL_STREAM_CLOSE_FN(name) void name(FLFile *f)
typedef FL_STREAM_CLOSE_FN(fl_stream_close_fn);

// console returns a non-owning handle onto the process's stdout or stderr, or NULL if
// the OS has none. Safe to pass to write(); passing it to close() is a no-op.
#define FL_STREAM_CONSOLE_FN(name) FLFile *name(FLConsoleStream which)
typedef FL_STREAM_CONSOLE_FN(fl_stream_console_fn);

typedef struct FLStreamService {
    fl_stream_open_fn    *open;
    fl_stream_write_fn   *write;
    fl_stream_close_fn   *close;
    fl_stream_console_fn *console;
} FLStreamService;

// These definitions are common to both the platform and consumer implementations
#define FLA_SET_STREAM_SERVICE_FN(name) \
    void name(FLStreamService *const svc, size_t size)
typedef FLA_SET_STREAM_SERVICE_FN(fla_set_stream_service_fn);
#define FLA_SET_STREAM_SERVICE_STR FL_STR(fla_set_stream_service)

#if defined(__cplusplus)
}
#endif

#endif // FL_STREAM_SERVICE_H_
