/**
 * @file set_tests.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2024-11-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "arena.c"
#include "arena_dbg.c"
#include "buffer.c"
#include "set.c"
#include "digital_search_tree.c"
#include "fault_injector.c"
#include "../third_party/fnv/FNV64.c"
#include "region.c"
#include "region_node.c"
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service
#include "fla_log_service.c"       // g_fla_log_service
#include "fla_memory_service.c"

#include <faultline/fl_abbreviated_types.h> // u16, u32, u64
#include <faultline/fl_test.h>              // FLTestCase

// Typed-set wrapper for int, exercised by test_typed_set.
DEFINE_TYPED_SET(int, int)

// Trivial custom hash and compare functions used by test_custom_hash_compare.
static u64 identity_hash(Set const *set, void const *value) {
    FL_UNUSED(set);
    return (u64)(*(int const *)value);
}

static bool identity_compare(Set const *set, void const *a, void const *b) {
    FL_UNUSED(set);
    return *(int const *)a == *(int const *)b;
}

typedef struct TestSet {
    FLTestCase tc;
    Arena     *arena;
    u32        capacity;
    u32        max_entries;
} TestSet;

FL_SETUP_FN(setup_set) {
    TestSet *t     = FL_CONTAINER_OF(tc, TestSet, tc);
    t->capacity    = 2039;
    t->max_entries = 2039;
    u32 reserve    = (u32)((sizeof(Set) + t->capacity * sizeof(SetEntry)
                         + t->max_entries * sizeof(SetEntry))
                        / arena_granularity());
    t->arena       = new_arena(0, reserve);
}

FL_CLEANUP_FN(cleanup_set) {
    TestSet *t = FL_CONTAINER_OF(tc, TestSet, tc);

    release_arena(&t->arena);
    FL_ASSERT_NULL(t->arena);
}

/**
 * @brief create a set, verify its capacity and that it is empty
 */
FL_TYPE_TEST_SETUP_CLEANUP("Create", TestSet, test_new_set, setup_set, cleanup_set) {
    size_t capacity = t->capacity;

    Set *set = new_set(t->arena, capacity, sizeof(uintptr_t));

    if (!set_is_empty(set)) {
        LOG_ERROR("Set Test", "Expected set to be empty");
        FL_THROW_DETAILS(fl_internal_error, "set_is_empty: expected false, actual true");
    }

    if (set_capacity(set) != capacity) {
        LOG_ERROR("Set Test", "Expected set capacity %zu. Actual %zu", capacity,
                  set_capacity(set));
        FL_THROW_DETAILS(fl_internal_error, "set_capacity: expected 2048, actual %zu",
                         set_capacity(set));
    }

    if (set_size(set) != 0) {
        LOG_ERROR("Set Test", "Expected non-zero set size");
        FL_THROW_DETAILS(fl_internal_error, "set_size: expected 0, actual %zu",
                         set_size(set));
    }

    destroy_set(set);
}

/**
 * @brief create a set, verify that an element is NOT in the set, insert an element, and
 * verify that it is in the set
 *
 * @param btc
 */
FL_TYPE_TEST_SETUP_CLEANUP("Not Member", TestSet, test_membership, setup_set,
                           cleanup_set) {
    Set      *set   = new_set(t->arena, t->capacity, sizeof(uintptr_t));
    uintptr_t value = 0;

    if (set_contains(set, &value)) {
        LOG_ERROR("Set Test", "Expected set to NOT contain %zu", value);
        FL_THROW_DETAILS(fl_internal_error,
                         "set_contains(set, %llu): expected false, actual true", value);
    }

    value = 0xdeadbeef;
    set_insert(set, &value);
    if (!set_contains(set, &value)) {
        LOG_ERROR("Set Test", "Expected set to contain %zu", value);
        FL_THROW_DETAILS(fl_internal_error,
                         "set_contains(set, %llu): expected false, actual true", value);
    }

    destroy_set(set);
}

FL_TYPE_TEST_SETUP_CLEANUP("Length", TestSet, test_length, setup_set, cleanup_set) {
    u32 const capacity    = t->capacity;
    u32 const max_entries = t->max_entries;
    Set      *set         = new_set(t->arena, capacity, sizeof(int));

    for (u32 i = 0; i < max_entries; i++) {
        if (!set_insert(set, &i)) {
            LOG_ERROR("Set Test", "Expected %u to be unique", i);
        }
    }

    if (set_size(set) != max_entries) {
        LOG_ERROR("Set Test", "Expected set size %u. Actual %zu", max_entries,
                  set_size(set));
        FL_THROW_DETAILS(fl_internal_error, "set_size: expected %u, actual %zu",
                         max_entries, set_size(set));
    }

    destroy_set(set);
}

