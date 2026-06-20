#ifndef FL_TRY_H_
#define FL_TRY_H_

/**
 * @file fl_try.h
 * @brief Unified try/catch/throw macros that work in both platform and application code.
 *
 * This header selects include/log/flp_exception_service.h or
 * include/fla_exception_service.h to provide FL_TRY, FL_CATCH, FL_END_TRY, FL_THROW,
 * etc. macros based on whether FL_PLATFORM_BUILD is defined.
 */

#if defined(FL_PLATFORM_BUILD)
#include "flp_exception_service.h"           // IWYU pragma: export
#else                                        // Application/DLL build
#include <faultline/fla_exception_service.h> // IWYU pragma: export
#endif                                       // FL_PLATFORM_BUILD

#endif // FL_TRY_H_
