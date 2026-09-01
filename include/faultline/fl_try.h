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
 * translation unit gets exactly one backend, and call sites use FL_TRY/FL_THROW
 * the same either way.
 */
#include <faultline/fl_exception_service.h> // fl_not_implemented, FL_*_EXCEPTION_SERVICE_FN
#include <faultline/fl_exception_types.h> // FLExceptionEnvironment, FL_ENTERED/THROWN/HANDLED, FL_MAX_DETAILS_LENGTH
#include <faultline/fl_macros.h> // FL_THREAD_LOCAL, FL_MAYBE_UNUSED
#include <setjmp.h>              // setjmp
#include <stddef.h>              // NULL
#include <stdio.h>               // snprintf
#include <string.h>              // strcmp

#if defined(FL_EXC_BACKEND_CXX) && !defined(__cplusplus)
#error "FL_EXC_BACKEND_CXX requires compiling as C++"
#endif

// The push/pop/throw hooks are the only asymmetry between the platform provider and
// the consumer accessor; everything below is defined once over them. FL_EXC_BACKEND_CXX
// is a separate, orthogonal axis: it picks the try/catch/throw *mechanism* (setjmp vs.
// a real C++ throw), independent of which side -- platform or consumer -- owns the
// throw hook that carries it out.
#if defined(FL_EXC_BACKEND_CXX)
// C++ unwinding needs no manual stack: the language finds its own way back to the
// matching catch, so pushing/popping an environment has nothing to do.
#define FL_EXC_PUSH(env) ((void)0)
#define FL_EXC_POP()     ((void)0)
#if defined(FL_PLATFORM_BUILD)
#include "flp_exception_service.h" // IWYU pragma: export -- flp_throw
#define FL_EXC_THROW(reason, details, file, line) flp_throw(reason, details, file, line)
#else                                        // consumer / DLL build
#include <faultline/fla_exception_service.h> // IWYU pragma: export -- g_fla_exception_service
#define FL_EXC_THROW(reason, details, file, line) \
    g_fla_exception_service.throw_exc(reason, details, file, line)
#endif // FL_PLATFORM_BUILD
#else  // setjmp backend
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
#endif // FL_EXC_BACKEND_CXX

// FL_TRY relies on {0}-initialization of FLExceptionEnvironment (or, under
// FL_EXC_BACKEND_CXX, FLExceptionFrame) leaving state == FL_ENTERED on setjmp's first
// return.
#if defined(__cplusplus)
static_assert(FL_ENTERED == 0, "FL_TRY requires FL_ENTERED == 0");
#else
_Static_assert(FL_ENTERED == 0, "FL_TRY requires FL_ENTERED == 0");
#endif

/**
 * @brief Per-thread scratch buffer for formatting exception details.
 *
 * All detail-formatting macros (FL_THROW_DETAILS, FL_THROW_DETAILS_FILE_LINE,
 * FL_ASSERT_DETAILS, FL_ASSERT_DETAILS_FILE_LINE) share this single buffer per
 * thread. Consequences:
 *  - Two threads formatting details concurrently do not race; each has its own buffer
 *    (subject to FL_THREAD_LOCAL being effective on the target).
 *  - A details pointer obtained from a caught exception (FL_DETAILS) is valid only until
 *    the next detail-formatting throw or assertion on the same thread. Code that keeps
 *    details longer, e.g., recording them in a test result or summary, must copy the
 *    string.
 *
 * longjmp never crosses threads, so a catch always reads the buffer of the
 * thread that formatted it. Under FL_EXC_BACKEND_CXX there is no longjmp, but the
 * same rule holds: a thread only ever unwinds its own stack, so a catch still only
 * ever reads the buffer it (or a callee on the same thread) formatted. That backend
 * adds a third writer beyond throw and assert: fl_cxx_capture_foreign (below) writes
 * this buffer whenever any FL_TRY stage catches a foreign exception, even one no
 * clause of that FL_TRY actually wants, so a saved FL_DETAILS pointer can be invalidated
 * by a nested FL_TRY that happens to catch an unrelated foreign exception, not only by
 * another throw or assertion on the same thread.
 */