FL_TYPE_TEST_SETUP_CLEANUP("Initialize", TestSet, test_init_set, setup_set,
                           cleanup_set) {
    size_t capacity = t->capacity;
    Set    set;

    init_set(&set, t->arena, capacity, 10);
    if (!set_is_empty(&set)) {
        LOG_ERROR("Set Test", "Expected empty set. Set size %zu", set_size(&set));
        FL_THROW_DETAILS(fl_internal_error, "set_is_empty: expected false, actual true");
    }

    if (set_capacity(&set) != capacity) {
        LOG_ERROR("Set Test", "Expected capacity %zu. Actual %zu", capacity,
                  set_capacity(&set));
        FL_THROW_DETAILS(fl_internal_error, "set_capacity: expected 2048, actual %zu",
                         set_capacity(&set));
    }

    if (set_size(&set) != 0) {
        LOG_ERROR("Set Test", "Expected size 0. Actual %zu", set_size(&set));
        FL_THROW_DETAILS(fl_internal_error, "set_size: expected 0, actual %zu",
                         set_size(&set));
    }

    set_release(&set);
}

FL_TYPE_TEST_SETUP_CLEANUP("Contains With Zero Capacity Throws", TestSet,
                           test_contains_zero_capacity_throws, setup_set, cleanup_set) {
    Set  empty;
    int  value = 42;
    bool threw = false;

    init_set(&empty, t->arena, 0, sizeof value);
    FL_TRY {
        set_contains(&empty, &value);
    }
    FL_CATCH_ALL {
        threw = true;
    }
    FL_END_TRY;

    FL_ASSERT_DETAILS(threw, "set_contains on a zero-capacity set should throw");
    set_release(&empty);
}

FL_TYPE_TEST_SETUP_CLEANUP("Insert With Zero Capacity Throws", TestSet,
                           test_insert_zero_capacity_throws, setup_set, cleanup_set) {
    FL_UNUSED_TYPE_ARG;
    Set  empty;
    int  value = 42;
    bool threw = false;

    init_set(&empty, t->arena, 0, sizeof value);
    FL_TRY {
        set_insert(&empty, &value);
    }
    FL_CATCH_ALL {
        threw = true;
    }
    FL_END_TRY;

    FL_ASSERT_DETAILS(threw, "set_insert on a zero-capacity set should throw");
    set_release(&empty);
}

FL_TYPE_TEST_SETUP_CLEANUP("Remove from Empty Set", TestSet, test_remove_empty,
                           setup_set, cleanup_set) {
    FL_UNUSED_TYPE_ARG;
    Set empty;
    int value = 12;

    init_set(&empty, t->arena, 0, sizeof value);
    FL_ASSERT_DETAILS(set_capacity(&empty) == 0, "set_capacity: expected 0, actual %zu",
                      set_capacity(&empty));
    FL_ASSERT_DETAILS(set_size(&empty) == 0, "set_size: expected 0, actual %zu",
                      set_size(&empty));

    set_remove(&empty, &value);
    FL_ASSERT_DETAILS(set_capacity(&empty) == 0, "set_capacity: expected 0, actual %zu",
                      set_capacity(&empty));
    FL_ASSERT_DETAILS(set_size(&empty) == 0, "set_size: expected 0, actual %zu",
                      set_size(&empty));

    set_release(&empty);
}

