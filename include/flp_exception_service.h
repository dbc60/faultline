#ifndef FLP_EXCEPTION_SERVICE_H_
#define FLP_EXCEPTION_SERVICE_H_

/**
 * @file flp_exception_service.h
 * @author Douglas Cuthbertson
 * @brief Exception handling service provided by the test driver.
 * @version 0.2
 * @date 2026-01-29
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * The driver creates and initializes an FLExceptionService, then injects it into
 * test DLLs via fla_set_exception_service(). Test code accesses the service via
 * flp_get_exception_service() (TLS-based). New FL_TRY/FL_CATCH macros use this
 * service for exception handling.
 */
#include <faultline/fl_exception_service.h> // for fla_set_exception_service_fn, FL_P...
#include <faultline/fl_exception_types.h>   // for FL_MAX_DETAILS_LENGTH
#include <setjmp.h>                         // for setjmp
#include <stdio.h>                          // for NULL
#include <string.h>                         // strcmp
#if defined(__cplusplus)
extern "C" {
#endif

// Hook macros — the only asymmetry between this file and fla_exception_service.h
#define FL_EXC_PUSH(env)                          flp_push(env)
#define FL_EXC_POP()                              flp_pop()
#define FL_EXC_THROW(reason, details, file, line) flp_throw(reason, details, file, line)

/**
 * @brief Start a new try/catch section.
 *
 * FL_TRY creates a local FLExceptionEnvironment on the program stack and
 * pushes it to the top of the exception stack. It then calls setjmp to
 * initialize the local frame's jump buffer. Finally, it compares setjmp's
 * return value in fl_env_.state to FL_ENTERED (which is zero). If that's
 * so, then execution continues with the code in the FL_TRY block.
 *
 * If setjmp returns a non-zero value (which should be FL_THROWN), then an
 * exception has been thrown via a call to longjmp (see the implementation of
 * flp_throw in flp_exception_service.c). In this case, if any FL_CATCH
 * blocks are executed and match the thrown exception, or if a FL_CATCH_ALL
 * block exists, then fl_env_.state is set to FL_HANDLED and the handler
 * code is run.
 */
#define FL_TRY                                               \
    do {                                                     \
        FLExceptionEnvironment fl_env_ = {0};                \
        fl_env_.reason                 = fl_not_implemented; \
        FL_EXC_PUSH(&fl_env_);                               \
        fl_env_.state = setjmp(fl_env_.jmp);                 \
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

/**
 * @brief Connect the platform and application implementations of the exception service
 */
#define FLP_INIT_EXCEPTION_SERVICE_FN(name) \
    void name(fla_set_exception_service_fn *fla_set)
typedef FLP_INIT_EXCEPTION_SERVICE_FN(flp_init_exception_service_fn);
FLP_INIT_EXCEPTION_SERVICE_FN(flp_init_exception_service);

/**
 * @brief push an FLExceptionEnvironment on to a stack. This is done by FL_TRY.
 *
 */
FL_PUSH_EXCEPTION_SERVICE_FN(flp_push);

/**
 * @brief pop an FLExceptionEnvironment from a stack. This is done by FL_CATCH,
 * FL_CATCH_ALL, FL_FINALLY, and FL_END_TRY.
 *
 */
FL_POP_EXCEPTION_SERVICE_FN(flp_pop);

/**
 * @brief pop an FLExceptionEnvironment from a stack and use it to throw an exception
 * (via longjmp).
 *
 * Precondition: for the platform (FLP) implementation, the driver must have a top-level
 * try-catch block to ensure there is always an exception environment to pop.
 *
 */
FL_THROW_EXCEPTION_SERVICE_FN(flp_throw);

#if defined(__cplusplus)
}
#endif

#endif // FLP_EXCEPTION_SERVICE_H_