static inline FL_MAYBE_UNUSED char *fl_details_buf(void) {
    static FL_THREAD_LOCAL char buf[FL_MAX_DETAILS_LENGTH];
    return buf;
}

#if defined(FL_EXC_BACKEND_CXX)
#include <exception> // std::exception

/**
 * @brief Capture a thrown FLException into the enclosing FL_TRY's frame.
 *
 * Called from the catch(FLException const&) that closes every stage (see
 * FL_CXX_STAGE_END). If state is already FL_HANDLED, the exception was thrown by a
 * handler body, e.g. a nested FL_THROW inside an FL_CATCH block, and must escape this
 * FL_TRY chain rather than be treated as a new pending exception of its own, so a bare
 * rethrow propagates it to whatever encloses this FL_TRY. Otherwise the exception's
 * fields are copied flat onto fl_env_ so FL_REASON/FL_DETAILS/FL_FILE/ FL_LINE keep
 * working unchanged, and state becomes FL_THROWN so the next stage's condition can test
 * for a match.
 */
static inline FL_MAYBE_UNUSED void fl_cxx_capture(FLExceptionFrame  &env,
                                                  FLException const &e) {
    if (env.state == FL_HANDLED) {
        throw;
    }
    env.reason  = e.reason;
    env.details = e.details;
    env.file    = e.file;
    env.line    = e.line;
    env.state   = FL_THROWN;
}

/**
 * @brief Capture a foreign C++ exception (not an FLException) into the enclosing
 * FL_TRY's frame, under the reason fl_foreign_exception.
 *
 * what is owned by the caught exception object, which the runtime may destroy once
 * this catch block returns -- unlike FLException::details, which already points into
 * fl_details_buf()'s per-thread scratch storage. Copying it there first keeps
 * FL_DETAILS valid under the same lifetime rule as every other detail string, rather
 * than leaving it dangling the moment the foreign exception is destroyed.
 *
 * env.foreign keeps the exception itself (std::current_exception(), not just its
 * message): if no clause in this FL_TRY wants it, FL_END_TRY rethrows the original
 * object rather than an FLException standing in for it, so a foreign exception passing
 * through an FL_TRY with no matching handler keeps its real type. file is a fixed
 * string rather than NULL: every other path through this file leaves FL_FILE non-NULL,
 * and callers format it with %s.
 */
static inline FL_MAYBE_UNUSED void fl_cxx_capture_foreign(FLExceptionFrame &env,
                                                          char const       *what) {
    if (env.state == FL_HANDLED) {
        throw;
    }
    char *details = fl_details_buf();
    snprintf(details, FL_MAX_DETAILS_LENGTH, "%s", what);
    env.reason  = fl_foreign_exception;
    env.details = details;
    env.file    = "<foreign exception>";
    env.line    = 0;
    env.state   = FL_THROWN;
    env.foreign = std::current_exception();
}

/**
 * @brief Closes the try opened by FL_TRY or a clause macro, and attaches the three
 * catches every stage shares. Every clause macro (FL_CATCH, FL_CATCH_STR,
 * FL_CATCH_ALL, FL_CATCH_ALL_RETHROW, FL_FINALLY, FL_END_TRY) starts with this.
 *
 * The three catches are tried in order: FLException by type (the common case, no
 * strcmp needed even for FL_CATCH_STR. That macro still compares fl_env_.reason by
 * string, but the exception itself is always an FLException), then std::exception for
 * a foreign exception with a message, then catch-all for anything else (e.g. a
 * consumer-defined type with no relation to std::exception).
 */
#define FL_CXX_STAGE_END                                \
    }                                                   \
    }                                                   \
    catch (FLException const &fl_ex_) {                 \
        fl_cxx_capture(fl_env_, fl_ex_);                \
    }                                                   \
    catch (std::exception const &fl_ex_) {              \
        fl_cxx_capture_foreign(fl_env_, fl_ex_.what()); \
    }                                                   \
    catch (...) {                                       \
        fl_cxx_capture_foreign(fl_env_, "unknown");     \
    }

