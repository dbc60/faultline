#ifndef FL_TRY_H_
#define FL_TRY_H_

/**
 * @file fl_try.h
 * @brief Unified try/catch/throw macros for both platform and consumer code.
 *
 * Single definition site for the FL_TRY / FL_CATCH* / FL_THROW* / FL_END_TRY family.
 * The push/pop/throw hooks are the only thing that differs between the platform
 * provider (flp_push/pop/throw) and the injected consumer accessor
 * (g_fla_exception_service); this header selects the hooks by whether
 * FL_PLATFORM_BUILD is defined, then defines the whole family once over them. A
 * translation unit gets exactly one backend, and call sites spell FL_TRY/FL_THROW
 * the same either way.
 */
#include <faultline/fl_exception_service.h> // fl_not_implemented, FL_*_EXCEPTION_SERVICE_FN
#include <faultline/fl_exception_types.h>   // FLExceptionEnvironment, FL_ENTERED/THROWN/HANDLED, FL_MAX_DETAILS_LENGTH
#include <setjmp.h>                         // setjmp
#include <stddef.h>                         // NULL
#include <stdio.h>                          // snprintf
#include <string.h>                         // strcmp

// The push/pop/throw hooks are the only asymmetry between the platform provider and
// the consumer accessor; everything below is defined once over them.
#if defined(FL_PLATFORM_BUILD)
#include "flp_exception_service.h" // IWYU pragma: export -- flp_push/pop/throw
#define FL_EXC_PUSH(env)                          flp_push(env)
#define FL_EXC_POP()                              flp_pop()
#define FL_EXC_THROW(reason, details, file, line) flp_throw(reason, details, file, line)
#else                                        // consumer / DLL build
#include <faultline/fla_exception_service.h> // IWYU pragma: export -- g_fla_exception_service
#define FL_EXC_PUSH(env) g_fla_exception_service.push_env(env)
#define FL_EXC_POP()     g_fla_exception_service.pop_env()
#define FL_EXC_THROW(reason, details, file, line) \
    g_fla_exception_service.throw_exc(reason, details, file, line)
#endif // FL_PLATFORM_BUILD

// FL_TRY relies on {0}-initialization of FLExceptionEnvironment leaving state ==
// FL_ENTERED on setjmp's first return.
#if defined(__cplusplus)
static_assert(FL_ENTERED == 0, "FL_TRY requires FL_ENTERED == 0");
#else
_Static_assert(FL_ENTERED == 0, "FL_TRY requires FL_ENTERED == 0");
#endif

/**
 * @brief Start a new try/catch section.
 *
 * FL_TRY creates a local FLExceptionEnvironment on the program stack with its state
 * pre-set to FL_ENTERED (by zero-initialization), pushes it to the top of the
 * exception stack, and calls setjmp to initialize the frame's jump buffer.
 *
 * The setjmp invocation appears only as `setjmp(...) != 0`, one of the contexts
 * C11 7.13.1.1p4 permits; assigning its result to a variable would be undefined
 * behavior. On the first return setjmp yields 0, the branch is not taken, and
 * state remains FL_ENTERED, so execution continues with the code in the FL_TRY
 * block. When an exception is thrown, longjmp (see the throw hook) returns through
 * setjmp with a non-zero value and state is set to FL_THROWN. The state member is
 * volatile-qualified, so that store is preserved across the longjmp (C11
 * 7.13.2.1p3). A matching FL_CATCH/FL_CATCH_ALL then sets fl_env_.state to
 * FL_HANDLED and runs the handler.
 */
