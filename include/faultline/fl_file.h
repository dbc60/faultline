#ifndef FL_FILE_H_
#define FL_FILE_H_

/**
 * @file fl_file.h
 * @brief Unified FL_FILE_* macros that work in both platform and consumer code.
 *
 * This header is the single definition site for the FL_FILE_OPEN/READ/WRITE/CLOSE
 * family. It selects the file-service backend -- platform (flp_file_*) or consumer
 * (g_fla_file_service.*) -- by whether FL_PLATFORM_BUILD is defined, then defines the
 * call set once over that backend. A translation unit gets exactly one backend, and the
 * call-site names never carry a platform/consumer prefix: the side is fixed per TU by
 * the build, not per call.
 *
 * read/write are positional -- each names the byte offset to act on -- so these macros
 * mirror the FLFileService signatures exactly. Append-only writes and console output are
 * provided by a separate stream service (fl_stream.h), not this one.
 */
#include <faultline/fl_file_service.h> // FLFile, FLFileMode

#if defined(FL_PLATFORM_BUILD)
#include <flp_file_service.h> // IWYU pragma: export -- flp_file_*
#define FL_FILE_OPEN(path, mode)            flp_file_open((path), (mode))
#define FL_FILE_READ(f, buf, bytes, offset) flp_file_read((f), (buf), (bytes), (offset))
#define FL_FILE_WRITE(f, buf, bytes, offset) \
    flp_file_write((f), (buf), (bytes), (offset))
#define FL_FILE_CLOSE(f) flp_file_close((f))
#else                                   // consumer / DLL build
#include <faultline/fla_file_service.h> // IWYU pragma: export -- g_fla_file_service
#define FL_FILE_OPEN(path, mode) g_fla_file_service.open((path), (mode))
#define FL_FILE_READ(f, buf, bytes, offset) \
    g_fla_file_service.read((f), (buf), (bytes), (offset))
#define FL_FILE_WRITE(f, buf, bytes, offset) \
    g_fla_file_service.write((f), (buf), (bytes), (offset))
#define FL_FILE_CLOSE(f) g_fla_file_service.close((f))
#endif // FL_PLATFORM_BUILD

#endif // FL_FILE_H_
