/**
 * @file arena_test.c
 * @author Douglas Cuthbertson
 * @brief test cases for Arena
 * @version 0.1
 * @date 2023-10-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "arena_dbg.h"
#include "arena_internal.h"
#include "win32_platform.h" // get_memory_info()

#include <faultline/fl_test.h>                     // FLTestCase, fl_default_setup
#include <faultline/fl_try.h>                      //FL_THROW
#include <faultline/fl_exception_service_assert.h> //FL_ASSERT()
#include <faultline/fl_math.h>                     // SUM_OVER_SCALED_RANGE
#include <faultline/fl_abbreviated_types.h>        // u32

#include <stddef.h> // size_t, NULL

void print_comma2(size_t n) {
    if (n < 1000) {
        printf("%zd", n);
        return;
    }
    print_comma2(n / 1000);
    printf(",%03zd", n % 1000);
}

void print_comma(size_t n) {
    if (n < 0) {
        printf("-");
        n = 0 - n;
    }
    print_comma2(n);
}

void dbg_display_range(u32 idx, size_t range, size_t low, size_t mid, size_t high) {
    printf("\n");
    printf("    DBG: idx %u\n", idx);
    printf("    DBG: range  ");
    print_comma(range);
    printf("\n");
    printf("    DBG: low    ");
    print_comma(low);
    printf("\n");
    printf("    DBG: mid    ");
    print_comma(mid);
    printf("\n");
    printf("    DBG: high   ");
    print_comma(high);
    printf("\n");
}

void dbg_display_allocate_arena(size_t high) {
    printf("\n");
    printf("\n");
    printf("    DBG: Allocated an arena with ");
    print_comma(high);
    printf(" bytes\n");
}

FL_TEST("Macros", test_arena_macros) {
    size_t max_chunk_size = CHUNK_SIZE_FROM_REQUEST(ARENA_MAX_SMALL_REQUEST);

    FL_ASSERT_DETAILS(ARENA_SMALL_INDEX(max_chunk_size) == ARENA_SMALL_BIN_COUNT - 1,
                      "Expected %zd. Actual %zd", ARENA_SMALL_BIN_COUNT - 1,
                      ARENA_SMALL_INDEX(max_chunk_size));
}

/**
 * @brief iterate through a range of reserved blocks
 */
FL_TEST("Initialization Size", test_initialize_size) {
    Arena    *arena;
    u32       page_size;
    size_t    request  = 0;
    u16       exponent = 0;
    u16 const log2_8GB = 33; // 2^33 is 8 GB

    /**
     * @brief I feel I should be able to use MEMORYSTATUSEX to enable testing
     * of ranges of reserved and committed chunks of memory.
     *
     * MEMORYSTATUSEX mem_status;
     *
     *    mem_status.dwLength = sizeof mem_status;
     *    BOOL success = GlobalMemoryStatusEx(&mem_status);
     *
     *    if (success) {
     *        printf("Length: %d\n", mem_status.dwLength);
     *        printf("Load: %d\n", mem_status.dwMemoryLoad);
     *        printf("Total Physical: %llu\n", mem_status.ullTotalPhys);
     *        printf("Avail Physical: %llu\n", mem_status.ullAvailPhys);
     *        printf("Total Page File: %llu\n", mem_status.ullTotalPageFile);
     *        printf("Avail Page File: %llu\n", mem_status.ullAvailPageFile);
     *        printf("Total Virtual: %llu\n", mem_status.ullTotalVirtual);
     *        printf("Avail Virtual: %llu\n", mem_status.ullAvailVirtual);
     *        printf("Avail Extended Virtual: %llu\n",
     *               mem_status.ullAvailExtendedVirtual);
     *    } else {
     *        printf("Failed to get
     *        global memory status: 0x%0x\n", GetLastError());
     *    }
     */
    get_memory_info(0, &page_size);
    arena = new_arena(request, 0);
    // test new_arena reserving space for requests for 0 bytes and then starting
    // with 2 bytes and doubling each iteration to a maximum of 8GB.
    for (exponent = 1; arena != 0 && exponent <= log2_8GB; exponent++) {
        size_t expected_size
            = ALIGN_UP(request + ARENA_ALIGNED_SIZE + REGION_ALIGNED_SIZE,
                       (size_t)page_size);
        // assert that result is the smallest multiple of page_size that is
        // greater than request.
        size_t actual_size = REGION_BYTES_COMMITTED(MEM_TO_REGION(arena));

        FL_ASSERT_DETAILS(actual_size == expected_size,
                          "[exponent=%hu, request=0x%llx]: unexpected size; expected: "
                          "0x%016llx, actual: 0x%016llx",
                          exponent, request, expected_size, actual_size);

        FL_ASSERT_DETAILS(arena->initialized,
                          "[exponent=%hu, request=0x%llx]: arena at %p is uninitialized",
                          exponent, request, arena);

        FL_ASSERT_DETAILS(arena->small_map == 0,
                          "[exponent=%hu, request=0x%llx]: invalid initialization for "
                          "small-bin map; expected: 1, actual: 0x%0llx",
                          exponent, request, arena->small_map);

        FL_ASSERT_DETAILS(arena->large_map == 0,
                          "[exponent=%hu, request=0x%llx]: invalid initialization for "
                          "tree-bin map; expected: 1, actual: 0x%0llx",
                          exponent, request, arena->large_map);

        release_arena(&arena);
        request = 1ULL << exponent;
        arena   = new_arena(request, 0);
    }

    release_arena(&arena);
}

