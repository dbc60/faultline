#ifndef FL_ABI_H_
#define FL_ABI_H_

/**
 * @file fl_abi.h
 * @author Douglas Cuthbertson
 * @brief Build identity a loaded module reports to its host.
 * @version 0.1
 * @date 2026-08-15
 *
 * A host loads modules and hands them services through function pointers. Some of what
 * crosses that boundary is not described by any of those service contracts: the host
 * walks an FLTestSuite the module owns and reads back an FLCaseOutcome the module fills
 * in, each side lays out the service structs themselves, and both must agree on the
 * types the C11 threads API uses. Those agreements are implicit in the toolchain, so two
 * images built by different toolchains can satisfy every service contract exactly and
 * still corrupt each other.
 *
 * FLAbiInfo states those agreements outright. Each image fills in an FLAbiInfo
 * describing itself, the host compares the module's against its own, and a mismatch
 * becomes a message naming the difference instead of an access violation with no
 * context.
 *
 * fl_fill_abi_info must remain a static inline in this header, because each image has to
 * compile its own copy of the function and report the values its own compiler saw.
 * Moving fl_fill_abi_info into a library that the host and its modules both link would
 * give them one shared copy reporting one set of values, so every comparison would match
 * and the check would pass everything.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_macros.h>          // FL_STR
#include <faultline/fl_threads.h>         // mtx_t, thrd_t, FL_THREADS_USE_SHIM
#include <faultline/fl_exception_types.h> // FLExceptionEnvironment
#include <setjmp.h>                       // jmp_buf
#include <stdint.h>                       // uint16_t, uint32_t

#if defined(__cplusplus)
extern "C" {
#endif

/** Increment for breaking changes. This allows FL_VERSION_MAJOR (below) to change for
 * non-breaking changes, like marketing new features or transitioning from development
 * (e.g., 0.MINOR.PATCH) to a stable release (e.g., 1.0.0). without tying compatibility
 * to the release. */
#define FL_COMPATIBILITY_VERSION 2

/** Project version, reported by the version command and carried in FLAbiInfo. */
#define FL_VERSION_MAJOR 0
#define FL_VERSION_MINOR 5
#define FL_VERSION_PATCH 0

/** Identifies the struct below as this contract's, and its revision. "FLA1" in ASCII hex
 * backwards so it reads in memory as FLA1 on little-endian platforms. */
#define FL_ABI_MAGIC 0x31414C46u

// -- Toolchain identity ----------------------------------------------------------------
// The C Runtime ID (crt_id) is used to decide compatibility between executables and the
// modules they load. The C runtime owns setjmp/longjmp and the heap, so two images must
// share the same one. compiler_id and compiler_version are report and logging only.
// Interestingly, clang-cl on Windows and MSVC produce different compiler IDs yet share a
// CRT, and are compatible.

#define FL_ABI_COMPILER_UNKNOWN 0u
#define FL_ABI_COMPILER_MSVC    1u
#define FL_ABI_COMPILER_CLANG   2u
#define FL_ABI_COMPILER_GCC     3u

#define FL_ABI_CRT_UNKNOWN 0u
#define FL_ABI_CRT_MSVC    1u
#define FL_ABI_CRT_MINGW   2u
#define FL_ABI_CRT_POSIX   3u

// The exception backend (setjmp/longjmp vs. a real C++ throw, FL_EXC_BACKEND_CXX)
// changes what FLExceptionEnvironment/FLExceptionFrame mean at the ABI level even when
// their sizes happen to collide, so it is reported and checked alongside them rather
// than folded into sizeof_exception_env.

#define FL_ABI_BACKEND_SETJMP 0u
#define FL_ABI_BACKEND_CXX    1u

#if defined(__clang__)
#define FL_ABI_COMPILER_ID  FL_ABI_COMPILER_CLANG
#define FL_ABI_COMPILER_VER ((uint32_t)(__clang_major__ * 100 + __clang_minor__))
#elif defined(_MSC_VER)
#define FL_ABI_COMPILER_ID  FL_ABI_COMPILER_MSVC
#define FL_ABI_COMPILER_VER ((uint32_t)_MSC_VER)
#elif defined(__GNUC__)
#define FL_ABI_COMPILER_ID  FL_ABI_COMPILER_GCC
#define FL_ABI_COMPILER_VER ((uint32_t)(__GNUC__ * 100 + __GNUC_MINOR__))
#else
#define FL_ABI_COMPILER_ID  FL_ABI_COMPILER_UNKNOWN
#define FL_ABI_COMPILER_VER 0u
#endif

