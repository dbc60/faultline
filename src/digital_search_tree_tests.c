/**
 * @file digital_search_tree_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the Digital Search Tree.
 * @version 0.1
 * @date 2025-05-02
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "digital_search_tree.c"
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service

#include <faultline/fl_test.h>   // FLTestCase
#include <faultline/dlist.h>     // DList
#include <faultline/fl_macros.h> // FL_CONTAINER_OF
#include <faultline/size.h>      // SIZE_T_BITSIZE, SIZE_T_ONE

#include <stddef.h> // size_t

#define MAX_BINS      48
#define LOG2_MIN      10
#define KEY_MIN       1024
#define KEY_MAX       1536
#define KEY_INCREMENT 16

/// Compute the left-shift for a given bin index (mirrors ARENA_LEFT_SHIFT).
#define TEST_LEFT_SHIFT(IDX) \
    (((IDX) == MAX_BINS - 1) \
         ? 0                 \
         : (SIZE_T_BITSIZE - SIZE_T_ONE) - (((IDX) >> 1) + LOG2_MIN - 2))

/**
 * @brief Build a 5-node test tree rooted at root.
 *
 * Tree structure (using TEST_LEFT_SHIFT(0), discriminator at bit 8):
 *        root(1024)
 *        /        \
 *     b(1040)   a(1280)
 *     /     \
 *   d(1056) c(1152)
 */
static void build_test_tree(DigitalSearchTree *root, DigitalSearchTree *a,
                            DigitalSearchTree *b, DigitalSearchTree *c,
                            DigitalSearchTree *d) {
    flag64 left_shift = TEST_LEFT_SHIFT(0);

    dst_init(root, 1024);
    dst_init(a, 1280);
    dst_init(b, 1040);
    dst_init(c, 1152);
    dst_init(d, 1056);

    dst_insert(root, a, left_shift);
    dst_insert(root, b, left_shift);
    dst_insert(root, c, left_shift);
    dst_insert(root, d, left_shift);
}

typedef struct DSTInsertTestCase {
    FLTestCase        tc;
    DigitalSearchTree dst;
} DSTInsertTestCase;

static FL_SETUP_FN(setup_insert) {
    DSTInsertTestCase *t = FL_CONTAINER_OF(tc, DSTInsertTestCase, tc);
    dst_init(&t->dst, KEY_MIN);
}

FL_TYPE_TEST_SETUP_CLEANUP("Insert Sibling", DSTInsertTestCase, insert_sibling,
                           setup_insert, fl_default_cleanup) {
    DigitalSearchTree *root = &t->dst;
    flag64 left_shift       = ((0 == MAX_BINS - 1) ? 0
                                                   : (SIZE_T_BITSIZE - SIZE_T_ONE)
                                                         - ((0 >> 1) + LOG2_MIN - 2));
    DigitalSearchTree node;

    dst_init(&node, KEY_MIN);

    dst_insert(root, &node, left_shift);
    FL_ASSERT_EQ_PTR(&node, DST_NEXT_SIBLING(root));
    FL_ASSERT_EQ_PTR(root, root->child[0]);
    FL_ASSERT_EQ_PTR(root, root->child[1]);
    FL_ASSERT_EQ_PTR(root, root->parent);
}

FL_TYPE_TEST_SETUP_CLEANUP("Insert", DSTInsertTestCase, insert, setup_insert,
                           fl_default_cleanup) {
    DigitalSearchTree *root = &t->dst;
    flag64 left_shift       = ((0 == MAX_BINS - 1) ? 0
                                                   : (SIZE_T_BITSIZE - SIZE_T_ONE)
                                                         - ((0 >> 1) + LOG2_MIN - 2));
    DigitalSearchTree node;

    for (size_t key = KEY_MIN + KEY_INCREMENT; key < KEY_MAX; key += KEY_INCREMENT) {
        dst_init(root, KEY_MIN);
        dst_init(&node, key);
        dst_insert(root, &node, left_shift);
        FL_ASSERT_EQ_PTR(root, DST_NEXT_SIBLING(root));

        if ((key & 0x100) == 0) {
            FL_ASSERT_EQ_PTR(&node, root->child[0]);
            FL_ASSERT_EQ_PTR(root, root->child[1]);
        } else {
            FL_ASSERT_EQ_PTR(&node, root->child[1]);
            FL_ASSERT_EQ_PTR(root, root->child[0]);
        }
        FL_ASSERT_EQ_PTR(root, root->parent);
    }
}

