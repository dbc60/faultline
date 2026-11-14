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
 * clean exit means the arena no longer poisons the byte the case reads, so the other
 * suites could make that access under ASan without a report.
 *
 * ASan ends the process at its first report, so each build of this file holds one
 * case, chosen by ASAN_ARENA_CANARY. asan_arena_canary.cmd builds one DLL per case:
 *
 *   ASAN_ARENA_CANARY        block      byte read
 *   -----------------------  ---------  ------------------------------------------
 *   CANARY_FREE_MIDDLE       freed      the middle of the payload
 *   CANARY_FREE_LINKS        freed      the first byte, where the free-list links go
 *   CANARY_FREE_FOOTER       freed      the last byte, where the footer goes
 *   CANARY_OVERFLOW          allocated  the first byte past the request
 *   CANARY_UNDERFLOW         allocated  the last byte before the payload
 *
 * The request is a multiple of the chunk alignment, so the overflow reads the first
 * byte of the redzone.
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
#include <stddef.h>            // ptrdiff_t

/// Bytes in each block the case allocates; a multiple of CHUNK_ALIGNMENT.
#define CANARY_BLOCK_SIZE 256

#define CANARY_FREE_MIDDLE 1
#define CANARY_FREE_LINKS  2
#define CANARY_FREE_FOOTER 3
#define CANARY_OVERFLOW    4
#define CANARY_UNDERFLOW   5

/// Whether the case reads the block after freeing it or while it is allocated.
typedef enum CanaryBlock {
    CANARY_BLOCK_FREED,
    CANARY_BLOCK_ALLOCATED,
} CanaryBlock;

#if !defined(ASAN_ARENA_CANARY)
#error "Define ASAN_ARENA_CANARY as one of the CANARY_* cases"
#elif ASAN_ARENA_CANARY == CANARY_FREE_MIDDLE
#define CANARY_STATE  CANARY_BLOCK_FREED
#define CANARY_OFFSET (CANARY_BLOCK_SIZE / 2)
#elif ASAN_ARENA_CANARY == CANARY_FREE_LINKS
#define CANARY_STATE  CANARY_BLOCK_FREED
#define CANARY_OFFSET 0
#elif ASAN_ARENA_CANARY == CANARY_FREE_FOOTER
#define CANARY_STATE  CANARY_BLOCK_FREED
#define CANARY_OFFSET (CANARY_BLOCK_SIZE - 1)
#elif ASAN_ARENA_CANARY == CANARY_OVERFLOW
#define CANARY_STATE  CANARY_BLOCK_ALLOCATED
#define CANARY_OFFSET CANARY_BLOCK_SIZE
#elif ASAN_ARENA_CANARY == CANARY_UNDERFLOW
#define CANARY_STATE  CANARY_BLOCK_ALLOCATED
#define CANARY_OFFSET (-1)
#else
#error "ASAN_ARENA_CANARY names no CANARY_* case"
#endif

// Read one byte at offset from the start of a block, after freeing it or while it is
// allocated. The second block stays allocated, so a freed block goes to a bin rather
// than merging into top. volatile keeps the compiler from dropping the read.
//
// Without a report the read returns whatever the byte holds, the case passes, and the
// driver exits 0, which fails the canary check.
static void read_arena_byte(ptrdiff_t offset, CanaryBlock state) {
    Arena *arena = new_arena(4096, 0);
    char  *block = ARENA_MALLOC_THROW(arena, CANARY_BLOCK_SIZE);
    char  *kept  = ARENA_MALLOC_THROW(arena, CANARY_BLOCK_SIZE);

    if (state == CANARY_BLOCK_FREED) {
        ARENA_FREE_THROW(arena, block);
    }

    volatile char byte = block[offset];
    (void)byte;

    if (state == CANARY_BLOCK_ALLOCATED) {
        ARENA_FREE_THROW(arena, block);
    }

    ARENA_FREE_THROW(arena, kept);
    release_arena(&arena);
}

FL_TEST("Arena Poisoned Read", asan_arena_canary_read) {
    read_arena_byte(CANARY_OFFSET, CANARY_STATE);
}

FL_SUITE_BEGIN(canary)
FL_SUITE_ADD(asan_arena_canary_read)
FL_SUITE_END;

FL_GET_TEST_SUITE("ASan Arena Canary", canary)