typedef struct {
    FLTestCase tc;
    size_t     fast_size;
    size_t     top_size;
    size_t     footprint;
} TestState;

static FLExceptionReason const invalid_allocation_size = "invalid allocation size";
static FLExceptionReason const invalid_top             = "invalid top";
static FLExceptionReason const invalid_small_map       = "invalid bitmap for small bins";
static FLExceptionReason const invalid_large_map       = "invalid bitmap for tree bins";
static FLExceptionReason const invalid_small_bins      = "invalid small bins address";
static FLExceptionReason const invalid_large_bins      = "invalid tree bins address";
static FLExceptionReason const invalid_least_address   = "invalid least address";
static FLExceptionReason const invalid_fast_size       = "invalid size of fast node";
static FLExceptionReason const invalid_top_size        = "invalid top size";
static FLExceptionReason const invalid_footprint       = "invalid footprint";

void test_exception_64(u64 actual, u64 expected, char const *file, int line) {
    FL_ASSERT_DETAILS_FILE_LINE(actual == expected, "Expected %llu. Actual %llu", file,
                                line, expected, actual);
}

void test_exception_pointer(ptrdiff_t actual, ptrdiff_t expected, char const *file,
                            int line) {
    FL_ASSERT_DETAILS_FILE_LINE(actual == expected, "Expected 0x%zu. Actual 0x%zu", file,
                                line, expected, actual);
}

/**
 * @brief validate the state of an Arena. If a field isn't set as expected, then throw an
 * exception.
 *
 * @param arena the address of the Arena under test.
 * @param state the address of the TestState data used to validate arena.
 */
void validate_arena_state(Arena *arena, TestState *state) {
    FL_ASSERT_TRUE(arena->initialized);

    test_exception_pointer(arena->small_map, 0, __FILE__, __LINE__);
    test_exception_pointer(arena->large_map, 0, __FILE__, __LINE__);

    test_exception_64(arena->fast_size, state->fast_size, __FILE__, __LINE__);
    test_exception_64(CHUNK_SIZE(arena->top), state->top_size, __FILE__, __LINE__);
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE(arena->top));
    test_exception_pointer((ptrdiff_t)DLIST_PREVIOUS(arena->small_bins),
                           (ptrdiff_t)DLIST_NEXT(arena->small_bins), __FILE__, __LINE__);
    test_exception_64(arena->footprint, state->footprint, __FILE__, __LINE__);
}

static void setup_initialization(FLTestCase *btc) {
    TestState *state      = FL_CONTAINER_OF(btc, TestState, tc);
    size_t     arena_size = sizeof(Arena);
    u32        page_size;

    get_memory_info(0, &page_size);
    size_t footprint = ALIGN_UP(arena_size, page_size);
    state->fast_size = 0;
    state->top_size
        = footprint - REGION_ALIGNED_SIZE - ARENA_ALIGNED_SIZE - CHUNK_SENTINEL_SIZE;
    state->footprint = footprint;
}

/**
 * @brief create Arena objects and validate they are
 * initialized correctly.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Initialization", TestState, test_initialization,
                           setup_initialization, fl_default_cleanup) {
    Arena       *arena;
    size_t       reserved;
    size_t const min_allocation = sizeof(Arena);

    arena = new_arena(0, 0);
    FL_TRY {
        reserved = REGION_BYTES_RESERVED(MEM_TO_REGION(arena));
        FL_ASSERT_DETAILS(reserved >= min_allocation,
                          "Expected allocation size > %llu. Actual %llu", min_allocation,
                          reserved);
        validate_arena_state(arena, t);
    }
    FL_FINALLY {
        // release the allocated memory.
        release_arena(&arena);
    }
    FL_END_TRY;
}

/**
 * @brief verify that the macro ARENA_SMALL_INDEX produces the correct index for small
 * bins.
 */
FL_TEST("Small Size to Index", test_chunk_size_to_index) {
    u32    actual;
    u32    expected;
    size_t chunk_size;

    for (size_t req = 0; req <= ARENA_MAX_SMALL_REQUEST; req++) {
        chunk_size = CHUNK_SIZE_FROM_REQUEST(req);
        actual     = ARENA_SMALL_INDEX(chunk_size);

        size_t payload = CHUNK_REQUEST_TO_PAYLOAD(req);
        FL_ASSERT_DETAILS(
            actual < ARENA_SMALL_BIN_COUNT,
            "Request %zd. Expected index < %lld. Actual %d. sizeof(Chunk) %zd. "
            "Chunk size %zd. CHUNK_MIN_PAYLOAD %zd, CHUNK_MIN_SIZE %zd, "
            "ARENA_MAX_SMALL_CHUNK %zd, ARENA_MAX_SMALL_REQUEST %zd. Aligned Header "
            "Size %zd. Payload %zd. Footer size %zd",
            req, ARENA_SMALL_BIN_COUNT, actual, sizeof(Chunk), chunk_size,
            CHUNK_MIN_PAYLOAD, CHUNK_MIN_SIZE, ARENA_MAX_SMALL_CHUNK,
            ARENA_MAX_SMALL_REQUEST, CHUNK_ALIGNED_SIZE, payload, CHUNK_FOOTER_SIZE);

        FL_ASSERT_DETAILS(chunk_size < ARENA_MIN_LARGE_CHUNK,
                          "Request %zd. Expected chunk size to be less than %d. Actual "
                          "%lld. Min large-bin size %d, Max small chunk %zd, Max small "
                          "request %zd, Small-bin count %zd",
                          req, ARENA_MIN_LARGE_CHUNK, chunk_size, ARENA_MIN_LARGE_CHUNK,
                          ARENA_MAX_SMALL_CHUNK, ARENA_MAX_SMALL_REQUEST,
                          ARENA_SMALL_BIN_COUNT);

        expected = req <= CHUNK_MIN_PAYLOAD
                       ? 0
                       : (u32)(ALIGN_UP(req, CHUNK_ALIGNMENT) / CHUNK_ALIGNMENT
                               - CHUNK_MIN_PAYLOAD / CHUNK_ALIGNMENT);
        FL_ASSERT_DETAILS(actual == expected,
                          "Expected %d for request %zd (chunk size %zd). Actual %d. "
                          "(CHUNK_MIN_PAYLOAD %zd), (CHUNK_MIN_SIZE %zd), "
                          "(ARENA_MAX_SMALL_REQUEST %zd).",
                          expected, req, chunk_size, actual, CHUNK_MIN_PAYLOAD,
                          CHUNK_MIN_SIZE, ARENA_MAX_SMALL_REQUEST);
    }
}

