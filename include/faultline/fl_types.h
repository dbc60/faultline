#ifndef FAULTLINE_TYPES_H_
#define FAULTLINE_TYPES_H_

/**
 * @file faultline_types.h
 * @author Douglas Cuthbertson
 * @brief Common type definitions for the FaultLine testing framework - Public API
 * @version 0.2.0
 * @date 2025-09-01
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This header defines common types used throughout the FaultLine testing framework,
 * including test phases (discovery vs. injection) and failure type categorization.
 */

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Test execution phase enumeration for failure categorization
 */
typedef enum {
    FL_DISCOVERY_PHASE, ///< discovery phase - main path execution without fault
                        ///< injection
    FL_INJECTION_PHASE  ///< fault injection phase - systematic fault injection
                        ///< testing
} FLTestPhase;

/**
 * @brief Test failure type enumeration for detailed categorization
 */
typedef enum {
    FL_FAILURE_NONE,        ///< no failure occurred (test passed)
    FL_SETUP_FAILURE,       ///< failure occurred during test setup
    FL_TEST_FAILURE,        ///< failure occurred during test execution
    FL_CLEANUP_FAILURE,     ///< failure occurred during test cleanup
    FL_LEAK_FAILURE,        ///< resource leak detected
    FL_INVALID_FREE_FAILURE ///< invalid resource release detected
} FLFailureType;

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_TYPES_H_
