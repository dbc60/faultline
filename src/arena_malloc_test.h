#ifndef ARENA_MALLOC_TEST_H_
#define ARENA_MALLOC_TEST_H_
/**
 * @file arena_malloc_test.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-03-18
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */

#include "arena_internal.h"

#include <faultline/fl_test.h> // FLTestCase

struct TestRequest {
    FLTestCase tc;
    Arena     *arena;
    size_t     lo;
    size_t     hi;
};
typedef struct TestRequest TestRequest;

void cleanup(FLTestCase *btc);
void setup_small_range(FLTestCase *btc);
void setup_large_range(FLTestCase *btc);

#endif // ARENA_MALLOC_TEST_H_
