#ifndef REGION_TEST_H_
#define REGION_TEST_H_

/**
 * @file region_test.h
 * @author Douglas Cuthbertson
 * @brief Test case type definitions for the Region library.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "region.h" // Region

#include <faultline/fl_test.h> // FLTestCase

// Test allocating and initializing a Region
typedef struct RegionAllocationTestCase {
    FLTestCase tc;
    Region    *region;
} RegionAllocationTestCase;

#endif // REGION_TEST_H_