#define FL_TRY                                               \
    do {                                                     \
        FLExceptionEnvironment fl_env_ = {0};                \
        fl_env_.reason                 = fl_not_implemented; \
        FL_EXC_PUSH(&fl_env_);                               \
        if (setjmp(fl_env_.jmp) != 0) {                      \
            fl_env_.state = FL_THROWN;                       \
        }                                                    \
        if (fl_env_.state == FL_ENTERED) {
/**
 * @brief Catch a specific exception by pointer identity.
 *
 * Compares fl_env_.reason to @p exception using pointer equality, not strcmp.
 * This is intentional: exception reasons are address tokens, not strings. Two
 * independently defined variables with identical text are different reasons.
 *
 * Consequence for static libraries: fl_exception_service.c is linked into each
 * module separately, so each module has its own copy of the shared reason
 * constants (fl_unexpected_failure, fl_expected_failure, etc.) at distinct
 * addresses. FL_CATCH therefore only matches an exception thrown from the same
 * module that owns the reason pointer passed as @p exception.
 *
 * The idiomatic pattern is to define a module-private reason, throw it, and
 * catch it within the same translation unit:
 *
 *   static FLExceptionReason my_reason = "my reason";
 *   FL_TRY { FL_THROW(my_reason); }
 *   FL_CATCH(my_reason) { ... }
 *   FL_END_TRY;
 *
 * To detect a specific exception that may have been thrown by a different
 * module (e.g., a test suite DLL), use FL_CATCH_STR, below.
 */
#define FL_CATCH(exception)                   \
    if (fl_env_.state == FL_ENTERED) {        \
        FL_EXC_POP();                         \
    }                                         \
    }                                         \
    else if (fl_env_.reason == (exception)) { \
        fl_env_.state = FL_HANDLED;

/**
 * @brief Catch a specific exception by string comparison.
 *
 * Compares fl_env_.reason to @p exception using strcmp rather than pointer
 * equality. Use this instead of FL_CATCH when the exception may have been
 * thrown by a different module (e.g., a test suite DLL catching one of the
 * shared reason constants from fl_exception_service.h). Because those
 * constants are statically linked into each module separately, their addresses
 * differ across modules and FL_CATCH would silently fail to match.
 *
 *   FL_TRY { ... }
 *   FL_CATCH_STR(fl_unexpected_failure) { ... }
 *   FL_END_TRY;
 */
#define FL_CATCH_STR(exception)                          \
    if (fl_env_.state == FL_ENTERED) {                   \
        FL_EXC_POP();                                    \
    }                                                    \
    }                                                    \
    else if (strcmp(fl_env_.reason, (exception)) == 0) { \
        fl_env_.state = FL_HANDLED;

/**
 * @brief FL_CATCH_ALL catches any exception that hasn't already been caught.
 *
 * Catch any and all exceptions, mark them as handled and execute the code in
 * the catch-all block.
 */
#define FL_CATCH_ALL                   \
    if (fl_env_.state == FL_ENTERED) { \
        FL_EXC_POP();                  \
    }                                  \
    }                                  \
    else {                             \
        fl_env_.state = FL_HANDLED;

/**
 * @brief FL_FINALLY will run any code in this block regardless of whether an
 * exception has been thrown and regardless of whether a thrown exception has
 * been caught.
 */
#define FL_FINALLY                         \
    if (fl_env_.state == FL_ENTERED) {     \
        FL_EXC_POP();                      \
    }                                      \
    }                                      \
    {                                      \
        if (fl_env_.state == FL_ENTERED) { \
            fl_env_.state = FL_HANDLED;    \
        }

/**
 * @brief FL_END_TRY ends a try/catch section.
 *
 * If an exception was thrown and it wasn't caught, it will rethrow it, passing
 * it to the next frame in the stack.
 */
#define FL_END_TRY                     \
    if (fl_env_.state == FL_ENTERED) { \
        FL_EXC_POP();                  \
    }                                  \
    }                                  \
    if (fl_env_.state == FL_THROWN) {  \
        FL_RETHROW;                    \
    }                                  \
    }                                  \
    while (0)

#define FL_THROW(reason) FL_EXC_THROW(reason, NULL, __FILE__, __LINE__)
#define FL_THROW_FILE_LINE(reason, file, line) \
    FL_EXC_THROW((reason), NULL, (file), (line))

/**
 * @brief FL_THROW_DETAILS throws reason with details.
 *
 * There is a limit of FL_MAX_DETAILS_LENGTH bytes in the buffer used for formatting the
 * reason.
 *
 * @param reason is the FLExceptionReason string that briefly describes the exception.
 * @param details a string that adds details to the reason why the exception was thrown.
 * If there are arguments passed after details, then the details string is a format
 * string.
 */
#define FL_THROW_DETAILS(reason, details, ...)                         \
    do {                                                               \
        static char _details[FL_MAX_DETAILS_LENGTH];                   \
        snprintf(_details, sizeof _details, (details), ##__VA_ARGS__); \
        FL_EXC_THROW((reason), _details, __FILE__, __LINE__);          \
    } while (0)

/**
 * @brief throw an exception where the details is a format string with optional format
 * specifiers.
 *
 * There is a limit of FL_MAX_DETAILS_LENGTH bytes in the buffer used for formatting the
 * reason.
 *
 * @param reason is the FLExceptionReason string that briefly describes the exception.
 * @param details  a string that adds details to the reason why the exception was thrown.
 * If there are arguments passed after details, then the details string must have format
 * specifiers to match.
 * @param file is the path to the source file where the exception was thrown.
 * @param line is the line number in the source file where the exception was thrown.
 */
#define FL_THROW_DETAILS_FILE_LINE(reason, details, file, line, ...) \
    do {                                                             \
        static char _details[FL_MAX_DETAILS_LENGTH];                 \
        snprintf(_details, sizeof _details, details, ##__VA_ARGS__); \
        FL_EXC_THROW((reason), _details, file, line);                \
    } while (0)

#define FL_RETHROW \
    FL_EXC_THROW(fl_env_.reason, fl_env_.details, fl_env_.file, fl_env_.line)

// Helper macro to format assertion failure messages
#define FL_ASSERT_IMPL(msg, ...) \
    FL_THROW_DETAILS(fl_unexpected_failure, msg, ##__VA_ARGS__)

// Helper macro that accepts a reason string so failed assertions can have a reason other
// than fl_unexpected_failure
#define FL_ASSERT_REASON_IMPL(REASON, msg, ...) \
    FL_THROW_DETAILS(REASON, msg, ##__VA_ARGS__)

#endif // FL_TRY_H_