// Tested before _MSC_VER: clang targeting mingw defines __MINGW64__ and must be
// grouped with the MinGW runtime, not with the MSVC one.
#if defined(__MINGW32__) || defined(__MINGW64__)
#define FL_ABI_CRT_ID FL_ABI_CRT_MINGW
#elif defined(_MSC_VER)
#define FL_ABI_CRT_ID FL_ABI_CRT_MSVC
#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)
#define FL_ABI_CRT_ID FL_ABI_CRT_POSIX
#else
#define FL_ABI_CRT_ID FL_ABI_CRT_UNKNOWN
#endif

/**
 * @brief One image's build identity.
 *
 * struct_size lets this struct grow. It records how many bytes the image that filled the
 * struct knew about, so an image compiled against a longer FLAbiInfo can tell which of
 * its own trailing fields the writer never wrote. Append new fields at the end,
 * otherwise two different versions will disagree on the offset of the current fields
 * after a new inserted field.
 */
typedef struct FLAbiInfo {
    uint32_t magic;                 ///< FL_ABI_MAGIC
    uint32_t struct_size;           ///< sizeof(FLAbiInfo) as the writer saw it
    uint32_t compiler_id;           ///< FL_ABI_COMPILER_*
    uint32_t compiler_version;      ///< scale differs per compiler; diagnostic only
    uint32_t crt_id;                ///< FL_ABI_CRT_*; the CRT compatibility identifier
    uint16_t compatibility_version; ///< FL_COMPATIBILITY_VERSION
    uint16_t version_major;         ///< FL_VERSION_MAJOR
    uint16_t version_minor;         ///< FL_VERSION_MINOR
    uint16_t version_patch;         ///< FL_VERSION_PATCH
    uint16_t threads_use_shim;      ///< FL_THREADS_USE_SHIM
    uint16_t sizeof_mtx_t;          ///< differs with the threads selection
    uint16_t sizeof_thrd_t;         ///< differs with the threads selection
    uint16_t sizeof_jmp_buf;        ///< equal size does not imply interchangeable
    uint16_t sizeof_exception_env;  ///< compare sizes between the executable and module
    uint16_t backend;               ///< FL_ABI_BACKEND_*; zero on an image built before
                                     ///< this field existed, which is FL_ABI_BACKEND_SETJMP
} FLAbiInfo;

/*
 * Why two images cannot be used together.
 */
typedef enum FLAbiVerdict {
    FL_ABI_OK = 0,
    FL_ABI_BAD_MAGIC,
    FL_ABI_CRT_MISMATCH,
    FL_ABI_THREADS_MISMATCH,
    FL_ABI_LAYOUT_MISMATCH,
    FL_ABI_VERSION_MISMATCH,
} FLAbiVerdict;

/**
 * @brief Fill in the calling image's own identity.
 *
 * Must stay a static inline in this header. See the file comment.
 */
static inline void fl_fill_abi_info(FLAbiInfo *out) {
    out->magic                 = FL_ABI_MAGIC;
    out->struct_size           = (uint32_t)sizeof *out;
    out->compiler_id           = FL_ABI_COMPILER_ID;
    out->compiler_version      = FL_ABI_COMPILER_VER;
    out->crt_id                = FL_ABI_CRT_ID;
    out->compatibility_version = FL_COMPATIBILITY_VERSION;
    out->version_major         = (uint16_t)FL_VERSION_MAJOR;
    out->version_minor         = (uint16_t)FL_VERSION_MINOR;
    out->version_patch         = (uint16_t)FL_VERSION_PATCH;
    out->threads_use_shim      = (uint16_t)FL_THREADS_USE_SHIM;
    out->sizeof_mtx_t          = (uint16_t)sizeof(mtx_t);
    out->sizeof_thrd_t         = (uint16_t)sizeof(thrd_t);
    out->sizeof_jmp_buf        = (uint16_t)sizeof(jmp_buf);
    out->sizeof_exception_env  = (uint16_t)sizeof(FLExceptionEnvironment);
#if defined(FL_EXC_BACKEND_CXX)
    out->backend = FL_ABI_BACKEND_CXX;
#else
    out->backend = FL_ABI_BACKEND_SETJMP;
#endif
}

/**
 * @brief Decide whether a loaded module can be used by this host.
 *
 * The tests run most-fundamental first, so that the verdict names the root difference
 * rather than one of its consequences. Two images built against different C runtimes
 * have usually also selected different C11 threads implementations, and both differences
 * would be reported; naming the runtime is the one that tells a caller which image to
 * rebuild.
 */
