/**
 * @file arena_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for Arena components.
 * @version 0.1
 * @date 2024-08-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#define TEST_SUITE_NAME "Arena"

#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"
#include "arena_test.c"
#include "arena_malloc_test.c"
#include "buffer.c"
#include "../third_party/fnv/FNV64.c"
#include "set.c"
#include "fault_injector.c"
#include "arena_malloc_throw_test.c"
#include "digital_search_tree.c"
#include "arena_release_test.c"
#include "arena_inspection_test.c"
#include "arena_extend_edge_test.c"
#include "arena_aligned_alloc_test.c"
#include "arena_fast_sysinfo_mixed_test.c"
#include "arena_footprint_limit_test.c"
#include "arena_catch_str_test.c"
#include "region.c"
#include "region_node.c"
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service
#include "fla_log_service.c"       // g_fla_log_service
#include "fla_memory_service.c"

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD(test_arena_macros)
FL_SUITE_ADD(test_initialize_size)
FL_SUITE_ADD_EMBEDDED(test_initialization)
FL_SUITE_ADD(test_chunk_size_to_index)
FL_SUITE_ADD(test_small_index_to_chunk_size)
FL_SUITE_ADD(test_select_small_bins)
FL_SUITE_ADD_EMBEDDED(test_small_range)
FL_SUITE_ADD(test_min_large_size)
FL_SUITE_ADD(test_large_chunk_to_index)
FL_SUITE_ADD_EMBEDDED(test_small_bins_exact_fit)
FL_SUITE_ADD_EMBEDDED(test_large_bins)
FL_SUITE_ADD_EMBEDDED(test_allocate_small_request_from_large_bin)
FL_SUITE_ADD_EMBEDDED(test_small_range_malloc)
FL_SUITE_ADD_EMBEDDED(test_large_range)
FL_SUITE_ADD(merge_backward)
FL_SUITE_ADD(merge_forward)
FL_SUITE_ADD_EMBEDDED(test_small_range_malloc_throw)
FL_SUITE_ADD_EMBEDDED(test_large_range_throw)
FL_SUITE_ADD_EMBEDDED(test_top)
FL_SUITE_ADD_EMBEDDED(test_in_order_delete)
FL_SUITE_ADD_EMBEDDED(test_reverse_order_delete)
FL_SUITE_ADD_EMBEDDED(test_use_region_node)
FL_SUITE_ADD_EMBEDDED(footprint_after_init)
FL_SUITE_ADD_EMBEDDED(footprint_after_extend)
FL_SUITE_ADD_EMBEDDED(max_footprint_tracks_peak)
FL_SUITE_ADD_EMBEDDED(allocation_count_lifecycle)
FL_SUITE_ADD_EMBEDDED(allocation_size_returns_chunk_size)
FL_SUITE_ADD_EMBEDDED(is_valid_allocation_true)
FL_SUITE_ADD_EMBEDDED(is_valid_allocation_false)
FL_SUITE_ADD_EMBEDDED(release_no_merge)
FL_SUITE_ADD_EMBEDDED(release_forward_merge_top)
FL_SUITE_ADD_EMBEDDED(release_forward_merge_fast)
FL_SUITE_ADD_EMBEDDED(release_forward_merge_small_bin)
FL_SUITE_ADD_EMBEDDED(release_forward_merge_large_bin)
FL_SUITE_ADD_EMBEDDED(release_backward_merge_fast)
FL_SUITE_ADD_EMBEDDED(release_backward_merge_small_bin)
FL_SUITE_ADD_EMBEDDED(release_backward_merge_large_bin)
FL_SUITE_ADD_EMBEDDED(release_merge_both)
FL_SUITE_ADD_EMBEDDED(release_merge_both_to_top)
FL_SUITE_ADD_EMBEDDED(extend_within_reserved)
FL_SUITE_ADD_EMBEDDED(extend_new_region_node)
FL_SUITE_ADD_EMBEDDED(extend_updates_footprint)
FL_SUITE_ADD_EMBEDDED(allocate_zero_bytes)
FL_SUITE_ADD_EMBEDDED(allocate_one_byte)
FL_SUITE_ADD_EMBEDDED(allocate_boundary_small_large)
FL_SUITE_ADD_EMBEDDED(release_arena_cleans_region_nodes)
FL_SUITE_ADD_EMBEDDED(allocate_exact_top_size)
FL_SUITE_ADD_EMBEDDED(release_adjacent_to_exhausted_top)
FL_SUITE_ADD_EMBEDDED(fast_node_populated_on_split)
FL_SUITE_ADD_EMBEDDED(fast_node_consumed)
FL_SUITE_ADD_EMBEDDED(fast_node_replaced)
FL_SUITE_ADD_EMBEDDED(fast_node_too_small)
FL_SUITE_ADD(page_size_nonzero_power_of_two)
FL_SUITE_ADD(granularity_nonzero_power_of_two)
FL_SUITE_ADD_EMBEDDED(interleaved_small_large)
FL_SUITE_ADD_EMBEDDED(reuse_freed_chunks)
FL_SUITE_ADD_EMBEDDED(fragmentation_and_coalesce)
FL_SUITE_ADD_EMBEDDED(test_aligned_alloc_natural)
FL_SUITE_ADD_EMBEDDED(test_aligned_alloc_32)
FL_SUITE_ADD_EMBEDDED(test_aligned_alloc_64)
FL_SUITE_ADD_EMBEDDED(test_aligned_alloc_128)
FL_SUITE_ADD_EMBEDDED(test_aligned_alloc_multiple)
FL_SUITE_ADD_EMBEDDED(test_aligned_alloc_non_throw_invalid)
FL_SUITE_ADD_EMBEDDED(test_aligned_alloc_throw_invalid)
FL_SUITE_ADD_EMBEDDED(test_aligned_alloc_throw_zero)
FL_SUITE_ADD_EMBEDDED(footprint_limit_rounds_up)
FL_SUITE_ADD_EMBEDDED(footprint_limit_enforced)
FL_SUITE_ADD_EMBEDDED(footprint_limit_zero_means_unlimited)
FL_SUITE_ADD_EMBEDDED(footprint_limit_clear_restores_growth)
FL_SUITE_ADD(catch_str_catches_matching_string)
FL_SUITE_ADD(catch_str_skips_non_matching_string)
FL_SUITE_ADD(catch_str_vs_pointer_identity)
FL_SUITE_END;

FL_GET_TEST_SUITE("Arena", ts)
