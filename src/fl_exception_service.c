/**
 * @file fl_exception_service.c
 * @author Douglas Cuthbertson
 * @brief FLExceptionReason constant definitions shared by platform and application code.
 * @version 0.2
 * @date 2026-01-30
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_exception_types.h> // FLExceptionEnvironment, FLExceptionReason

//
// Exception reason constants
//

FLExceptionReason fl_expected_failure
    = "expected failure"; ///< test drivers catch this and do not report it as a failure
FLExceptionReason fl_unexpected_failure = "unexpected failure"; ///< something is wrong
FLExceptionReason fl_test_exception
    = "test exception"; ///< a test needs to throw and catch an exception
FLExceptionReason fl_not_implemented   = "not implemented"; ///< useful in development
FLExceptionReason fl_invalid_value     = "invalid value";   ///< an argument is invalid
FLExceptionReason fl_internal_error    = "internal error";  ///< a bad state
FLExceptionReason fl_invalid_address   = "invalid address"; ///< address not valid
