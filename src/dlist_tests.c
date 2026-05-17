/**
 * @file dlist_butts.c
 * @author Douglas Cuthbertson
 * @brief Test suite for a doubly-linked list implementation
 * @version 0.2
 * @date 2024-11-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include "fl_exception_service.c"
#include "fla_exception_service.c"
#include <faultline/fl_exception_service_assert.h> // assert
#include <faultline/fl_test.h>                     // FLTestCase. FLTestSuite
#include <faultline/fl_macros.h>                   // FL_SPEC_EXPORT
#include <faultline/dlist.h>                       // DList

#if defined(_WIN32) || defined(WIN32)
#include "intrinsics_win32.h" // local memset
#else
#include <string.h> // memset
#endif

#include <stdbool.h>

typedef struct DListTestCase {
    FLTestCase tc;
    DList    **data;
} DListTestCase;

// Test Data
static DList dl1, dl2, dl3, dl4;

static DList *one_node[]    = {&dl1, NULL};
static DList *two_nodes[]   = {&dl1, &dl2, NULL};
static DList *three_nodes[] = {&dl1, &dl2, &dl3, NULL};
static DList *four_nodes[]  = {&dl1, &dl2, &dl3, &dl4, NULL};

static void init_nodes(FLTestCase *tc) {
    DListTestCase *dtc = FL_CONTAINER_OF(tc, DListTestCase, tc);
    DList        **dl  = dtc->data;

    while (*dl) {
        DLIST_INIT(*dl);
        dl++;
    }
}

// setup1: 1 initialized node
static void setup1(FLTestCase *data) {
    DListTestCase *tc = FL_CONTAINER_OF(data, DListTestCase, tc);
    tc->data          = one_node;
    init_nodes(data);
}

// setup2: 2 initialized nodes, no insertions
static void setup2(FLTestCase *data) {
    DListTestCase *tc = FL_CONTAINER_OF(data, DListTestCase, tc);
    tc->data          = two_nodes;
    init_nodes(data);
}

// setup2_inserted: sentinel + 1 node inserted via DLIST_INSERT_NEXT
// Resulting order: list -> first -> list
static void setup2_inserted(FLTestCase *data) {
    DListTestCase *tc = FL_CONTAINER_OF(data, DListTestCase, tc);
    tc->data          = two_nodes;
    init_nodes(data);
    DList **dl = tc->data;
    DLIST_INSERT_NEXT(dl[0], dl[1]);
}

// setup3: 3 initialized nodes, no insertions
static void setup3(FLTestCase *data) {
    DListTestCase *tc = FL_CONTAINER_OF(data, DListTestCase, tc);
    tc->data          = three_nodes;
    init_nodes(data);
}

// setup4: sentinel + 3 nodes inserted via DLIST_INSERT_NEXT
// Resulting order: list -> third -> second -> first -> list
static void setup4(FLTestCase *data) {
    DListTestCase *tc = FL_CONTAINER_OF(data, DListTestCase, tc);
    tc->data          = four_nodes;
    init_nodes(data);
    DList **dl     = tc->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    DList  *third  = dl[3];
    DLIST_INSERT_NEXT(list, first);
    DLIST_INSERT_NEXT(list, second);
    DLIST_INSERT_NEXT(list, third);
}

static void cleanup_nodes(FLTestCase *data) {
    DListTestCase *tc = FL_CONTAINER_OF(data, DListTestCase, tc);
    DList        **dl = tc->data;

    while (*dl) {
        memset(*dl, 0, sizeof(DList));
        dl++;
    }
}

// ============================================================================
// Initialization Tests
// ============================================================================

// DLIST_INIT: node points to itself and is empty
FL_TYPE_TEST_SETUP_CLEANUP("Initialize", DListTestCase, test_initialize, setup1,
                           cleanup_nodes) {
    DList **dl = t->data;
    bool    result;

    result = (*dl != DLIST_PREVIOUS(*dl));
    result |= (*dl != DLIST_NEXT(*dl));
    result |= !DLIST_IS_EMPTY(*dl);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// ============================================================================
// Insertion Tests - DLIST_INSERT_NEXT (head insertion)
// ============================================================================

// Insert 1 node at head. Expected: list <-> first (circular)
FL_TYPE_TEST_SETUP_CLEANUP("Insert Next One", DListTestCase, insert_next_one, setup2,
                           cleanup_nodes) {
    DList **dl    = t->data;
    DList  *list  = dl[0];
    DList  *first = dl[1];
    bool    result;

    DLIST_INSERT_NEXT(list, first);
    result = (DLIST_NEXT(list) != first) | (DLIST_PREVIOUS(list) != first);
    result |= (DLIST_NEXT(first) != list) | (DLIST_PREVIOUS(first) != list);
    result |= DLIST_IS_EMPTY(list);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// Insert 2 nodes at head. Expected: list -> second -> first -> list
FL_TYPE_TEST_SETUP_CLEANUP("Insert Next Two", DListTestCase, insert_next_two, setup3,
                           cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    bool    result;

    DLIST_INSERT_NEXT(list, first);
    DLIST_INSERT_NEXT(list, second);
    result = (DLIST_NEXT(list) != second) | (DLIST_PREVIOUS(list) != first);
    result |= (DLIST_NEXT(second) != first) | (DLIST_PREVIOUS(second) != list);
    result |= (DLIST_NEXT(first) != list) | (DLIST_PREVIOUS(first) != second);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// ============================================================================
// Insertion Tests - DLIST_INSERT_PREVIOUS (tail insertion)
// ============================================================================

// Insert 1 node at tail. Expected: list <-> first (circular)
FL_TYPE_TEST_SETUP_CLEANUP("Insert Previous One", DListTestCase, insert_previous_one,
                           setup2, cleanup_nodes) {
    DList **dl    = t->data;
    DList  *list  = dl[0];
    DList  *first = dl[1];
    bool    result;

    DLIST_INSERT_PREVIOUS(list, first);
    result = (DLIST_NEXT(list) != first) | (DLIST_PREVIOUS(list) != first);
    result |= (DLIST_NEXT(first) != list) | (DLIST_PREVIOUS(first) != list);
    result |= DLIST_IS_EMPTY(list);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// Insert 2 nodes at tail. Expected: list -> first -> second -> list
FL_TYPE_TEST_SETUP_CLEANUP("Insert Previous Two", DListTestCase, insert_previous_two,
                           setup3, cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    bool    result;

    DLIST_INSERT_PREVIOUS(list, first);
    DLIST_INSERT_PREVIOUS(list, second);
    result = (DLIST_NEXT(list) != first) | (DLIST_PREVIOUS(list) != second);
    result |= (DLIST_NEXT(first) != second) | (DLIST_PREVIOUS(first) != list);
    result |= (DLIST_NEXT(second) != list) | (DLIST_PREVIOUS(second) != first);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// ============================================================================
// Navigation Tests - DLIST_FIRST, DLIST_LAST
// ============================================================================

// setup4 order: list -> third -> second -> first -> list
FL_TYPE_TEST_SETUP_CLEANUP("First and Last", DListTestCase, first_and_last, setup4,
                           cleanup_nodes) {
    DList **dl    = t->data;
    DList  *list  = dl[0];
    DList  *first = dl[1];
    DList  *third = dl[3];
    bool    result;

    result = (DLIST_FIRST(list) != third);
    result |= (DLIST_LAST(list) != first);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// ============================================================================
// Traversal Tests
// ============================================================================

// setup4 order: list -> third -> second -> first -> list
FL_TYPE_TEST_SETUP_CLEANUP("Forward Traversal", DListTestCase, traverse_forward, setup4,
                           cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    DList  *third  = dl[3];
    bool    result;

    DList *node = DLIST_FIRST(list);
    result      = (node != third);
    node        = DLIST_NEXT(node);
    result |= (node != second);
    node = DLIST_NEXT(node);
    result |= (node != first);
    node = DLIST_NEXT(node);
    result |= (node != list);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// setup4 order: list -> third -> second -> first -> list
FL_TYPE_TEST_SETUP_CLEANUP("Backward Traversal", DListTestCase, traverse_backward,
                           setup4, cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    DList  *third  = dl[3];
    bool    result;

    DList *node = DLIST_LAST(list);
    result      = (node != first);
    node        = DLIST_PREVIOUS(node);
    result |= (node != second);
    node = DLIST_PREVIOUS(node);
    result |= (node != third);
    node = DLIST_PREVIOUS(node);
    result |= (node != list);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// ============================================================================
// Removal Tests - DLIST_REMOVE
// ============================================================================

// Remove the only node. Both list and node should be empty afterward.
FL_TYPE_TEST_SETUP_CLEANUP("Remove Only Node", DListTestCase, remove_only,
                           setup2_inserted, cleanup_nodes) {
    DList **dl    = t->data;
    DList  *list  = dl[0];
    DList  *first = dl[1];
    bool    result;

    DLIST_REMOVE(first);
    result = !DLIST_IS_EMPTY(list);
    result |= !DLIST_IS_EMPTY(first);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// setup4 order: list -> third -> second -> first -> list
// Remove third (head). Expected: list -> second -> first -> list
FL_TYPE_TEST_SETUP_CLEANUP("Remove Head", DListTestCase, remove_head, setup4,
                           cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    DList  *third  = dl[3];
    bool    result;

    DLIST_REMOVE(third);
    result = !DLIST_IS_EMPTY(third);
    result |= (DLIST_FIRST(list) != second);
    result |= (DLIST_LAST(list) != first);
    result |= (DLIST_NEXT(second) != first) | (DLIST_PREVIOUS(second) != list);
    result |= (DLIST_NEXT(first) != list) | (DLIST_PREVIOUS(first) != second);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// setup4 order: list -> third -> second -> first -> list
// Remove second (middle). Expected: list -> third -> first -> list
FL_TYPE_TEST_SETUP_CLEANUP("Remove Middle", DListTestCase, remove_middle, setup4,
                           cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    DList  *third  = dl[3];
    bool    result;

    DLIST_REMOVE(second);
    result = !DLIST_IS_EMPTY(second);
    result |= (DLIST_FIRST(list) != third);
    result |= (DLIST_LAST(list) != first);
    result |= (DLIST_NEXT(third) != first) | (DLIST_PREVIOUS(third) != list);
    result |= (DLIST_NEXT(first) != list) | (DLIST_PREVIOUS(first) != third);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// setup4 order: list -> third -> second -> first -> list
// Remove first (tail). Expected: list -> third -> second -> list
FL_TYPE_TEST_SETUP_CLEANUP("Remove Tail", DListTestCase, remove_tail, setup4,
                           cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    DList  *third  = dl[3];
    bool    result;

    DLIST_REMOVE(first);
    result = !DLIST_IS_EMPTY(first);
    result |= (DLIST_FIRST(list) != third);
    result |= (DLIST_LAST(list) != second);
    result |= (DLIST_NEXT(third) != second) | (DLIST_PREVIOUS(third) != list);
    result |= (DLIST_NEXT(second) != list) | (DLIST_PREVIOUS(second) != third);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// ============================================================================
// Removal Tests - DLIST_REMOVE_SIMPLE
// ============================================================================

// setup4 order: list -> third -> second -> first -> list
// Remove second with REMOVE_SIMPLE. List re-links but second's pointers are NOT reset.
FL_TYPE_TEST_SETUP_CLEANUP("Remove Simple", DListTestCase, remove_simple, setup4,
                           cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    DList  *third  = dl[3];
    bool    result;

    DLIST_REMOVE_SIMPLE(second);

    // List should be re-linked: list -> third -> first -> list
    result = (DLIST_NEXT(list) != third) | (DLIST_PREVIOUS(list) != first);
    result |= (DLIST_NEXT(third) != first) | (DLIST_PREVIOUS(third) != list);
    result |= (DLIST_NEXT(first) != list) | (DLIST_PREVIOUS(first) != third);

    // second's links should still point to old neighbors (NOT self-referencing)
    result |= (DLIST_NEXT(second) != first);
    result |= (DLIST_PREVIOUS(second) != third);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// ============================================================================
// Re-insertion Tests
// ============================================================================

// setup4 order: list -> third -> second -> first -> list
// Remove second, re-insert at tail. Expected: list -> third -> first -> second -> list
FL_TYPE_TEST_SETUP_CLEANUP("Remove and Reinsert", DListTestCase, remove_reinsert, setup4,
                           cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    DList  *third  = dl[3];
    bool    result;

    DLIST_REMOVE(second);
    DLIST_INSERT_PREVIOUS(list, second);

    result = (DLIST_FIRST(list) != third);
    result |= (DLIST_LAST(list) != second);
    result |= (DLIST_NEXT(third) != first);
    result |= (DLIST_NEXT(first) != second);
    result |= (DLIST_NEXT(second) != list);
    result |= (DLIST_PREVIOUS(second) != first);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

// ============================================================================
// Drain Tests
// ============================================================================

// setup4 order: list -> third -> second -> first -> list
// Remove all nodes one by one from the head, verify state at each step.
FL_TYPE_TEST_SETUP_CLEANUP("Remove All", DListTestCase, remove_all, setup4,
                           cleanup_nodes) {
    DList **dl     = t->data;
    DList  *list   = dl[0];
    DList  *first  = dl[1];
    DList  *second = dl[2];
    DList  *third  = dl[3];
    bool    result;

    // Remove head (third). Expected: list -> second -> first -> list
    DLIST_REMOVE(third);
    result = DLIST_IS_EMPTY(list);
    result |= (DLIST_FIRST(list) != second);

    // Remove head (second). Expected: list -> first -> list
    DLIST_REMOVE(second);
    result |= DLIST_IS_EMPTY(list);
    result |= (DLIST_FIRST(list) != first);

    // Remove last (first). Expected: list empty
    DLIST_REMOVE(first);
    result |= !DLIST_IS_EMPTY(list);

    if (result) {
        FL_THROW(fl_internal_error);
    }
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(test_initialize)
FL_SUITE_ADD_EMBEDDED(insert_next_one)
FL_SUITE_ADD_EMBEDDED(insert_next_two)
FL_SUITE_ADD_EMBEDDED(insert_previous_one)
FL_SUITE_ADD_EMBEDDED(insert_previous_two)
FL_SUITE_ADD_EMBEDDED(first_and_last)
FL_SUITE_ADD_EMBEDDED(traverse_forward)
FL_SUITE_ADD_EMBEDDED(traverse_backward)
FL_SUITE_ADD_EMBEDDED(remove_only)
FL_SUITE_ADD_EMBEDDED(remove_head)
FL_SUITE_ADD_EMBEDDED(remove_middle)
FL_SUITE_ADD_EMBEDDED(remove_tail)
FL_SUITE_ADD_EMBEDDED(remove_simple)
FL_SUITE_ADD_EMBEDDED(remove_reinsert)
FL_SUITE_ADD_EMBEDDED(remove_all)
FL_SUITE_END;
FL_GET_TEST_SUITE("DList", ts)