FL_TEST_SETUP_CLEANUP("Remove Leftmost with Sibling", remove_leftmost_with_sibling,
                      fl_default_setup, fl_default_cleanup) {
    DigitalSearchTree parent, dst, sibling;

    // Initialize all nodes
    dst_init(&parent, 999);
    dst_init(&dst, 100);
    dst_init(&sibling, 100);

    // Set up parent-child relationship
    dst_insert(&parent, &dst, 0);
    FL_ASSERT_EQ_PTR(&parent, dst.parent);

    // Set up sibling relationship
    dst_insert(&parent, &sibling, 0);
    FL_ASSERT_EQ_PTR(&sibling, DST_NEXT_SIBLING(&dst));

    // Call function under test
    dst_remove_leftmost(&dst);

    // Verify: sibling should now take dst's place
    FL_ASSERT_EQ_PTR(&parent, sibling.parent);
    FL_ASSERT_EQ_PTR(&sibling, sibling.child[0]);
    FL_ASSERT_EQ_PTR(&sibling, sibling.child[1]);

    // Verify: dst has been fully detached
    FL_ASSERT_EQ_PTR(&dst, dst.parent);
    FL_ASSERT_EQ_PTR(&dst, dst.child[0]);
    FL_ASSERT_EQ_PTR(&dst, dst.child[1]);
}

// ---------------------------------------------------------------------------
// 1. Init macros
// ---------------------------------------------------------------------------

FL_TEST("Init", test_init) {
    DigitalSearchTree node;
    dst_init(&node, 1024);

    FL_ASSERT_EQ_SIZE_T((size_t)1024, node.size);
    FL_ASSERT_TRUE(DST_IS_ROOT(&node));
    FL_ASSERT_TRUE(DST_IS_LEAF(&node));
    FL_ASSERT_FALSE(DST_HAS_PARENT(&node));
    FL_ASSERT_FALSE(DST_HAS_SIBLINGS(&node));
    FL_ASSERT_EQ_PTR(&node, node.parent);
    FL_ASSERT_EQ_PTR(&node, node.child[0]);
    FL_ASSERT_EQ_PTR(&node, node.child[1]);
}

// ---------------------------------------------------------------------------
// 2. Build multi-node tree
// ---------------------------------------------------------------------------

FL_TEST("Build Multi-Node Tree", test_build_tree) {
    DigitalSearchTree root, a, b, c, d;
    build_test_tree(&root, &a, &b, &c, &d);

    // Root's children
    FL_ASSERT_EQ_PTR(&b, root.child[0]);
    FL_ASSERT_EQ_PTR(&a, root.child[1]);
    FL_ASSERT_EQ_PTR(&root, b.parent);
    FL_ASSERT_EQ_PTR(&root, a.parent);

    // B's children
    FL_ASSERT_EQ_PTR(&d, b.child[0]);
    FL_ASSERT_EQ_PTR(&c, b.child[1]);
    FL_ASSERT_EQ_PTR(&b, d.parent);
    FL_ASSERT_EQ_PTR(&b, c.parent);

    // A, C, D are leaves
    FL_ASSERT_TRUE(DST_IS_LEAF(&a));
    FL_ASSERT_TRUE(DST_IS_LEAF(&c));
    FL_ASSERT_TRUE(DST_IS_LEAF(&d));

    // No siblings
    FL_ASSERT_FALSE(DST_HAS_SIBLINGS(&root));
    FL_ASSERT_FALSE(DST_HAS_SIBLINGS(&a));
    FL_ASSERT_FALSE(DST_HAS_SIBLINGS(&b));
}