/**
 * @brief verify that the macro ARENA_SMALL_INDEX_TO_SIZE(i) returns the correct size of
 * the chunks in the bin at the given index.
 */
FL_TEST("Small Index to Size", test_small_index_to_chunk_size) {
    size_t actual;
    size_t expected;

    for (u32 idx = 0; idx < ARENA_SMALL_BIN_COUNT; idx++) {
        actual = ARENA_SMALL_INDEX_TO_SIZE(idx);
        FL_ASSERT_DETAILS(actual <= ARENA_MAX_SMALL_CHUNK,
                          "Index %d. Expected size <= %zd. Actual %zd", idx,
                          ARENA_MAX_SMALL_CHUNK, actual);

        expected = CHUNK_MIN_SIZE + CHUNK_ALIGNMENT * idx;
        FL_ASSERT_DETAILS(actual == expected, "Index %d. Expected %zd. Actual %zd", idx,
                          expected, actual);
    }
}

/**
 * @brief test ARENA_SMALL_BIN_INCREMENT for non-zero values.
 *
 * If bit 0 is set, return 0 regardless of the other bits. Otherwise, if bit 1 is set
 * return 1 regardless of the value of bit 2. Otherwise, return 2.
 */
FL_TEST("Select Small Bin Index", test_select_small_bins) {
    u32 expected;
    u32 actual;

    // ARENA_SMALL_BIN_INCREMENT(i) is valid only for non-zero indices.
    for (u32 i = 0; i < U64_BIT; i++) {
        actual = ARENA_SMALL_BIN_INCREMENT(i);
        // check bits 0, 1, 2, and 3 in order.
        expected = ARENA_SBM(i) == 0 ? 0
                   : (i & 0x1)       ? 0
                                     : ((i & 0x2) ? 1
                                                  : ((i & 0x4)   ? 2
                                                     : (i & 0x8) ? 3
                                                                 : 0));
        FL_ASSERT_DETAILS(actual == expected, "Index %d. Expected %d. Actual %d", i,
                          expected, actual);
    }
}

typedef struct {
    FLTestCase tc;
    Arena     *arena;
} ArenaTestSmall;

/**
 * @brief allocate an arena for testing allocations of chunks up to ARENA_MAX_SMALL_CHUNK
 * bytes.
 *
 * @param btc the address of a ArenaTestSmall whose first member is a FLTestCase.
 */
static void setup_test_arena_max_small(FLTestCase *btc) {
    ArenaTestSmall *ta    = FL_CONTAINER_OF(btc, ArenaTestSmall, tc);
    Arena          *arena = new_arena(ARENA_MAX_SMALL_CHUNK, 0);
    ta->arena             = arena;
}

/**
 * @brief release the resources acquired by setupTestArena255
 *
 * @param btc
 */
static void cleanup_test_arena(FLTestCase *btc) {
    ArenaTestSmall *ta = FL_CONTAINER_OF(btc, ArenaTestSmall, tc);
    release_arena(&ta->arena);
}

/**
 * @brief allocate (from top) a range of requests from 0 to ARENA_MAX_SMALL_REQUEST bytes
 * and verify that the resulting chunk sizes are as expected.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Small Range", ArenaTestSmall, test_small_range,
                           setup_test_arena_max_small, cleanup_test_arena) {
    FreeChunk   *ch           = t->arena->top;  // chunk of unallocated space
    size_t const initial_size = CHUNK_SIZE(ch); // chunk's initial size
    size_t       actual, expected;
    size_t       remainder;

    for (size_t req = 0; req <= ARENA_MAX_SMALL_REQUEST; req++) {
        size_t     chunk_size = CHUNK_SIZE_FROM_REQUEST(req);
        FreeChunk *rem        = free_chunk_split(ch, chunk_size, __FILE__, __LINE__);
        actual                = CHUNK_SIZE(ch);
        remainder             = initial_size - actual;
        FL_ASSERT_DETAILS(actual >= req, "chunk size. Expected at least %zd. Actual %zd",
                          req, actual);

        // either the initial chunk wasn't resized, so the "resized" chunk should be the
        // initial size, or it was resized so the "resized" chunk should be the same size
        // as that derived from the request.
        expected = CHUNK_SIZE_FROM_REQUEST(req);
        if (remainder == 0) {
            FL_ASSERT_DETAILS(actual == initial_size, "Expected %zd. Actual %zd",
                              initial_size, actual);
            FL_ASSERT_DETAILS(rem == NULL, "Expected NULL remainder. Actual 0x%p", rem);

        } else {
            FL_ASSERT_DETAILS(actual == expected,
                              "Expected %zd. Actual %zd, remainder %zd", expected,
                              actual, remainder);
        }

        // ensure that the size is always at least CHUNK_MIN_SIZE.
        expected = CHUNK_MIN_SIZE;
        FL_ASSERT_DETAILS(
            CHUNK_SIZE(ch) >= expected,
            "chunk too small. Expected at least %zd bytes. Actual %zd bytes", expected,
            CHUNK_SIZE(ch));

        // If the chunk was split, validate the size of the next chunk, and merge the
        // remainder back into the initial chunk and validate the size of the initial
        // chunk is restored.
        if (actual < initial_size) {
            size_t next_chunk_size = CHUNK_SIZE(CHUNK_NEXT(ch));

            FL_ASSERT_DETAILS(next_chunk_size == remainder,
                              "unexpected size for next chunk. Initial size %zd. "
                              "Expected %zd. Actual %zd",
                              initial_size, remainder, next_chunk_size);
            free_chunk_merge_next(ch);
        }

        FL_ASSERT_DETAILS(CHUNK_SIZE(ch) == initial_size,
                          "merge failed. Expected %zd. Actual %zd", initial_size,
                          CHUNK_SIZE(ch));
    }
}

/**
 * @brief verify the values of ARENA_MIN_LARGE_CHUNK and ARENA_MAX_SMALL_CHUNK, and that
 * ARENA_SMALL_INDEX(ARENA_MIN_LARGE_CHUNK) returns the number of small bins.
 */
