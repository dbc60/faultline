#ifndef FL_TRY_H_
#define FL_TRY_H_

/**
 * @file fl_try.h
 * @author Douglas Cuthbertson
 * @brief Standalone override — same FL_BUILD_DRIVER selector as the original.
 *
 * Both paths resolve to fl_sa_push/pop/throw; the split is structural only.
 * @version 0.1
 * @date 2026-05-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#if defined(FL_BUILD_DRIVER)
#include "flp_exception_service.h" // IWYU pragma: export
#else
#include <faultline/fla_exception_service.h> // IWYU pragma: export
#endif

#endif // FL_TRY_H_