// ---------------------------------------------------------------------------
// 3. Leftmost leaf
// ---------------------------------------------------------------------------

FL_TEST("Leftmost Leaf", test_leftmost_leaf) {
    DigitalSearchTree root, a, b, c, d;
    build_test_tree(&root, &a, &b, &c, &d);

    // Leftmost leaf of root is D (left → left)
    FL_ASSERT_EQ_PTR(&d, dst_leftmost_leaf(&root));

    // Leftmost leaf of B is also D
    FL_ASSERT_EQ_PTR(&d, dst_leftmost_leaf(&b));

    // Leaf of a leaf is itself
    FL_ASSERT_EQ_PTR(&a, dst_leftmost_leaf(&a));
    FL_ASSERT_EQ_PTR(&d, dst_leftmost_leaf(&d));
}

// ---------------------------------------------------------------------------
// 4. Rightmost leaf
// ---------------------------------------------------------------------------

FL_TEST("Rightmost Leaf", test_rightmost_leaf) {
    DigitalSearchTree root, a, b, c, d;
    build_test_tree(&root, &a, &b, &c, &d);

    // Rightmost leaf of root is A (right, then A is a leaf)
    FL_ASSERT_EQ_PTR(&a, dst_rightmost_leaf(&root));

    // Rightmost leaf of B is C (right child of B)
    FL_ASSERT_EQ_PTR(&c, dst_rightmost_leaf(&b));

    // Leaf of a leaf is itself
    FL_ASSERT_EQ_PTR(&c, dst_rightmost_leaf(&c));
    FL_ASSERT_EQ_PTR(&d, dst_rightmost_leaf(&d));
}

// ---------------------------------------------------------------------------
// 5. Remove leftmost leaf (no siblings, no children)
// ---------------------------------------------------------------------------

FL_TEST("Remove Leftmost Leaf", test_remove_leftmost_leaf) {
    DigitalSearchTree root, a, b, c, d;
    build_test_tree(&root, &a, &b, &c, &d);

    // D is a leaf with parent B, no siblings
    DigitalSearchTree *replacement = dst_remove_leftmost(&d);

    // Leaf with no siblings → replacement is NULL
    FL_ASSERT_TRUE(replacement == NULL);

    // D should be fully detached
    FL_ASSERT_TRUE(DST_IS_ROOT(&d));
    FL_ASSERT_TRUE(DST_IS_LEAF(&d));

    // B's left child should be cleared, right child (C) unchanged
    FL_ASSERT_EQ_PTR(&b, b.child[0]);
    FL_ASSERT_EQ_PTR(&c, b.child[1]);
}

// ---------------------------------------------------------------------------
// 6. Remove leftmost with children (no siblings)
// ---------------------------------------------------------------------------

FL_TEST("Remove Leftmost with Children", test_remove_leftmost_with_children) {
    DigitalSearchTree root, a, b, c, d;
    build_test_tree(&root, &a, &b, &c, &d);

    // B has children D and C but no siblings.
    // Leftmost leaf of B is D, so D replaces B.
    DigitalSearchTree *replacement = dst_remove_leftmost(&b);
    FL_ASSERT_EQ_PTR(&d, replacement);

    // D takes B's position: parent is root, right child is C
    FL_ASSERT_EQ_PTR(&root, d.parent);
    FL_ASSERT_EQ_PTR(&d, root.child[0]);
    FL_ASSERT_EQ_PTR(&c, d.child[1]);
    FL_ASSERT_EQ_PTR(&d, d.child[0]);
    FL_ASSERT_EQ_PTR(&d, c.parent);

    // B is fully detached
    FL_ASSERT_TRUE(DST_IS_ROOT(&b));
    FL_ASSERT_TRUE(DST_IS_LEAF(&b));

    // A unchanged
    FL_ASSERT_EQ_PTR(&root, a.parent);
    FL_ASSERT_EQ_PTR(&a, root.child[1]);
}