FL_TEST("Minimum Large Bin", test_min_large_size) {
    // the smallest chunk in a large bin should be the size of the largest small-bin
    // chunk plus the width of a small bin (CHUNK_ALIGNMENT).
    FL_ASSERT_DETAILS(ARENA_MIN_LARGE_CHUNK == ARENA_MAX_SMALL_CHUNK + CHUNK_ALIGNMENT,
                      "Expected %zu. Actual %u.",
                      ARENA_MAX_SMALL_CHUNK + CHUNK_ALIGNMENT, ARENA_MIN_LARGE_CHUNK);

    // detect off-by-one errors - the small-bin index of the smallest large-bin chunk
    // should be equal to the number of small bins.
    size_t actual = ARENA_SMALL_INDEX(ARENA_MIN_LARGE_CHUNK);
    FL_ASSERT_DETAILS(actual == ARENA_SMALL_BIN_COUNT, "Expected %zd. Actual %zd",
                      ARENA_SMALL_BIN_COUNT, actual);
}

/**
 * @brief test mapping large chunks to an index.
 *
 * It's completely impractical to test every chunk size, so test just the low, middle,
 * and high sizes expected for each bin.
 */
FL_TEST("Map Large Chunks to Index", test_large_chunk_to_index) {
    size_t    range     = ARENA_MIN_LARGE_CHUNK >> 1;    // size-range for a bin
    size_t    low       = ARENA_MIN_LARGE_CHUNK;         // smallest size for the bin
    size_t    mid       = low + (range >> 1);            // midpoint size
    size_t    high      = low + range - CHUNK_ALIGNMENT; // greatest size
    u32 const max_index = (u32)ARENA_LARGE_BIN_COUNT;

    // printf("| Bin Index |            Chunk Size |\n");
    // printf("| --------: | ---------------------:|\n");
    for (u32 idx = 0; idx < max_index; idx++) {
        u32 actual;

        // dbg_display_range is useful for debugging and to see just how wide large bins
        // can be.
        // dbg_display_range(idx, range, low, mid, high);

        // test low
        INDEX_BY_VALUE64(low, max_index, ARENA_LOG2_MIN_LARGE_CHUNK, actual);
        FL_ASSERT_DETAILS(
            idx == actual,
            "Expected %u. Actual %u\n        log2: %d\n        range: %zd\n        "
            "low:   %zd\n        mid:   %zd\n        high:  %zd\n        bins:  %u",
            (u32)ARENA_LOG2_MIN_LARGE_CHUNK, idx, actual, range, low, mid, high,
            max_index);

        // test mid
        INDEX_BY_VALUE64(mid, max_index, ARENA_LOG2_MIN_LARGE_CHUNK, actual);
        FL_ASSERT_DETAILS(idx == actual,
                          "Expected %u. Actual %u\n    range: %zd\n    low:   %zd\n    "
                          "mid:   %zd\n    high:  %zd\n    bins:  %u",
                          idx, actual, range, low, mid, high, max_index);

        // test high
        INDEX_BY_VALUE64(high, max_index, ARENA_LOG2_MIN_LARGE_CHUNK, actual);
        FL_ASSERT_DETAILS(idx == actual,
                          "Expected %u. Actual %u\n    range: %zd\n    low:   %zd\n    "
                          "mid:   %zd\n    high:  %zd\n    bins:  %u",
                          idx, actual, range, low, mid, high, max_index);

        // set low to the baseline size of the next bin
        low += range;
        // double the range on odd indices, because bin widths double every other bin.
        range = idx & 0x1 ? range << 1 : range;
        mid   = low + (range >> 1);
        high  = low + range - CHUNK_ALIGNMENT;
    }
}

