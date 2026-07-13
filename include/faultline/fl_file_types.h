#ifndef FL_FILE_TYPES_H_
#define FL_FILE_TYPES_H_

/**
 * @file fl_file_types.h
 * @author Douglas Cuthbertson
 * @brief Vocabulary shared by the synchronous and asynchronous file services.
 * @version 0.1
 * @date 2026-06-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This header carries only the types both file services agree on. The contracts
 * themselves live in fl_file_service.h (synchronous) and fl_async_file_service.h
 * (asynchronous); each is provided, injected, and consumed independently.
 */

#if defined(__cplusplus)
extern "C" {
#endif

// An opaque handle provided by the platform and used by the core implementation. A given
// instance belongs to whichever service opened it: a handle opened by the synchronous
// service is not interchangeable with one opened by the asynchronous service (on Windows
// the async-capability is fixed at open by FILE_FLAG_OVERLAPPED).
typedef struct FLFile FLFile;

typedef enum {
    FL_FILE_READ,   // open existing for reading
    FL_FILE_WRITE,  // create/truncate for writing
    FL_FILE_APPEND, // create-or-append; every write lands at end of file
} FLFileMode;

#if defined(__cplusplus)
}
#endif

#endif // FL_FILE_TYPES_H_
