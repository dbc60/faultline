/**
 * @file digital_search_tree.c
 * @author Douglas Cuthbertson
 * @brief An implementation of a Digital Search Tree intended to track memory chunks in
 * an arena.
 * @version 0.1
 * @date 2025-05-02
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "digital_search_tree.h"
#include <faultline/dlist.h>           // for DLIST_INSERT_NEXT
#include <stddef.h>                      // for size_t
#include "bits.h"                        // for BITS_RIGHT_MSB_LEAST
#include "chunk.h"                       // for CHUNK_SIZE
#include <faultline/fl_abbreviated_types.h>        // for flag64
#include <faultline/fl_exception_service_assert.h> // for assert, FL_ASSERT_DETAILS

DigitalSearchTree *dst_leftmost_leaf(DigitalSearchTree *dst) {
    DigitalSearchTree *child = dst_leftmost_child(dst);

    while (child != dst) {
        dst   = child;
        child = dst_leftmost_child(dst);
    }

    return child;
}

DigitalSearchTree *dst_rightmost_leaf(DigitalSearchTree *dst) {
    DigitalSearchTree *child = dst_rightmost_child(dst);

    while (child != dst) {
        dst   = child;
        child = dst_rightmost_child(dst);
    }

    return child;
}

void dst_insert(DigitalSearchTree *restrict root, DigitalSearchTree *restrict node,
                flag64 left_shift) {
    DigitalSearchTree *child = root;
    size_t             size  = CHUNK_SIZE(node);

    FL_ASSERT_TRUE(DST_IS_ROOT(node));
    FL_ASSERT_TRUE(DST_IS_LEAF(node));

    do {
        root = child;
        if (CHUNK_SIZE(root) == size) {
            // node and root are the same size, so make them siblings
            DLIST_INSERT_NEXT(&root->siblings, &node->siblings);
        } else {
            // traverse to next subtree
            flag64 next = BITS_RIGHT_MSB_LEAST(size, left_shift, 1);
            child       = root->child[next];

            // node should NOT be in the tree already
            FL_ASSERT_NEQ_PTR(child, node);

            if (root == child) {
                // We found an unused child
                root->child[next] = node;
                node->parent      = root;
            }
        }
        left_shift++;
    } while (root != child);

    FL_ASSERT_DETAILS(
        root->child[0] == root || root->child[0]->parent == root,
        "Expected 0x%p to be parent of its left child 0x%p. Actual parent 0x%p", root,
        root->child[0], root->child[0]->parent);

    FL_ASSERT_DETAILS(
        root->child[1] == root || root->child[1]->parent == root,
        "Expected 0x%p to be parent of its right child 0x%p. Actual parent 0x%p", root,
        root->child[1], root->child[1]->parent);
}
