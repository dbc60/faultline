#ifndef FL_MACROS_H_
#define FL_MACROS_H_

/**
 * @file fl_macros.h
 * @author Douglas Cuthbertson
 * @brief Project-wide macro definitions.
 * @version 0.1
 * @date 2022-01-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <stddef.h> // offsetof

#if !defined(FL_THREAD_LOCAL)
// Note that for Visual Studio 2022 to define __STDC_VERSION__, the compiler
// option "/std:cxx" must be applied where "cxx" is one of "c11" or "c17". Without
// this option, __STDC_VERSION__ is undefined.
//
// Hopefully one day there will be a C23-compliant version of VS, because that
// will mean that `thread_local` will be a standard keyword.

// FL_THREAD_LOCAL can be applied to objects that have static storage duration,
// that is any object that is valid during the entire execution of the program,
// such as global data objects (both static and extern), local static objects,
// and static data members of classes. It specifically excludes objects on the
// call stack.
//
// The "thread_local" keyword was added to C as of C23. It was added to C++ in
// C++11. In the meantime, C has had "_Thread_local" since C11.
#if defined(__cplusplus)
#define FL_THREAD_LOCAL thread_local
#else // !__cplusplus
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define FL_THREAD_LOCAL thread_local
#else // !(__STDC_VERSION__ && __STDC_VERSION__ >= 202311L)
#if defined(__clang__) && defined(_WIN32) && !defined(_MSC_VER)
// clang+MinGW: native TLS is broken without compiler-rt; emulated TLS
// (-femulated-tls) requires libgcc_eh's pthreads-backed emutls, which is
// unavailable here. Disable until compiler-rt is built for this target.
// FIXME: re-enable once compiler-rt builtins are installed at
//   lib/clang/<ver>/lib/windows/libclang_rt.builtins-x86_64.a
#define FL_THREAD_LOCAL
#elif defined(__clang__) || defined(__GNUC__)
#define FL_THREAD_LOCAL _Thread_local
#elif defined(_WIN32) || defined(WIN32)
// MSVC does not support _Thread_local; use its proprietary spelling instead.
#define FL_THREAD_LOCAL __declspec(thread)
#else
#define FL_THREAD_LOCAL _Thread_local
#endif // __clang__ || __GNUC__
#endif // __STDC_VERSION__ && __STDC_VERSION__ > 201710L
#endif // __cplusplus
#endif // FL_THREAD_LOCAL

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Stringize a value.
 *
 * Usage:
 *  #pragma message("DBG: Compiling " __FILE__)
 *  #pragma message ("DBG: " __FILE__ "(" FL_STR(__LINE__) "): test")
 *  #pragma message("DBG: FL_DECL_SPEC " FL_STR(FL_DECL_SPEC))
 *
 * Example Output:
 *  DBG: Compiling W:\faultline\but\driver.c
 *  DBG: W:\faultline\but\driver.c(23): test
 *  DBG: FL_DECL_SPEC __declspec(dllexport)
 */
#define FL_XSTR(x) #x
#define FL_STR(x)  FL_XSTR(x)

/**
 * @brief Explicitly note the intent of not using a function parameter.
 *
 * It can eliminate compiler warnings about unused parameters.
 */
#define FL_UNUSED(P) ((void)(P))

/**
 * @brief Mark a function as intentionally possibly unused.
 *
 * Apply to static inline functions that are part of a generated API (e.g. via
 * DEFINE_TYPED_SET) where not every caller will use every generated function.
 * Suppresses -Wunused-function on GCC/Clang; expands to nothing on MSVC, which
 * does not warn about unused static inline functions.
 */
#if defined(__GNUC__) || defined(__clang__)
#define FL_MAYBE_UNUSED __attribute__((unused))
#else
#define FL_MAYBE_UNUSED
#endif

/// The number of elements in a fixed-size array
#define FL_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/**
 * @brief Calculate the address of a struct given the address of a field in the
 * struct, the struct's type, and the name of the field to which the address
 * points. This is equivalent of Microsoft's CONTAINING_RECORD and Linux's
 * container_of macros.
 */
#define FL_CONTAINER_OF(addr, type, member) \
    ((type *)(((char *)(addr)) - offsetof(type, member)))

/**
 * @brief Given a type and a member, FL_MEMBER_SIZE calculates the size of the member in
 * bytes at compile time.
 */
#define FL_MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

#if defined(_WIN32) || defined(WIN32)
#define FL_SPEC_EXPORT __declspec(dllexport)
#define FL_SPEC_IMPORT __declspec(dllimport)
#if defined(DLL_BUILD)
#define FL_DECL_SPEC __declspec(dllexport)
#elif defined(FL_EMBEDDED) // the function is built-in to an executable
#define FL_DECL_SPEC
#else // import from DLL
#define FL_DECL_SPEC __declspec(dllimport)
#endif // DLL_BUILD
#if !defined STDCALL
#define STDCALL __stdcall
#endif // STDCALL
#if !defined CDECL
#define CDECL __cdecl
#endif // CDECL
#else  // _WIN32 || WIN32
#define FL_SPEC_EXPORT
#define FL_SPEC_IMPORT
#define FL_DECL_SPEC
#define STDCALL
#define CDECL
#endif

#if defined(__cplusplus)
}
#endif

#endif // FL_MACROS_H_
