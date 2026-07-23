#ifndef FL_STREAM_H_
#define FL_STREAM_H_

/**
 * @file fl_stream.h
 * @brief Unified FL_STREAM_* macros that work in both platform and consumer code.
 *
 * This header is the single definition site for the FL_STREAM_OPEN/WRITE/CLOSE/CONSOLE
 * family. It selects the stream-service backend -- platform (flp_stream_*) or consumer
 * (g_fla_stream_service.*) -- by whether FL_PLATFORM_BUILD is defined, then defines the
 * call set once over that backend. A translation unit gets exactly one backend, and the
 * call-site names never carry a platform/consumer prefix: the side is fixed per TU by
 * the build, not per call.
 *
 * write takes no offset -- neither an append-opened handle nor a console handle has an
 * addressable position (see fl_stream_service.h).
 */
#include <faultline/fl_stream_service.h> // FLFile, FLConsoleStream

#if defined(FL_PLATFORM_BUILD)
#include <flp_stream_service.h> // IWYU pragma: export -- flp_stream_*
#define FL_STREAM_OPEN(path)           flp_stream_open((path))
#define FL_STREAM_WRITE(f, buf, bytes) flp_stream_write((f), (buf), (bytes))
#define FL_STREAM_CLOSE(f)             flp_stream_close((f))
#define FL_STREAM_CONSOLE(which)       flp_stream_console((which))
#else                                    // consumer / DLL build
#include <faultline/fla_stream_service.h> // IWYU pragma: export -- g_fla_stream_service
#define FL_STREAM_OPEN(path)           g_fla_stream_service.open((path))
#define FL_STREAM_WRITE(f, buf, bytes) g_fla_stream_service.write((f), (buf), (bytes))
#define FL_STREAM_CLOSE(f)             g_fla_stream_service.close((f))
#define FL_STREAM_CONSOLE(which)       g_fla_stream_service.console((which))
#endif // FL_PLATFORM_BUILD

#endif // FL_STREAM_H_
