#ifndef FL_DRIVER_H_
#define FL_DRIVER_H_

/**
 * @file faultline_driver.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.2.0
 * @date 2025-02-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_context.h> // FLContext

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief execute the current test case and collect test results.
 *
 * @param fctx a test context.
 */
#define FL_EXERCISE_TEST(name) void name(FLContext *fctx)
typedef FL_EXERCISE_TEST(faultline_driver_fn);
FL_EXERCISE_TEST(faultline_driver);
FL_EXERCISE_TEST(faultline_run_test);

#if defined(__cplusplus)
}
#endif

#endif // FL_DRIVER_H_