/**
 * @brief Insert a value, remove it, verify the count decrements and the value is gone.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Remove Present", TestSet, test_remove_present, setup_set,
                           cleanup_set) {
    Set *set   = new_set(t->arena, t->capacity, sizeof(int));
    int  value = 42;

    set_insert(set, &value);

    bool removed = set_remove(set, &value);
    FL_ASSERT_DETAILS(removed, "set_remove: expected true for present element");
    FL_ASSERT_DETAILS(set_size(set) == 0,
                      "set_size: expected 0 after remove, actual %zu", set_size(set));
    FL_ASSERT_DETAILS(!set_contains(set, &value),
                      "set_contains: expected false after remove");

    destroy_set(set);
}

/**
 * @brief Inserting the same value twice must return false and leave the count unchanged.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Duplicate Insert", TestSet, test_duplicate_insert, setup_set,
                           cleanup_set) {
    Set *set   = new_set(t->arena, t->capacity, sizeof(int));
    int  value = 42;

    bool first = set_insert(set, &value);
    FL_ASSERT_DETAILS(first, "set_insert: first insert should return true");
    FL_ASSERT_DETAILS(set_size(set) == 1,
                      "set_size: expected 1 after first insert, actual %zu",
                      set_size(set));

    bool second = set_insert(set, &value);
    FL_ASSERT_DETAILS(!second, "set_insert: duplicate insert should return false");
    FL_ASSERT_DETAILS(set_size(set) == 1,
                      "set_size: expected 1 after duplicate insert, actual %zu",
                      set_size(set));

    destroy_set(set);
}

/**
 * @brief set_clear empties the set while preserving capacity; the set is usable
 * afterwards.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Clear", TestSet, test_clear, setup_set, cleanup_set) {
    Set *set = new_set(t->arena, t->capacity, sizeof(int));

    for (int i = 0; i < 10; i++) {
        set_insert(set, &i);
    }
    FL_ASSERT_DETAILS(set_size(set) == 10,
                      "set_size: expected 10 before clear, actual %zu", set_size(set));

    set_clear(set);
    FL_ASSERT_DETAILS(set_is_empty(set), "set_is_empty: expected true after clear");
    FL_ASSERT_DETAILS(set_size(set) == 0, "set_size: expected 0 after clear, actual %zu",
                      set_size(set));
    FL_ASSERT_DETAILS(set_capacity(set) == t->capacity,
                      "set_capacity: expected %u after clear, actual %zu", t->capacity,
                      set_capacity(set));

    // The set must be usable after clearing.
    int value = 99;
    FL_ASSERT_DETAILS(set_insert(set, &value), "set_insert: expected true after clear");
    FL_ASSERT_DETAILS(set_size(set) == 1,
                      "set_size: expected 1 after insert following clear, actual %zu",
                      set_size(set));

    destroy_set(set);
}

/**
 * @brief Removing a value absent from a non-empty set must return false and leave the
 * set unchanged.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Remove Absent", TestSet, test_remove_absent, setup_set,
                           cleanup_set) {
    Set *set     = new_set(t->arena, t->capacity, sizeof(int));
    int  present = 10;
    int  absent  = 99;

    set_insert(set, &present);

    bool removed = set_remove(set, &absent);
    FL_ASSERT_DETAILS(!removed, "set_remove: expected false for absent element");
    FL_ASSERT_DETAILS(set_size(set) == 1,
                      "set_size: expected 1 after failed remove, actual %zu",
                      set_size(set));
    FL_ASSERT_DETAILS(set_contains(set, &present),
                      "set_contains: original element should still be present");

    destroy_set(set);
}

/**
 * @brief After removing a value, reinserting it must succeed.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Re-insert After Remove", TestSet, test_reinsert_after_remove,
                           setup_set, cleanup_set) {
    Set *set   = new_set(t->arena, t->capacity, sizeof(int));
    int  value = 42;

    FL_ASSERT_DETAILS(set_insert(set, &value), "initial insert should succeed");
    FL_ASSERT_DETAILS(set_remove(set, &value), "remove should succeed");
    FL_ASSERT_DETAILS(!set_contains(set, &value),
                      "element should be absent after remove");

    bool reinserted = set_insert(set, &value);
    FL_ASSERT_DETAILS(reinserted, "set_insert: re-insert after remove should succeed");
    FL_ASSERT_DETAILS(set_size(set) == 1,
                      "set_size: expected 1 after re-insert, actual %zu", set_size(set));
    FL_ASSERT_DETAILS(set_contains(set, &value),
                      "set_contains: element should be present after re-insert");

    destroy_set(set);
}

/**
 * @brief capacity=1 forces every value into the same bucket, exercising collision-list
 * insert, lookup, and removal.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Collisions", TestSet, test_collisions, setup_set,
                           cleanup_set) {
    FL_UNUSED_TYPE_ARG;
    // key % 1 == 0 for any key, so all values land in bucket 0.
    Set *set = new_set(t->arena, 1, sizeof(int));
    int  a = 1, b = 2, c = 3;

    FL_ASSERT_DETAILS(set_insert(set, &a), "insert a");
    FL_ASSERT_DETAILS(set_insert(set, &b), "insert b");
    FL_ASSERT_DETAILS(set_insert(set, &c), "insert c");
    FL_ASSERT_DETAILS(set_size(set) == 3, "set_size: expected 3, actual %zu",
                      set_size(set));

    FL_ASSERT_DETAILS(set_contains(set, &a), "a should be present");
    FL_ASSERT_DETAILS(set_contains(set, &b), "b should be present");
    FL_ASSERT_DETAILS(set_contains(set, &c), "c should be present");

    // Remove from the middle of the collision chain.
    FL_ASSERT_DETAILS(set_remove(set, &b), "remove b should succeed");
    FL_ASSERT_DETAILS(set_size(set) == 2,
                      "set_size: expected 2 after remove, actual %zu", set_size(set));
    FL_ASSERT_DETAILS(set_contains(set, &a), "a should still be present");
    FL_ASSERT_DETAILS(!set_contains(set, &b), "b should not be present after remove");
    FL_ASSERT_DETAILS(set_contains(set, &c), "c should still be present");

    destroy_set(set);
}

/**
 * @brief set_is_initialized returns true after init_set and false after set_release.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Is Initialized", TestSet, test_is_initialized, setup_set,
                           cleanup_set) {
    Set set;

    init_set(&set, t->arena, t->capacity, sizeof(int));
    FL_ASSERT_DETAILS(set_is_initialized(&set),
                      "set_is_initialized: expected true after initializatin");

    set_release(&set);
    FL_ASSERT_DETAILS(!set_is_initialized(&set),
                      "set_is_initialized: expected false after set_release");
}

/**
 * @brief Calling set_release on a set created by new_set must throw, because the
 * bucket memory is part of the same allocation as the Set itself.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Release Created Throws", TestSet,
                           test_release_created_throws, setup_set, cleanup_set) {
    Set *set   = new_set(t->arena, t->capacity, sizeof(int));
    bool threw = false;

    FL_TRY {
        set_release(set); // must throw: set->created == true
    }
    FL_CATCH_ALL {
        threw = true;
    }
    FL_END_TRY;

    FL_ASSERT_DETAILS(threw, "set_release on a created set should throw");
    destroy_set(set);
}

/**
 * @brief new_set_custom with caller-supplied hash and compare functions stores and
 * retrieves values using those functions.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Custom Hash And Compare", TestSet, test_custom_hash_compare,
                           setup_set, cleanup_set) {
    Set *set = new_set_custom(t->arena, t->capacity, sizeof(int), identity_hash,
                              identity_compare);
    int  a   = 1;
    int  b   = 2;

    FL_ASSERT_DETAILS(set_insert(set, &a), "insert a with custom hash");
    FL_ASSERT_DETAILS(set_insert(set, &b), "insert b with custom hash");
    FL_ASSERT_DETAILS(!set_insert(set, &a),
                      "duplicate insert with custom hash should return false");
    FL_ASSERT_DETAILS(set_contains(set, &a), "a should be present");
    FL_ASSERT_DETAILS(set_contains(set, &b), "b should be present");
    FL_ASSERT_DETAILS(set_size(set) == 2, "set_size: expected 2, actual %zu",
                      set_size(set));

    destroy_set(set);
}

/**
 * @brief DEFINE_TYPED_SET generates a type-safe wrapper API that delegates correctly
 * to the underlying set operations.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Typed Set", TestSet, test_typed_set, setup_set,
                           cleanup_set) {
    intSet *set   = new_int_set(t->arena, t->capacity);
    int     value = 42;

    FL_ASSERT_DETAILS(int_set_is_empty(set), "typed set should be empty initially");
    FL_ASSERT_DETAILS(int_set_capacity(set) == t->capacity,
                      "int_set_capacity: expected %u, actual %zu", t->capacity,
                      int_set_capacity(set));

    FL_ASSERT_DETAILS(int_set_insert(set, &value), "int_set_insert should return true");
    FL_ASSERT_DETAILS(!int_set_is_empty(set),
                      "typed set should not be empty after insert");
    FL_ASSERT_DETAILS(int_set_contains(set, &value), "inserted value should be present");
    FL_ASSERT_DETAILS(int_set_size(set) == 1, "int_set_size: expected 1, actual %zu",
                      int_set_size(set));

    FL_ASSERT_DETAILS(int_set_remove(set, &value), "int_set_remove should return true");
    FL_ASSERT_DETAILS(int_set_is_empty(set), "typed set should be empty after remove");

    destroy_int_set(set);
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(test_new_set)
FL_SUITE_ADD_EMBEDDED(test_init_set)
FL_SUITE_ADD_EMBEDDED(test_membership)
FL_SUITE_ADD_EMBEDDED(test_length)
FL_SUITE_ADD_EMBEDDED(test_remove_empty)
FL_SUITE_ADD_EMBEDDED(test_remove_present)
FL_SUITE_ADD_EMBEDDED(test_remove_absent)
FL_SUITE_ADD_EMBEDDED(test_reinsert_after_remove)
FL_SUITE_ADD_EMBEDDED(test_duplicate_insert)
FL_SUITE_ADD_EMBEDDED(test_contains_zero_capacity_throws)
FL_SUITE_ADD_EMBEDDED(test_insert_zero_capacity_throws)
FL_SUITE_ADD_EMBEDDED(test_clear)
FL_SUITE_ADD_EMBEDDED(test_collisions)
FL_SUITE_ADD_EMBEDDED(test_is_initialized)
FL_SUITE_ADD_EMBEDDED(test_release_created_throws)
FL_SUITE_ADD_EMBEDDED(test_custom_hash_compare)
FL_SUITE_ADD_EMBEDDED(test_typed_set)
FL_SUITE_END;
FL_GET_TEST_SUITE("Set", ts)
