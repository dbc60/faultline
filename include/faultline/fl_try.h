#ifndef FL_TRY_H_
#define FL_TRY_H_

/**
 * @file fl_try.h
 * @brief Unified try/catch/throw macros that work in both platform and application code.
 *
 * This header selects include/log/flp_exception_service.h or
 * include/fla_exception_service.h to provide FL_TRY, FL_CATCH, FL_END_TRY, FL_THROW,
 * etc. macros based on whether FL_BUILD_DRIVER is defined.
 */

#if defined(FL_BUILD_DRIVER)
#include "flp_exception_service.h"           // IWYU pragma: export
#else                                        // Application/DLL build
#include <faultline/fla_exception_service.h> // IWYU pragma: export
#endif                                       // FL_BUILD_DRIVER

#endif // FL_TRY_H_
