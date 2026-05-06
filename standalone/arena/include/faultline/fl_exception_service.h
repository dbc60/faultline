#ifndef FL_EXCEPTION_SERVICE_H_
#define FL_EXCEPTION_SERVICE_H_

/**
 * @file fl_exception_service.h
 * @brief Standalone override — extern reason declarations only; no service struct.
 */
#include <faultline/fl_exception_types.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern FLExceptionReason fl_expected_failure;
extern FLExceptionReason fl_test_exception;
extern FLExceptionReason fl_not_implemented;
extern FLExceptionReason fl_invalid_value;
extern FLExceptionReason fl_internal_error;
extern FLExceptionReason fl_invalid_address;

/* Function-pointer typedefs retained so headers that reference them compile. */
#define FL_PUSH_EXCEPTION_SERVICE_FN(name) void name(FLExceptionEnvironment *env)
#define FL_POP_EXCEPTION_SERVICE_FN(name)  void name(void)
#define FL_THROW_EXCEPTION_SERVICE_FN(name) \
    void name(FLExceptionReason reason, char const *details, char const *file, int line)

typedef FL_PUSH_EXCEPTION_SERVICE_FN(fl_push_exception_service_fn);
typedef FL_POP_EXCEPTION_SERVICE_FN(fl_pop_exception_service_fn);
typedef FL_THROW_EXCEPTION_SERVICE_FN(fl_throw_exception_service_fn);

#define FL_REASON  (fl_env_.reason)
#define FL_DETAILS (fl_env_.details)
#define FL_FILE    (fl_env_.file)
#define FL_LINE    (fl_env_.line)

#if defined(__cplusplus)
}
#endif

#endif // FL_EXCEPTION_SERVICE_H_
