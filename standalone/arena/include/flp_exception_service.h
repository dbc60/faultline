#ifndef FLP_EXCEPTION_SERVICE_H_
#define FLP_EXCEPTION_SERVICE_H_

/**
 * @file flp_exception_service.h
 * @brief Standalone override (top-level) — driver builds use the same fl_sa_* impl.
 *
 * In the standalone arena there is no DLL boundary, so the FLP (platform/driver)
 * side and the FLA (application) side both wire directly to fl_sa_push/pop/throw.
 * This file mirrors faultline/fla_exception_service.h exactly so that
 * -DFL_BUILD_DRIVER builds compile without change.
 */
#include <faultline/fla_exception_service.h> // IWYU pragma: export

#endif // FLP_EXCEPTION_SERVICE_H_
