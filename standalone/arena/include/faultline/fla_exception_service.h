#ifndef FLA_EXCEPTION_SERVICE_H_
#define FLA_EXCEPTION_SERVICE_H_

/**
 * @file fla_exception_service.h
 * @author Douglas Cuthbertson
 * @brief Application-side exception service macros for test suite DLLs.
 * @version 0.1
 * @date 2026-02-01
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <setjmp.h> // for setjmp
#include <stddef.h> // IWYU pragma: keep - NULL used in FL_THROW
#include <stdio.h>  // IWYU pragma: keep - snprintf used in FL_THROW_DETAILS macro body
#include <string.h> // strcmp
#include <faultline/fl_exception_service.h> // for FLExceptionService
#include <faultline/fl_exception_types.h>   // for FL_MAX_DETAILS_LENGTH
#include <faultline/fl_macros.h>            // FL_DECL_SPEC

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Exception service.
 *
 * Each DLL gets its own copy of this variable (static linkage). The driver sets it via
 * fla_set_exception_service() after loading the DLL so it's push, pop, and throw fields
 * point to the driver's implementation. FL_TRY*, FL_CATCH*, and FL_END_TRY* macros use
 * it to manage the stack of exception environments.
 */
extern FLExceptionService g_fla_exception_service;

/**
 * @brief Set the exception service for an application.
 *
 * @param svc Address of a platform-owned exception service
 * @param size the size of the exception service in bytes
 */
FL_DECL_SPEC FLA_SET_EXCEPTION_SERVICE_FN(fla_set_exception_service);

// Hook macros — the only asymmetry between this file and flp_exception_service.h
#define FL_EXC_PUSH(env) g_fla_exception_service.push_env(env)
#define FL_EXC_POP()     g_fla_exception_service.pop_env()
#define FL_EXC_THROW(reason, details, file, line) \
    g_fla_exception_service.throw_exc(reason, details, file, line)

/**
 * @brief Start a try block using the exception service.
 *
 * Creates a local FLExceptionEnvironment, retrieves the exception service
 * from TLS, and pushes the environment onto the service's stack.
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
 * @brief Catch any exception.
 *
 * Catches all exceptions regardless of reason, marks them as handled,
 * and executes the catch-all block.
 */
#define FL_CATCH_ALL                   \
    if (fl_env_.state == FL_ENTERED) { \
        FL_EXC_POP();                  \
    }                                  \
    }                                  \
    else {                             \
        fl_env_.state = FL_HANDLED;

/**
 * @brief Run code regardless of exception state.
 *
 * The finally block executes whether or not an exception was thrown,
 * and whether or not it was caught.
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
 * @brief End a try block. Rethrows unhandled exceptions.
 *
 * If an exception was thrown and not caught, rethrows it to the next
 * enclosing try block (or the driver's unhandled exception handler).
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

#if defined(__cplusplus)
}
#endif

#endif // FLA_EXCEPTION_SERVICE_H_
