#ifndef FL_EXCEPTION_TYPES_H_
#define FL_EXCEPTION_TYPES_H_

/**
 * @file fl_exception_types.h
 * @author Douglas Cuthbertson
 * @brief Type definitions for an exception handling library.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // u32
#include <setjmp.h>                         // jmp_buf

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef FL_MAX_DETAILS_LENGTH
///< the maximum length of details in an exception
#define FL_MAX_DETAILS_LENGTH 512
#endif

// The four states within an FL_TRY block used to track whether a try-block
// was entered, an exception was thrown and not handled, caught and handled,
// and whether a finally block was entered.
typedef enum {
    FL_ENTERED, // try block entered; setjmp has returned zero
    FL_THROWN,  // exception thrown
    FL_HANDLED, // exception handled/caught or finally block entered w/o throwing an
                // exception
} FLExceptionState;

/**
 * @brief The type of an exception. It is a constant string representing the reason the
 * exception was thrown (e.g., "out of memory", "invalid argument", or "just cause").
 */
typedef char const *FLExceptionReason;

typedef struct FLExceptionEnvironment {
    jmp_buf jmp; ///< the jump buffer for setjmp/longjmp. Note that the offset of jmp_buf
                 ///< must be 16-byte aligned on some systems.
    struct FLExceptionEnvironment
        *next; ///< a pointer to a parent context from an enclosing FL_TRY block.

    // reason, details, file, and line are written by flp_throw() after setjmp() saves
    // state and before longjmp() fires. Per C11 §7.13.2.1 ¶3, automatic-duration objects
    // modified between setjmp and longjmp have indeterminate value after longjmp unless
    // they are volatile-qualified. Without volatile, an optimizing compiler may hold
    // these fields in registers (restoring stale values on longjmp) instead of reloading
    // from memory.
    FLExceptionReason volatile reason; ///< a constant string describing the reason for
                                       ///< the exception.
    char const *volatile details;      ///< extra details about the exception
    char const *volatile file;         ///< the file where the exception was thrown.
    u32 volatile line;                 ///< the line where the exception was thrown.
    FLExceptionState volatile state;   ///< a try block is entered, thrown, handled, or
                                       ///< finalized.
} FLExceptionEnvironment;

/**
 * @brief a pointer to an exception handling function.
 *
 * @param ctx the address of an implementation-defined exception context.
 * @param reason address of a string - it's the thrown exception.
 * @param details a possibly NULL address of a string with additional details about the
 * thrown exception.
 * @param file address of a string with the full path to the file in which the exception
 * was thrown.
 * @param line the line number of the thrown exception.
 */
#define FL_EXCEPTION_HANDLER_FN(name)                                   \
    void name(void *ctx, FLExceptionReason reason, char const *details, \
              char const *file, int line)
typedef FL_EXCEPTION_HANDLER_FN(fl_exception_handler_fn);

#if defined(__cplusplus)
}
#endif

#endif // FL_EXCEPTION_TYPES_H_
