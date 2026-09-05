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
#include <setjmp.h> // jmp_buf

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

    // reason, details, file, and line are written by fl_throw() after setjmp() saves
    // state and before longjmp() fires. Per C11 §7.13.2.1 ¶3, automatic-duration objects
    // modified between setjmp and longjmp have indeterminate value after longjmp unless
    // they are volatile-qualified. Without volatile, an optimizing compiler may hold
    // these fields in registers (restoring stale values on longjmp) instead of reloading
    // from memory.
    FLExceptionReason volatile reason; ///< a constant string describing the reason for
                                       ///< the exception.
    char const *volatile details;      ///< extra details about the exception
    char const *volatile file;         ///< the file where the exception was thrown.
    int volatile line;                 ///< the line where the exception was thrown.
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

#if defined(FL_EXC_BACKEND_CXX)
#include <exception> // std::exception_ptr

/**
 * @brief The object thrown by the C++ exception backend (FL_EXC_BACKEND_CXX).
 *
 * Mirrors the four arguments fl_throw() would otherwise hand to longjmp(): reason,
 * details, file and line. details is deliberately not owned here -- it borrows the
 * same per-thread scratch buffer as the setjmp backend (fl_details_buf(), fl_try.h),
 * so it is valid only until the next detail-formatting throw or foreign-exception
 * catch on the same thread, exactly as FL_DETAILS is documented today; see
 * fl_details_buf()'s comment for the full lifetime rule.
 */
class FLException {
  public:
    FLException()
        : reason(nullptr)
        , details(nullptr)
        , file(nullptr)
        , line(0) {
    }

    FLException(FLExceptionReason reason, char const *details, char const *file,
                int line)
        : reason(reason)
        , details(details)
        , file(file)
        , line(line) {
    }

    FLExceptionReason reason;
    char const       *details;
    char const       *file;
    int               line;
};

/**
 * @brief Per-FL_TRY bookkeeping for the C++ backend; the C++ analogue of
 * FLExceptionEnvironment.
 *
 * FL_REASON/FL_DETAILS/FL_FILE/FL_LINE (fl_exception.h) read fl_env_.reason,
 * .details, .file and .line unconditionally, so this type carries them as flat fields
 * rather than nesting them inside an FLException member -- keeping those accessor
 * macros identical across both backends. There is no jmp_buf and no push/pop stack:
 * C++ unwinding finds its own way back to the matching catch.
 *
 * foreign holds the original exception when the pending one did not come from FL_THROW
 * -- a std::exception or anything else caught by FL_CXX_STAGE_END. FL_END_TRY rethrows
 * it directly (std::rethrow_exception) instead of reconstructing an FLException, so a
 * foreign exception passing through an FL_TRY with no clause that wants it keeps its
 * original type and payload, rather than always surfacing as an FLException.
 */
struct FLExceptionFrame {
    FLExceptionState   state;
    FLExceptionReason  reason;
    char const        *details;
    char const        *file;
    int                line;
    std::exception_ptr foreign;
};
#endif // FL_EXC_BACKEND_CXX
#endif // __cplusplus

#endif // FL_EXCEPTION_TYPES_H_
