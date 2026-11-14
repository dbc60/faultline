/**
 * @file asan_canary_tests.c
 * @author Douglas Cuthbertson
 * @brief A suite whose one case must end in an AddressSanitizer report.
 * @version 0.1
 * @date 2026-11-14
 *
 * all.cmd runs this suite alone, before the other suites, when the asan build option is
 * set. The run passes only if the driver exits non-zero and leaves a report naming a
 * heap-buffer-overflow in this file. A clean exit means the binaries are not
 * instrumented, or the ASan runtime is not loaded, or its report is not written.
 *
 * ASan ends the process at its first report, so the case cannot be caught in-process
 * and the suite must never be in the ordinary suite list.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_exception.c"
#include "fla_timer_service.c" // g_fla_timer_service, fla_set_timer_service

#include <faultline/fl_test.h> // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE

#include <stddef.h> // size_t
#include <stdlib.h> // malloc

/// Bytes in the block the case overflows.
#define CANARY_BLOCK_SIZE 16

// Write one byte past the end of a heap block. volatile keeps the compiler from
// proving the index out of range and diagnosing it at compile time.
//
// The block is never freed. ASan reports at the write, so free() adds nothing under
// ASan. Without ASan, the debug CRT's free() finds the overwritten guard bytes and
// opens a modal dialog, which blocks the build instead of failing the canary.
FL_TEST("Heap Overflow", asan_canary_heap_overflow) {
    volatile size_t len   = CANARY_BLOCK_SIZE;
    char           *block = malloc(len);

    FL_ASSERT_NOT_NULL(block);
    block[len] = 1;
}

FL_SUITE_BEGIN(canary)
FL_SUITE_ADD(asan_canary_heap_overflow)
FL_SUITE_END;

FL_GET_TEST_SUITE("ASan Canary", canary)