// ---------------------------------------------------------------------------
// 7. Remove rightmost with sibling
// ---------------------------------------------------------------------------

FL_TEST("Remove Rightmost with Sibling", test_remove_rightmost_with_sibling) {
    DigitalSearchTree parent, node, sibling;
    flag64            left_shift = TEST_LEFT_SHIFT(0);

    dst_init(&parent, 1024);
    dst_init(&node, 1280);
    dst_init(&sibling, 1280);

    dst_insert(&parent, &node, left_shift);
    dst_insert(&parent, &sibling, left_shift);
    FL_ASSERT_EQ_PTR(&sibling, DST_NEXT_SIBLING(&node));

    dst_remove_rightmost(&node);

    // Sibling takes node's place
    FL_ASSERT_EQ_PTR(&parent, sibling.parent);
    FL_ASSERT_TRUE(DST_IS_LEAF(&sibling));

    // node is fully detached
    FL_ASSERT_TRUE(DST_IS_ROOT(&node));
    FL_ASSERT_TRUE(DST_IS_LEAF(&node));
}

// ---------------------------------------------------------------------------
// 8. Remove rightmost with children (no siblings)
// ---------------------------------------------------------------------------

FL_TEST("Remove Rightmost with Children", test_remove_rightmost_with_children) {
    DigitalSearchTree root, a, b, c, d;
    build_test_tree(&root, &a, &b, &c, &d);

    // B has children D and C, no siblings.
    // Rightmost leaf of B is C, so C replaces B.
    DigitalSearchTree *replacement = dst_remove_rightmost(&b);
    FL_ASSERT_EQ_PTR(&c, replacement);

    // C takes B's position: parent is root, left child is D
    FL_ASSERT_EQ_PTR(&root, c.parent);
    FL_ASSERT_EQ_PTR(&c, root.child[0]);
    FL_ASSERT_EQ_PTR(&d, c.child[0]);
    FL_ASSERT_EQ_PTR(&c, c.child[1]);
    FL_ASSERT_EQ_PTR(&c, d.parent);

    // B is fully detached
    FL_ASSERT_TRUE(DST_IS_ROOT(&b));
    FL_ASSERT_TRUE(DST_IS_LEAF(&b));

    // A unchanged
    FL_ASSERT_EQ_PTR(&root, a.parent);
}

// ---------------------------------------------------------------------------
// 9. Find smallest large node - exact match
// ---------------------------------------------------------------------------

FL_TEST("Find Exact Match", test_find_exact_match) {
    DigitalSearchTree root, a, b, c, d;
    flag64            left_shift = TEST_LEFT_SHIFT(0);
    build_test_tree(&root, &a, &b, &c, &d);

    // Search for 1152 - should navigate to C and return exact match
    DigitalSearchTree *found = dst_find_smallest_large_node(&root, 1152, left_shift);
    FL_ASSERT_EQ_PTR(&c, found);

    // Search for 1024 - exact match at root
    found = dst_find_smallest_large_node(&root, 1024, left_shift);
    FL_ASSERT_EQ_PTR(&root, found);
}

// ---------------------------------------------------------------------------
// 10. Find smallest large node - best fit on path
// ---------------------------------------------------------------------------

