#ifndef FLA_EXCEPTION_SERVICE_H_
#define FLA_EXCEPTION_SERVICE_H_

/**
 * @file fla_exception_service.h
 * @author Douglas Cuthbertson
 * @brief Standalone override — FL_TRY/FL_CATCH/FL_END_TRY using fl_sa_push/pop/throw.
 *
 * Hook macros are wired directly to the standalone TLS setjmp/longjmp implementation
 * in fl_sa_exception_service.c.  No service struct, no injection, no DLL boundary.
 * @version 0.1
 * @date 2026-05-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <setjmp.h>
#include <stddef.h>                       // NULL
#include <stdio.h>                        // snprintf
#include <string.h>                       // strcmp
#include <faultline/fl_exception_types.h> // FLExceptionEnvironment, FL_MAX_DETAILS_LENGTH
#include <faultline/fl_exception_service.h> // extern reason declarations, FL_PUSH/POP/THROW typedefs

#if defined(__cplusplus)
extern "C" {
#endif

FL_PUSH_EXCEPTION_SERVICE_FN(fl_sa_push);
FL_POP_EXCEPTION_SERVICE_FN(fl_sa_pop);
FL_THROW_EXCEPTION_SERVICE_FN(fl_sa_throw);

/* Hook macros — the only asymmetry between this file and flp_exception_service.h */
#define FL_EXC_PUSH(env) fl_sa_push(env)
#define FL_EXC_POP()     fl_sa_pop()
#define FL_EXC_THROW(reason, details, file, line) \
    fl_sa_throw(reason, details, file, line)

#define FL_TRY                                               \
    do {                                                     \
        FLExceptionEnvironment fl_env_ = {0};                \
        fl_env_.reason                 = fl_not_implemented; \
        FL_EXC_PUSH(&fl_env_);                               \
        fl_env_.state = setjmp(fl_env_.jmp);                 \
        if (fl_env_.state == FL_ENTERED) {
#define FL_CATCH(exception)                   \
    if (fl_env_.state == FL_ENTERED) {        \
        FL_EXC_POP();                         \
    }                                         \
    }                                         \
    else if (fl_env_.reason == (exception)) { \
        fl_env_.state = FL_HANDLED;

#define FL_CATCH_STR(exception)                          \
    if (fl_env_.state == FL_ENTERED) {                   \
        FL_EXC_POP();                                    \
    }                                                    \
    }                                                    \
    else if (strcmp(fl_env_.reason, (exception)) == 0) { \
        fl_env_.state = FL_HANDLED;

#define FL_CATCH_ALL                   \
    if (fl_env_.state == FL_ENTERED) { \
        FL_EXC_POP();                  \
    }                                  \
    }                                  \
    else {                             \
        fl_env_.state = FL_HANDLED;

#define FL_FINALLY                         \
    if (fl_env_.state == FL_ENTERED) {     \
        FL_EXC_POP();                      \
    }                                      \
    }                                      \
    {                                      \
        if (fl_env_.state == FL_ENTERED) { \
            fl_env_.state = FL_HANDLED;    \
        }

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

#define FL_THROW_DETAILS(reason, details, ...)                         \
    do {                                                               \
        static char _details[FL_MAX_DETAILS_LENGTH];                   \
        snprintf(_details, sizeof _details, (details), ##__VA_ARGS__); \
        FL_EXC_THROW((reason), _details, __FILE__, __LINE__);          \
    } while (0)

#define FL_THROW_DETAILS_FILE_LINE(reason, details, file, line, ...) \
    do {                                                             \
        static char _details[FL_MAX_DETAILS_LENGTH];                 \
        snprintf(_details, sizeof _details, details, ##__VA_ARGS__); \
        FL_EXC_THROW((reason), _details, file, line);                \
    } while (0)

#define FL_RETHROW \
    FL_EXC_THROW(fl_env_.reason, fl_env_.details, fl_env_.file, fl_env_.line)

#ifndef FL_ASSERT_IMPL
#define FL_ASSERT_IMPL(msg, ...) \
    FL_THROW_DETAILS(fl_unexpected_failure, msg, ##__VA_ARGS__)
#endif

#ifndef FL_ASSERT_REASON_IMPL
#define FL_ASSERT_REASON_IMPL(REASON, msg, ...) \
    FL_THROW_DETAILS(REASON, msg, ##__VA_ARGS__)
#endif

#if defined(__cplusplus)
}
#endif

#endif // FLA_EXCEPTION_SERVICE_H_