static inline FLAbiVerdict fl_abi_check(FLAbiInfo const *host, FLAbiInfo const *mod) {
    FLAbiVerdict verdict = FL_ABI_OK;

    if (mod->magic != FL_ABI_MAGIC) {
        verdict = FL_ABI_BAD_MAGIC;
    } else if (mod->crt_id != host->crt_id) {
        verdict = FL_ABI_CRT_MISMATCH;
    } else if (mod->threads_use_shim != host->threads_use_shim
               || mod->sizeof_mtx_t != host->sizeof_mtx_t
               || mod->sizeof_thrd_t != host->sizeof_thrd_t) {
        verdict = FL_ABI_THREADS_MISMATCH;
    } else if (mod->sizeof_jmp_buf != host->sizeof_jmp_buf
               || mod->sizeof_exception_env != host->sizeof_exception_env
               || mod->backend != host->backend) {
        verdict = FL_ABI_LAYOUT_MISMATCH;
    } else if (mod->compatibility_version != host->compatibility_version) {
        verdict = FL_ABI_VERSION_MISMATCH;
    }
    return verdict;
}

static inline char const *fl_abi_verdict_str(FLAbiVerdict verdict) {
    char const *text;
    switch (verdict) {
    case FL_ABI_OK:
        text = "compatible";
        break;
    case FL_ABI_BAD_MAGIC:
        text = "not a FaultLine module descriptor";
        break;
    case FL_ABI_CRT_MISMATCH:
        text = "built against a different C runtime";
        break;
    case FL_ABI_THREADS_MISMATCH:
        text = "built against a different C11 threads implementation";
        break;
    case FL_ABI_LAYOUT_MISMATCH:
        text = "exception environment layout differs";
        break;
    case FL_ABI_VERSION_MISMATCH:
        text = "built against an incompatible version";
        break;
    default:
        text = "unrecognized verdict";
        break;
    }
    return text;
}

static inline char const *fl_abi_compiler_str(uint32_t compiler_id) {
    char const *text;
    switch (compiler_id) {
    case FL_ABI_COMPILER_MSVC:
        text = "MSVC";
        break;
    case FL_ABI_COMPILER_CLANG:
        text = "clang";
        break;
    case FL_ABI_COMPILER_GCC:
        text = "gcc";
        break;
    default:
        text = "unknown compiler";
        break;
    }
    return text;
}

static inline char const *fl_abi_crt_str(uint32_t crt_id) {
    char const *text;
    switch (crt_id) {
    case FL_ABI_CRT_MSVC:
        text = "MSVC CRT";
        break;
    case FL_ABI_CRT_MINGW:
        text = "MinGW runtime";
        break;
    case FL_ABI_CRT_POSIX:
        text = "POSIX libc";
        break;
    default:
        text = "unknown runtime";
        break;
    }
    return text;
}

static inline char const *fl_abi_backend_str(uint32_t backend) {
    char const *text;
    switch (backend) {
    case FL_ABI_BACKEND_SETJMP:
        text = "setjmp/longjmp";
        break;
    case FL_ABI_BACKEND_CXX:
        text = "C++ throw";
        break;
    default:
        text = "unknown backend";
        break;
    }
    return text;
}

/**
 * @brief The symbol a loadable module exports so a host can read its identity.
 *
 * FL_GET_TEST_SUITE emits the definition, so a module gains it with no edit.
 *
 * The prototype below is what makes that definition safe to find by name under
 * FL_EXC_BACKEND_CXX: fla_get_abi has no other declaration anywhere, so without
 * one visible before FL_GET_TEST_SUITE's definition, a C++ compile gives it
 * mangled linkage and GetProcAddress(FLA_GET_ABI_STR) stops finding it. A prior
 * declaration inside this header's extern "C" block fixes that -- linkage comes
 * from the first declaration a translation unit sees, and the definition then
 * matches it even though FL_GET_TEST_SUITE never says extern "C" itself.
 */
#define FLA_GET_ABI_FN(name) void name(FLAbiInfo *out)
typedef FLA_GET_ABI_FN(fla_get_abi_fn);
extern FL_SPEC_EXPORT fla_get_abi_fn fla_get_abi;
#define FLA_GET_ABI_STR FL_STR(fla_get_abi)

#if defined(__cplusplus)
}
#endif

#endif // FL_ABI_H_