/**
 * @brief Start a new try/catch section (C++ backend).
 *
 * Declares fl_env_ zero-initialized, where state == FL_ENTERED since FL_ENTERED == 0
 * (see the static_assert above), and opens the first stage's try. No jump buffer, no
 * push onto a stack: an exception thrown in the block below is caught by this same
 * stage's own catches (FL_CXX_STAGE_END, attached by whichever clause macro comes
 * next). fl_env_.reason is seeded the same way the setjmp backend's FL_TRY seeds it, so
 * FL_REASON is never NULL even if a clause reads it before anything has been thrown.
 */
#define FL_TRY                                         \
    do {                                               \
        FLExceptionFrame fl_env_ = {};                 \
        fl_env_.reason           = fl_not_implemented; \
        try {                                          \
            if (fl_env_.state == FL_ENTERED) {
/**
 * @brief Catch a specific exception by pointer identity (C++ backend).
 *
 * Same semantics as the setjmp backend's FL_CATCH: fl_env_.reason is compared to
 * @p exception by pointer, not strcmp, so this only matches an exception thrown from
 * the same module that owns the reason constant. See FL_CATCH_STR for a cross-module
 * match. This is not an else-if sibling of the previous stage; it is the next stage's
 * try, gated on fl_env_.state so it only runs the body when this clause is the one
 * that matches.
 */
#define FL_CATCH(exception)                                                \
    FL_CXX_STAGE_END                                                       \
    try {                                                                  \
        if (fl_env_.state == FL_THROWN && fl_env_.reason == (exception)) { \
            fl_env_.state = FL_HANDLED;

/**
 * @brief Catch a specific exception by string comparison (C++ backend).
 *
 * See the setjmp backend's FL_CATCH_STR for why: fl_unexpected_failure and its
 * siblings are duplicated per module by static linking, so a cross-module catch has
 * to compare the reason text rather than its address.
 */
#define FL_CATCH_STR(exception)                                                       \
    FL_CXX_STAGE_END                                                                  \
    try {                                                                             \
        if (fl_env_.state == FL_THROWN && strcmp(fl_env_.reason, (exception)) == 0) { \
            fl_env_.state = FL_HANDLED;

/**
 * @brief FL_CATCH_ALL catches any exception that hasn't already been caught.
 */
#define FL_CATCH_ALL                      \
    FL_CXX_STAGE_END                      \
    try {                                 \
        if (fl_env_.state == FL_THROWN) { \
            fl_env_.state = FL_HANDLED;

/**
 * @brief FL_CATCH_ALL_RETHROW runs its block for any uncaught exception and leaves the
 * exception pending, so FL_END_TRY rethrows it.
 *
 * Use this for failure-only cleanup that must not consume the exception:
 *
 *   FL_TRY { r = acquire(); use(r); }
 *   FL_CATCH_ALL_RETHROW { release(r); }
 *   FL_END_TRY;
 *
 * Unlike FL_CATCH_ALL, state is not set to FL_HANDLED here, so no explicit FL_RETHROW
 * is needed (or allowed) at the end of the block: FL_END_TRY finds state still
 * FL_THROWN and rethrows on its own.
 */
#define FL_CATCH_ALL_RETHROW \
    FL_CXX_STAGE_END         \
    try {                    \
        if (fl_env_.state == FL_THROWN) {
/**
 * @brief FL_FINALLY will run any code in this block regardless of whether an
 * exception has been thrown and regardless of whether a thrown exception has
 * been caught.
 *
 * Unlike the clause macros above, this block is unconditional -- there is no
 * fl_env_.state test gating whether it runs -- so it cannot be followed by another
 * clause, only by FL_END_TRY. If the block itself throws while a prior exception was
 * still pending (state == FL_THROWN), the new one replaces it: FL_CXX_STAGE_END's
 * catches recapture into fl_env_, and FL_END_TRY rethrows whichever is current.
 */
#define FL_FINALLY                     \
    FL_CXX_STAGE_END                   \
    if (fl_env_.state == FL_ENTERED) { \
        fl_env_.state = FL_HANDLED;    \
    }                                  \
    try {                              \
        {
/**
 * @brief FL_END_TRY ends a try/catch section.
 *
 * If an exception was thrown and it wasn't caught, it will rethrow it, passing
 * it to the next frame in the stack. A foreign exception (fl_env_.foreign set) is
 * rethrown as itself, preserving its original type and payload, rather than
 * reconstructed as an FLException standing in for it -- FL_RETHROW is only for an
 * exception that actually originated from FL_THROW.
 */
#define FL_END_TRY                                   \
    FL_CXX_STAGE_END                                 \
    if (fl_env_.state == FL_THROWN) {                \
        if (fl_env_.foreign) {                       \
            std::rethrow_exception(fl_env_.foreign); \
        }                                            \
        FL_RETHROW;                                  \
    }                                                \
    }                                                \
    while (0)

#else // setjmp backend

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
 * @brief FL_CATCH_ALL_RETHROW runs its block for any uncaught exception and leaves the
 * exception pending, so FL_END_TRY rethrows it.
 *
 * Use this for failure-only cleanup that must not consume the exception:
 *
 *   FL_TRY { r = acquire(); use(r); }
 *   FL_CATCH_ALL_RETHROW { release(r); }
 *   FL_END_TRY;
 *
 * Unlike FL_CATCH_ALL, the exception is not marked handled, so no explicit FL_RETHROW is
 * needed (or allowed) at the end of the block. This matters for more than brevity:
 * FL_RETHROW expands to a throw-hook call the optimizer can prove noreturn, so a catch
 * block ending in it makes FL_END_TRY's epilogue statically unreachable and trips C4702
 * under /WX in whole-program (unity/LTCG) release builds. Here the rethrow happens
 * inside FL_END_TRY's runtime conditional, which both success and failure paths reach,
 * so nothing is provably unreachable.
 *
 * To swallow the exception after all (e.g. only rethrow unexpected failures), use
 * FL_CATCH_ALL with an explicit conditional FL_RETHROW instead; the fall-through path
 * keeps the epilogue reachable there.
 */
#define FL_CATCH_ALL_RETHROW           \
    if (fl_env_.state == FL_ENTERED) { \
        FL_EXC_POP();                  \
    }                                  \
    }                                  \
    else {
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

#endif // FL_EXC_BACKEND_CXX

#define FL_THROW(reason) FL_EXC_THROW(reason, NULL, __FILE__, __LINE__)
#define FL_THROW_FILE_LINE(reason, file, line) \
    FL_EXC_THROW((reason), NULL, (file), (line))

/**
 * @brief FL_THROW_DETAILS throws reason with details.
 *
 * The details are formatted into the per-thread scratch buffer (fl_details_buf),
 * limited to FL_MAX_DETAILS_LENGTH bytes; see fl_details_buf for the lifetime
 * rules.
 *
 * @param reason is the FLExceptionReason string that briefly describes the exception.
 * @param details a string that adds details to the reason why the exception was thrown.
 * If there are arguments passed after details, then the details string is a format
 * string.
 */
#define FL_THROW_DETAILS(reason, details, ...)                                  \
    do {                                                                        \
        char *fl_details_ = fl_details_buf();                                   \
        snprintf(fl_details_, FL_MAX_DETAILS_LENGTH, (details), ##__VA_ARGS__); \
        FL_EXC_THROW((reason), fl_details_, __FILE__, __LINE__);                \
    } while (0)

/**
 * @brief throw an exception where the details is a format string with optional format
 * specifiers.
 *
 * The details are formatted into the per-thread scratch buffer (fl_details_buf),
 * limited to FL_MAX_DETAILS_LENGTH bytes; see fl_details_buf for the lifetime
 * rules.
 *
 * @param reason is the FLExceptionReason string that briefly describes the exception.
 * @param details  a string that adds details to the reason why the exception was thrown.
 * If there are arguments passed after details, then the details string must have format
 * specifiers to match.
 * @param file is the path to the source file where the exception was thrown.
 * @param line is the line number in the source file where the exception was thrown.
 */
#define FL_THROW_DETAILS_FILE_LINE(reason, details, file, line, ...)          \
    do {                                                                      \
        char *fl_details_ = fl_details_buf();                                 \
        snprintf(fl_details_, FL_MAX_DETAILS_LENGTH, details, ##__VA_ARGS__); \
        FL_EXC_THROW((reason), fl_details_, file, line);                      \
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