static void setup_small_from_large_arena_test(FLTestCase *btc) {
    ArenaTestSmall *at = FL_CONTAINER_OF(btc, ArenaTestSmall, tc);
    size_t          size;
    size_t          lower   = INDEX_BIN_TO_LOWER_LIMIT(0, ARENA_LOG2_MIN_LARGE_CHUNK);
    size_t          upper   = INDEX_BIN_TO_UPPER_LIMIT(0, ARENA_LOG2_MIN_LARGE_CHUNK);
    u32             cnt     = (u32)(upper - lower) / CHUNK_ALIGNMENT;
    size_t          request = lower - CHUNK_ALIGNED_SIZE; // smallest request

    SUM_OVER_SCALED_RANGE(lower, upper, CHUNK_ALIGNMENT, size);
    at->arena  = new_arena(size + cnt * sizeof(void *), 0);
    void **mem = arena_malloc_throw(at->arena, cnt * sizeof(void *), __FILE__, __LINE__);

    for (u32 i = 0; i < cnt; i++) {
        FL_ASSERT_DETAILS(
            CHUNK_SIZE_FROM_REQUEST(request) == lower,
            "i=%d, hdr=%zd, align=%zd, request %zd: Expected %zd. Actual %zd", i,
            CHUNK_ALIGNED_SIZE, CHUNK_ALIGNMENT, request, lower,
            CHUNK_SIZE_FROM_REQUEST(request));
        mem[i] = arena_malloc_throw(at->arena, request, __FILE__, __LINE__);
        FL_ASSERT_EQ_SIZE_T(lower, CHUNK_SIZE(CHUNK_FROM_MEMORY(mem[i])));
        lower += CHUNK_ALIGNMENT;
        request = lower - CHUNK_ALIGNED_SIZE;
    }

    FreeChunk *ch;
    Chunk     *next;
    for (u32 i = 0; i < cnt; i++) {
        ch   = FREE_CHUNK_FROM_MEMORY(mem[i], __FILE__, __LINE__);
        next = CHUNK_NEXT(ch);
        FL_ASSERT_DETAILS(i == 0 || at->arena->large_map != 0,
                          "i=%d, large map is zero!", i);

        arena_free_throw(at->arena, mem[i], __FILE__, __LINE__);
        Arena *arena = at->arena;
        // when merged to top, the footer is not set. Otherwise, validate the address of
        // the footer is just prior the next chunk.
        FL_ASSERT_DETAILS(
            ch == arena->top || free_chunk_get_footer(ch) == chunk_previous_footer(next),
            "chunk: 0x%08zx, next: 0x%08zx. Expected 0x%08zx. Actual 0x%08zx",
            ARENA_RELATIVE_ADDRESS(at->arena, ch),
            ARENA_RELATIVE_ADDRESS(at->arena, next),
            (char *)chunk_previous_footer(next) - (char *)arena->base,
            (char *)free_chunk_get_footer(ch) - (char *)arena->base);
    }
}

/**
 * @brief ensure there's enough space in the arena to allocate ARENA_SMALL_BIN_COUNT
 * chunks, each whose size is ARENA_MAX_SMALL_CHUNK.
 */
void setup_small_bins(FLTestCase *btc) {
    ArenaTestSmall *at    = FL_CONTAINER_OF(btc, ArenaTestSmall, tc);
    size_t          bytes = ARENA_SMALL_BIN_COUNT * ARENA_MAX_SMALL_CHUNK;
    at->arena             = new_arena(bytes, 0);
}

static void cleanup_small_bins(FLTestCase *btc) {
    ArenaTestSmall *at = FL_CONTAINER_OF(btc, ArenaTestSmall, tc);
    release_arena(&at->arena);
}

FL_TYPE_TEST_SETUP_CLEANUP("Allocate Small Request from Large Bin", ArenaTestSmall,
                           test_allocate_small_request_from_large_bin,
                           setup_small_from_large_arena_test, cleanup_small_bins) {
    Arena *arena = t->arena;
    Chunk *chunk;
    void  *mem;

    // make a loop to call allocate_small_request_from_large_bin and verify that the
    // returned Chunk is sized and configured as expected or throw an internal-error
    // exception.
    for (size_t req = 0; req <= ARENA_MAX_SMALL_REQUEST; req++) {
        if (arena->large_map != 0) {
            chunk
                = allocate_small_request_from_large_bin(arena, req, __FILE__, __LINE__);
            FL_ASSERT_DETAILS(CHUNK_SIZE(chunk) % CHUNK_ALIGNMENT == 0,
                              "Expected %zd to have %zd alignment.", CHUNK_SIZE(chunk),
                              CHUNK_ALIGNMENT);
            mem = CHUNK_TO_MEMORY(chunk);
        } else {
            mem   = arena_malloc_throw(arena, req, __FILE__, __LINE__);
            chunk = CHUNK_FROM_MEMORY(mem);
            ARENA_RTCHECK(arena_address_ok(arena, chunk), __FILE__, __LINE__);
        }
        arena_free_throw(arena, mem, __FILE__, __LINE__);
    }
}

