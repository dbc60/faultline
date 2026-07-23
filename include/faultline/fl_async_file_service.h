#ifndef FL_ASYNC_FILE_SERVICE_H_
#define FL_ASYNC_FILE_SERVICE_H_

/**
 * @file fl_async_file_service.h
 * @author Douglas Cuthbertson
 * @brief Asynchronous file I/O service contract (op-handle / "future" model).
 * @version 0.1
 * @date 2026-06-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Completion model: submit returns an opaque in-flight operation (the portable
 * analogue of an OVERLAPPED). The caller reaps the result later with poll
 * (non-blocking) or wait (blocking). This imposes no threading model on the
 * consumer and maps onto overlapped+event, POSIX AIO, or io_uring; a provider
 * that needs to scale to many concurrent ops may back it with a completion port
 * internally without changing this contract.
 *
 * Contract notes that the synchronous service does not have to make:
 *   - Buffer lifetime: the memory referenced by submit_read/submit_write must stay
 *     valid and untouched until the op completes (poll returns true, wait returns,
 *     or cancel is acknowledged).
 *   - Explicit offset: an overlapped handle keeps no file pointer, so every submit
 *     names the byte offset to act on.
 *   - Op ownership: a returned FLFileOp is owned by the caller and must be handed
 *     to release once its result has been retrieved.
 *   - Short count: a byte count smaller than requested still signals EOF or error,
 *     and is where the platform can inject completion-time I/O faults. It is the same
 *     convention the synchronous read/write use.
 */

#include <faultline/fl_file_types.h> // FLFile, FLFileMode
#include <faultline/fl_macros.h>     // FL_STR
#include <stdbool.h>                 // bool
#include <stddef.h>                  // size_t
#include <stdint.h>                  // uint64_t

#if defined(__cplusplus)
extern "C" {
#endif

// An opaque in-flight operation, the portable analogue of OVERLAPPED. Owned by the
// caller from submit until release.
typedef struct FLFileOp FLFileOp;

// open returns NULL on failure and yields an async-capable handle (opened overlapped on
// Windows). Async I/O cannot be performed on a handle opened by the synchronous service,
// so the async service supplies its own open/close.
#define FL_FILE_OPEN_ASYNC_FN(name) FLFile *name(char const *path, FLFileMode mode)
typedef FL_FILE_OPEN_ASYNC_FN(fl_file_open_async_fn);

// submit returns NULL if the operation could not even be queued (the submit-time fault
// site). On success the transfer is in flight against the named offset.
#define FL_FILE_SUBMIT_READ_FN(name) \
    FLFileOp *name(FLFile *f, void *buf, size_t bytes, uint64_t offset)
typedef FL_FILE_SUBMIT_READ_FN(fl_file_submit_read_fn);

#define FL_FILE_SUBMIT_WRITE_FN(name) \
    FLFileOp *name(FLFile *f, void const *buf, size_t bytes, uint64_t offset)
typedef FL_FILE_SUBMIT_WRITE_FN(fl_file_submit_write_fn);

// poll is non-blocking: it returns true once the op has completed, setting *out_bytes to
// the count transferred (a short count is the EOF/error/fault signal). The op remains
// valid until release, so out_bytes stays readable after a true return.
#define FL_FILE_POLL_FN(name) bool name(FLFileOp *op, size_t *out_bytes)
typedef FL_FILE_POLL_FN(fl_file_poll_fn);

// wait blocks until the op completes and returns the count transferred.
#define FL_FILE_WAIT_FN(name) size_t name(FLFileOp *op)
typedef FL_FILE_WAIT_FN(fl_file_wait_fn);

// cancel requests cancellation of an in-flight op. The op still completes (with a short
// or zero count) and must still be released; cancel only asks.
#define FL_FILE_CANCEL_FN(name) void name(FLFileOp *op)
typedef FL_FILE_CANCEL_FN(fl_file_cancel_fn);

// release frees a completed op. The buffer it referenced may be reused afterward.
#define FL_FILE_OP_RELEASE_FN(name) void name(FLFileOp *op)
typedef FL_FILE_OP_RELEASE_FN(fl_file_op_release_fn);

#define FL_FILE_CLOSE_ASYNC_FN(name) void name(FLFile *f)
typedef FL_FILE_CLOSE_ASYNC_FN(fl_file_close_async_fn);

typedef struct FLAsyncFileService {
    fl_file_open_async_fn   *open;
    fl_file_submit_read_fn  *submit_read;
    fl_file_submit_write_fn *submit_write;
    fl_file_poll_fn         *poll;
    fl_file_wait_fn         *wait;
    fl_file_cancel_fn       *cancel;
    fl_file_op_release_fn   *release;
    fl_file_close_async_fn  *close;
} FLAsyncFileService;

// These definitions are common to both the platform and consumer implementations.
// size lets the struct grow (e.g. a future on-complete callback) without breaking ABI.
#define FLA_SET_ASYNC_FILE_SERVICE_FN(name) \
    void name(FLAsyncFileService *const svc, size_t size)
typedef FLA_SET_ASYNC_FILE_SERVICE_FN(fla_set_async_file_service_fn);
#define FLA_SET_ASYNC_FILE_SERVICE_STR FL_STR(fla_set_async_file_service)

#if defined(__cplusplus)
}
#endif

#endif // FL_ASYNC_FILE_SERVICE_H_
