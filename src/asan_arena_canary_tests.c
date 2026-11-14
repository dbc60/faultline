/**
 * @file asan_arena_canary_tests.c
 * @author Douglas Cuthbertson
 * @brief A suite whose one case must end in an AddressSanitizer report on arena memory.
 * @version 0.1
 * @date 2026-11-14
 *
 * ASan sees arena memory only because the arena poisons it: the allocator obtains its
 * regions from the OS rather than the CRT heap. all.cmd runs this suite alone, before
 * the other suites, when the asan build option is set. The run passes only if the
 * driver exits non-zero and leaves a report naming a use-after-poison in this file. A
 * clean exit means the arena no longer poisons freed blocks, so a clean run of the
 * other suites would say nothing about arena memory.
 *
 * ASan ends the process at its first report, so the case cannot be caught in-process
 * and the suite must never be in the ordinary suite list.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "arena.c"
#include "arena_dbg.c"
#include "digital_search_tree.c"
#include "fl_exception.c"
#include "fla_log_service.c"    // g_fla_log_service
#include "fla_memory_service.c" // g_fla_memory_service, fla_set_memory_service
#include "fla_timer_service.c"  // g_fla_timer_service, fla_set_timer_service
#include "lock_os.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"

#include <faultline/arena.h>   // ARENA_MALLOC_THROW, ARENA_FREE_THROW
#include <faultline/fl_test.h> // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE

/// Bytes in each block the case allocates.
#define CANARY_BLOCK_SIZE 256

/// Offset of the byte read after free. The first bytes of a freed block hold the
/// free-list links and stay addressable, so the read lands past them.
#define CANARY_READ_OFFSET 128

// Read a freed block. The second block stays allocated, so the freed one goes to a bin
// rather than merging into top. volatile keeps the compiler from dropping the read.
//
// Without ASan the read returns stale bytes, the case passes, and the driver exits 0,
// which fails the canary check.
FL_TEST("Arena Use After Free", asan_arena_canary_use_after_free) {
    Arena *arena = new_arena(4096, 0);
    char  *freed = ARENA_MALLOC_THROW(arena, CANARY_BLOCK_SIZE);
    char  *kept  = ARENA_MALLOC_THROW(arena, CANARY_BLOCK_SIZE);

    ARENA_FREE_THROW(arena, freed);
    volatile char stale = freed[CANARY_READ_OFFSET];
    (void)stale;

    ARENA_FREE_THROW(arena, kept);
    release_arena(&arena);
}

FL_SUITE_BEGIN(canary)
FL_SUITE_ADD(asan_arena_canary_use_after_free)
FL_SUITE_END;

FL_GET_TEST_SUITE("ASan Arena Canary", canary)
