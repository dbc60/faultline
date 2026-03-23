#ifndef FL_LOG_H_
#define FL_LOG_H_

/**
 * @file fl_log.h
 * @brief Unified LOG_* macros that work in both platform and application code.
 *
 * This header selects include/log/flp_log_service.h or include/fla_log_service.h to
 * provide LOG_ERROR, LOG_INFO, etc. macros based on whether FL_BUILD_DRIVER is defined.
 */

#if defined(FL_BUILD_DRIVER)
#include <flp_log_service.h>             // IWYU pragma: export
#else                                    // Application/DLL build
#include <faultline/fla_log_service.h>   // IWYU pragma: export
#endif                           // FL_BUILD_DRIVER

#endif // FL_LOG_H_