/**
 * @brief test allocate_exact_fit and insert_small_chunk.
 *
 * First verify the small-bin map and all small bins are initialized correctly.
 *
 * Next, allocate chunks such that there is one for each small bin.
 *
 * Once all of the chunks are allocated, insert each one into a small bin. Each time a
 * chunk is inserted into a bin, verify that the bin is not empty and its bit in the
 * small bin map is set.
 *
 * Allocate chunks again, but this time they should be pulled from small bins. For each
 * allocation, verify that the corresponding bin is empty and that its bit in the small
 * bin map is unset.
 *
 * Finally, check that the map indicates there are no chunks stored and that each bin is
 * empty.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Small Exact-Fit Allocation", ArenaTestSmall,
                           test_small_bins_exact_fit, setup_small_bins,
                           cleanup_small_bins) {
    Arena     *arena = t->arena;
    FreeChunk  head;
    DList     *bin;
    FreeChunk *ch;
    u32        idx;

    // initialize the list to hold our allocated chunks
    free_chunk_init(&head, sizeof head, false);

    // assert initial conditions for small_map and each small bin.
    FL_ASSERT_DETAILS(arena->small_map == 0, "arena's small bin map not empty");

    for (u32 i = 0; i < ARENA_SMALL_BIN_COUNT; i++) {
        bin = ARENA_SMALL_BIN_AT(arena, i);
        FL_ASSERT_DETAILS(DLIST_IS_EMPTY(bin), "bin %d is not empty", i);
    }

    // use allocation requests in the middle of each bin's range
    size_t sz = CHUNK_SIZE_FROM_REQUEST(sizeof(DList)); // sized for bin 0
    ch        = (FreeChunk *)allocate_from_top(arena, sz, __FILE__, __LINE__);

    // write to a small part of the chunk to simulate it being in use and to
    // dispel any lingering spirits of invalid memory address.
    char *mem = CHUNK_TO_MEMORY(ch);
    for (u32 j = 0; j < CHUNK_MIN_PAYLOAD; j++) {
        mem[j] = 0;
    }

    // before a used chunk can be put on a list, its internal data must be reset.
    free_chunk_init_simple(ch);
    FL_ASSERT_DETAILS(*free_chunk_get_footer(ch) == CHUNK_SIZE(ch),
                      "Chunk size %zd. Footer size %zd", CHUNK_SIZE(ch),
                      *free_chunk_get_footer(ch));

    free_chunk_insert(&head, ch);

    // allocate the rest of the chunks, one per small bin.
    sz += CHUNK_ALIGNMENT; // sized for all succeeding bins
    for (; sz < ARENA_MAX_SMALL_REQUEST; sz += CHUNK_ALIGNMENT) {
        ch = (FreeChunk *)allocate_from_top(arena, sz, __FILE__, __LINE__);
        // use the allocated memory
        mem = CHUNK_TO_MEMORY(ch);
        for (u32 j = 0; j < CHUNK_MIN_PAYLOAD; j++) {
            mem[j] = 0;
        }

        free_chunk_init_simple(ch);
        FL_ASSERT_DETAILS(*free_chunk_get_footer(ch) == CHUNK_SIZE(ch),
                          "Chunk size %zd. Footer size %zd", CHUNK_SIZE(ch),
                          *free_chunk_get_footer(ch));

        free_chunk_insert(&head, ch);
    }

    // insert each chunk into a small bin; check the state of the map and the bin both
    // before and after insertion.
    while (free_chunk_has_siblings(&head)) {
        ch = free_chunk_next_sibling(&head);
        free_chunk_remove(ch);
        sz  = CHUNK_SIZE(ch);
        idx = ARENA_SMALL_INDEX(sz);

        insert_small_chunk(arena, ch, __FILE__, __LINE__);

        FL_ASSERT_DETAILS(ARENA_SMALL_MAP_IS_MARKED(arena, idx),
                          "small bin map is not set for index %d and chunk size %zd",
                          idx, sz);

        bin = ARENA_SMALL_BIN_AT(arena, idx);
        FL_ASSERT_DETAILS(!DLIST_IS_EMPTY(bin), "bin %d is empty", idx);
    }

    // allocate small chunks again, but this time they should all be pulled from small
    // bins.
    sz  = CHUNK_SIZE_FROM_REQUEST(sizeof(DList)); // sized for bin 0
    idx = ARENA_SMALL_INDEX(sz);
    FL_ASSERT_DETAILS(idx < ARENA_SMALL_BIN_COUNT, "bin index %d out of range", idx);

    Chunk *allocated = allocate_exact_fit(arena, sz, __FILE__, __LINE__);
    ch               = chunk_free(allocated);
    free_chunk_insert(&head, ch);

    FL_ASSERT_DETAILS(!ARENA_SMALL_MAP_IS_MARKED(arena, idx),
                      "small bin map should be unset, but is set for index %d and "
                      "chunk size %zd",
                      idx, sz);

    bin = ARENA_SMALL_BIN_AT(arena, idx);
    FL_ASSERT_DETAILS(DLIST_IS_EMPTY(bin), "bin %d is not empty", idx);

    // allocate the chunks from all succeeding bins
    for (sz += CHUNK_ALIGNMENT; sz < ARENA_MAX_SMALL_REQUEST; sz += CHUNK_ALIGNMENT) {
        idx = ARENA_SMALL_INDEX(sz);
        FL_ASSERT_DETAILS(idx < ARENA_SMALL_BIN_COUNT, "bin index %d out of range", idx);

        allocated = allocate_exact_fit(arena, sz, __FILE__, __LINE__);
        ch        = chunk_free(allocated);
        free_chunk_insert(&head, ch);

        FL_ASSERT_DETAILS(!ARENA_SMALL_MAP_IS_MARKED(arena, idx),
                          "small bin map should be unset, but is set for index %d and "
                          "chunk size %zd",
                          idx, sz);

        bin = ARENA_SMALL_BIN_AT(arena, idx);
        FL_ASSERT_DETAILS(DLIST_IS_EMPTY(bin), "bin %d is not empty", idx);
    }

    // one more check of the map and each bin to verify there are no chunks stored there.
    FL_ASSERT_DETAILS(arena->small_map == 0, "arena's small bin map not empty");

    for (u32 i = 0; i < ARENA_SMALL_BIN_COUNT; i++) {
        bin = ARENA_SMALL_BIN_AT(arena, i);
        FL_ASSERT_DETAILS(DLIST_IS_EMPTY(bin), "bin %d is not empty", i);
    }
}

typedef struct {
    FLTestCase tc;
    Arena     *arena;
    u32        max_index;
    size_t     max_size;
} ArenaTestLarge;

/**
 * @brief ensure there's enough space in the arena to allocate a low, mid, and high chunk
 * for each large bin in the arena.
 */