FL_TEST("Find Best Fit", test_find_best_fit) {
    DigitalSearchTree root, a, b, c, d;
    flag64            left_shift = TEST_LEFT_SHIFT(0);
    build_test_tree(&root, &a, &b, &c, &d);

    // Search for 1030 - follows left path through root(1024) → B(1040) → D(1056).
    // B(1040) is the closest fit on the search path: 1040 - 1030 = 10.
    DigitalSearchTree *found = dst_find_smallest_large_node(&root, 1030, left_shift);
    FL_ASSERT_EQ_PTR(&b, found);

    // Verify the result is >= the requested size
    FL_ASSERT_GE_SIZE_T(CHUNK_SIZE(found), (size_t)1030);
}

// ---------------------------------------------------------------------------
// 11. Find smallest large node - no match
// ---------------------------------------------------------------------------

FL_TEST("Find No Match", test_find_no_match) {
    DigitalSearchTree root, a, b, c, d;
    flag64            left_shift = TEST_LEFT_SHIFT(0);
    build_test_tree(&root, &a, &b, &c, &d);

    // Search for 2000 - larger than all nodes (max is 1280)
    DigitalSearchTree *found = dst_find_smallest_large_node(&root, 2000, left_shift);
    FL_ASSERT_TRUE(found == NULL);
}

// ---------------------------------------------------------------------------
// 12. Insert sibling then remove node with children
// ---------------------------------------------------------------------------

FL_TEST("Remove with Sibling Inherits Children", test_remove_sibling_inherits_children) {
    DigitalSearchTree parent, node, sibling, child;
    flag64            left_shift = TEST_LEFT_SHIFT(0);

    dst_init(&parent, 1024);
    dst_init(&node, 1280);
    dst_init(&sibling, 1280);
    dst_init(&child, 1792);

    // Build: node is right child of parent (bit8 of 1280 = 1)
    dst_insert(&parent, &node, left_shift);
    FL_ASSERT_EQ_PTR(&parent, node.parent);

    // Add sibling (same size as node)
    dst_insert(&parent, &sibling, left_shift);
    FL_ASSERT_EQ_PTR(&sibling, DST_NEXT_SIBLING(&node));

    // Add child under node: 1792 has bit8=1 (goes right to node),
    // then bit7=0 (goes left from node)
    dst_insert(&parent, &child, left_shift);
    FL_ASSERT_EQ_PTR(&node, child.parent);
    FL_ASSERT_EQ_PTR(&child, node.child[0]);

    // Remove node - sibling should replace it and inherit child
    dst_remove_leftmost(&node);

    // Sibling takes node's position
    FL_ASSERT_EQ_PTR(&parent, sibling.parent);
    FL_ASSERT_EQ_PTR(&sibling, parent.child[1]);

    // Sibling inherits node's child
    FL_ASSERT_EQ_PTR(&child, sibling.child[0]);
    FL_ASSERT_EQ_PTR(&sibling, child.parent);

    // node is fully detached
    FL_ASSERT_TRUE(DST_IS_ROOT(&node));
    FL_ASSERT_TRUE(DST_IS_LEAF(&node));
}

FL_SUITE_BEGIN(dst)
FL_SUITE_ADD_EMBEDDED(insert_sibling)
FL_SUITE_ADD_EMBEDDED(insert)
FL_SUITE_ADD(remove_leftmost_with_sibling)
FL_SUITE_ADD(test_init)
FL_SUITE_ADD(test_build_tree)
FL_SUITE_ADD(test_leftmost_leaf)
FL_SUITE_ADD(test_rightmost_leaf)
FL_SUITE_ADD(test_remove_leftmost_leaf)
FL_SUITE_ADD(test_remove_leftmost_with_children)
FL_SUITE_ADD(test_remove_rightmost_with_sibling)
FL_SUITE_ADD(test_remove_rightmost_with_children)
FL_SUITE_ADD(test_find_exact_match)
FL_SUITE_ADD(test_find_best_fit)
FL_SUITE_ADD(test_find_no_match)
FL_SUITE_ADD(test_remove_sibling_inherits_children)
FL_SUITE_END;

FL_GET_TEST_SUITE("Digital Search Tree", dst);