static void setup_large_bins(FLTestCase *btc) {
    ArenaTestLarge *at        = FL_CONTAINER_OF(btc, ArenaTestLarge, tc);
    u32             idx       = 0;
    u32 const       max_index = (u32)ARENA_LARGE_BIN_COUNT - 1;
    size_t          range     = ARENA_MIN_LARGE_CHUNK >> 1; // size-range for a bin
    size_t          low       = ARENA_MIN_LARGE_CHUNK;      // smallest size for the bin
    // size_t          mid       = low + (range >> 1);         // midpoint size
    size_t high = low + range - CHUNK_ALIGNMENT; // greatest size

    for (idx = 0; idx < max_index; idx++) {
        // dbg_display_range(idx, range, low, mid, high);
        //  set low to the baseline size of the next bin
        low += range;
        // double the range on odd indices, because bin widths double every other bin.
        range = idx & 0x1 ? range << 1 : range;
        // mid   = low + (range >> 1);
        high = low + range - CHUNK_ALIGNMENT;
    }
    // dbg_display_range(idx, range, low, mid, high);

    Arena *arena = NULL;
    while (arena == NULL && idx > 0) {
        FL_TRY {
            arena = new_arena(high, 0);
            // dbg_display_allocate_arena(high);
        }
        FL_CATCH(region_out_of_memory) {
            idx--;
            high -= range;
            // mid   = high - (range >> 1);
            range = idx & 0x1 ? range >> 1 : range;
            low   = high - range;
        }
        FL_END_TRY;
    }

    FL_ASSERT_DETAILS(arena != NULL, "Failed to allocate an arena");

    at->arena     = arena;
    at->max_index = idx;
    at->max_size  = high;
}

static void cleanup_large_bins(FLTestCase *btc) {
    ArenaTestLarge *at = FL_CONTAINER_OF(btc, ArenaTestLarge, tc);
    release_arena(&at->arena);
}

FL_TYPE_TEST_SETUP_CLEANUP("Large Chunk Allocation", ArenaTestLarge, test_large_bins,
                           setup_large_bins, cleanup_large_bins) {
    Arena              *arena = t->arena;
    DigitalSearchTree **bin;

    // assert initial conditions for large map and each large bin.
    FL_ASSERT_DETAILS(arena->large_map == 0, "arena's large bin map not empty");

    for (u32 i = 0; i < ARENA_LARGE_BIN_COUNT; i++) {
        bin = ARENA_LARGE_BIN_AT(arena, i);
        FL_ASSERT_DETAILS(*bin == NULL, "bin %d is not empty", i);
    }

    FL_ASSERT_DETAILS(CHUNK_SIZE(arena->top) >= t->max_size,
                      "top size %zd, expected at least %zd", CHUNK_SIZE(arena->top),
                      t->max_size);

    // size-range for a bin
    size_t range = ARENA_MIN_LARGE_CHUNK >> 1;
    // smallest request for the bin
    size_t low  = ARENA_MIN_LARGE_CHUNK - CHUNK_ALIGNED_SIZE - (CHUNK_ALIGNMENT - 1);
    size_t mid  = low + (range >> 1);     // midpoint request
    size_t high = mid + (range >> 1) - 1; // greatest request
    size_t const top_size = CHUNK_SIZE(arena->top);
    u32 const    indicies = t->max_index + 1;
    for (u32 idx = 0; idx < indicies; idx++) {
        for (int i = 0; i < 3; i++) {
            size_t size;
            size_t req = 0;
            switch (i) {
            case 0:
                req = low;
                break;
            case 1:
                req = mid;
                break;
            case 2:
                req = high;
                break;
            }

            size                 = CHUNK_SIZE_FROM_REQUEST(req);
            FreeChunk *ch        = arena->top;
            FreeChunk *remainder = free_chunk_split(ch, size, __FILE__, __LINE__);
            if (remainder == NULL) {
                arena->top = (FreeChunk *)CHUNK_NEXT(arena->top); // the sentinel
            } else {
                arena->top = remainder;
            }
            dst_init_minimal(ARENA_CHUNK_TO_DST(ch));
            DigitalSearchTree *tree = ARENA_CHUNK_TO_DST(ch);

            // write to a small part of the chunk to simulate it being in use and to
            // dispel any lingering spirits of invalid memory address.
            char *mem = CHUNK_TO_MEMORY(tree);
            for (u32 j = 0; j < CHUNK_MIN_PAYLOAD; j++) {
                mem[j] = 0;
            }

            // calculate the expected index
            INDEX_BY_VALUE64(size, indicies, ARENA_LOG2_MIN_LARGE_CHUNK, idx);

            // get the address of the tree from mem
            DigitalSearchTree *tree_free = ARENA_CHUNK_TO_DST(CHUNK_FROM_MEMORY(mem));
            FL_ASSERT_EQ_PTR(tree_free, tree);

            // Just like freeing a Chunk, when a tree is to be freed, its internal fields
            // (child, parent, and index) must be set.
            dst_init_minimal(tree);

            FL_ASSERT_DETAILS(*free_chunk_get_footer((FreeChunk *)tree)
                                  == CHUNK_SIZE(tree),
                              "Tree size %zd. Footer size %zd", CHUNK_SIZE(tree),
                              *free_chunk_get_footer((FreeChunk *)tree));

            // "free" the chunk by putting it in a large bin
            ARENA_RTCHECK(arena_address_ok(arena, tree), __FILE__, __LINE__);
            insert_large_chunk(arena, tree, __FILE__, __LINE__);

            FL_ASSERT_DETAILS((arena->large_map & 1ull << idx) != 0,
                              "expected map bit set at %d, actual map 0x%016zu", idx,
                              arena->large_map);

            DigitalSearchTree *actual;
            actual = ARENA_CHUNK_TO_DST(allocate_from_large_bin(arena, req, __FILE__,
                                                                __LINE__));
            FL_ASSERT_DETAILS(actual != NULL,
                              "unexpected out-of-memory error, request %zu, size %zu",
                              req, size);

            FL_ASSERT_DETAILS(arena->large_map == 0,
                              "expected map bit set at %d, actual map 0x%016zu", idx,
                              arena->large_map);

            FL_ASSERT_DETAILS(*bin == NULL, "expected bin to be NULL, actual 0x%016p",
                              *bin);

            FL_ASSERT_DETAILS(tree == actual, "expected 0x%016p, actual 0x%016p", tree,
                              actual);

            FreeChunk *expected = FREE_CHUNK_NEXT(ARENA_DST_TO_FREE_CHUNK(tree));
            FL_ASSERT_DETAILS(expected == arena->top,
                              "expected top 0x%016p, actual 0x%016p", expected,
                              arena->top);

            free_chunk_merge_next(ARENA_DST_TO_FREE_CHUNK(tree));
            FL_ASSERT_DETAILS(CHUNK_SIZE(tree) == top_size,
                              "expected merged top size %zu, actual %zu", top_size,
                              CHUNK_SIZE(tree));

            // restore the arena
            arena->top = ch;
        }

        // set low to the baseline size of the next bin
        low += range;
        // double the range on odd indices, because bin widths double every other
        // bin.
        range = idx & 0x1 ? range << 1 : range;
        mid   = low + (range >> 1);
        high  = mid + (range >> 1) - 1;
    }
}

typedef struct {
    FLTestCase tc;
    Arena     *arena[2];
} ArenaRegionNode;

/**
 * @brief Set up the test for region node allocation.
 *
 * Allocate a single arena with a small amount of space and allocate most of it,
 * leaving just enough for the test to attempt additional allocations that will
 * require a region node.
 *
 * @param btc the test case
 */
void setup_use_region_node(FLTestCase *btc) {
    ArenaRegionNode *arn = FL_CONTAINER_OF(btc, ArenaRegionNode, tc);

    // Allocate a small arena - just 4KB beyond the arena structure itself
    size_t initial_request = KIBI(4);
    arn->arena[0]          = new_arena(initial_request, 0);
    arn->arena[1]          = new_arena(initial_request, 0);

    // Allocate most of the space, leaving CHUNK_MIN_SIZE for the test. Subtract
    // CHUNK_ALIGNED_SIZE, because that will be added by the allocator to ensure there's
    // room for the chunk's size. Subtract CHUNK_MIN_SIZE to ensure there's room for one
    // small allocation of CHUNK_MIN_PAYLOAD (16 bytes) (for a total of 32 bytes for the
    // FreeChunk) in the test.
    size_t top_size  = CHUNK_SIZE(arn->arena[0]->top);
    size_t available = region_available_bytes(MEM_TO_REGION(arn->arena[0]));
    size_t request   = available + top_size - CHUNK_ALIGNED_SIZE - CHUNK_MIN_SIZE;
    (void)arena_malloc_throw(arn->arena[0], request, __FILE__, __LINE__);
    available = region_available_bytes(MEM_TO_REGION(arn->arena[0]));
}

void cleanup_use_region_node(FLTestCase *btc) {
    ArenaRegionNode *arn = FL_CONTAINER_OF(btc, ArenaRegionNode, tc);
    for (size_t i = 0; i < sizeof arn->arena / sizeof arn->arena[0]; i++) {
        if (arn->arena[i] != NULL) {
            release_arena(&arn->arena[i]);
        }
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Use Region Node", ArenaRegionNode, test_use_region_node,
                           setup_use_region_node, cleanup_use_region_node) {
    Arena *arena = t->arena[0];
    void  *mem1  = NULL;
    void  *mem2  = NULL;

    // Verify the arena is set up as expected
    size_t top_size = CHUNK_SIZE(arena->top);

    FL_ASSERT_DETAILS(top_size >= CHUNK_MIN_SIZE,
                      "top size is too small. Expected at least %zu, actual %zu",
                      CHUNK_MIN_SIZE, top_size);

    // Allocate a small amount from the remaining space
    size_t small_request = CHUNK_MIN_PAYLOAD;
    mem1                 = arena_malloc_throw(arena, small_request, __FILE__, __LINE__);

    // TODO: Once region node allocation is implemented, attempt another allocation
    // that should trigger allocation of a new region node. For now, just verify
    // current behavior.

    // Check the region list - should still be empty until region nodes are implemented
    FL_ASSERT_DETAILS(DLIST_IS_EMPTY(&arena->region_list),
                      "expected region list to be empty");

    mem2 = arena_malloc_throw(arena, small_request, __FILE__, __LINE__);
    FL_ASSERT_DETAILS(!DLIST_IS_EMPTY(&arena->region_list),
                      "expected region list to be non-empty");

    // Clean up the test allocation
    arena_free_throw(arena, mem1, __FILE__, __LINE__);
    arena_free_throw(arena, mem2, __FILE__, __LINE__);
}
