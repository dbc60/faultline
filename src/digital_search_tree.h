#ifndef DIGITAL_SEARCH_TREE_H_
#define DIGITAL_SEARCH_TREE_H_

/**
 * @file digital_search_tree.h
 * @author Douglas Cuthbertson
 * @brief A binary search tree keyed by the individual bits of each node's size.
 * @version 0.1
 * @date 2025-05-19
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/dlist.h>           // for DLIST_IS_EMPTY, DLIST_NEXT
#include <faultline/fl_abbreviated_types.h>        // for flag64
#include <faultline/fl_exception_service_assert.h> // for assert
#include <faultline/fl_macros.h>                   // for FL_CONTAINER_OF
#include <faultline/fl_try.h>                      // for FL_THROW_DETAILS, FL_THROW
#include <stddef.h>                      // for NULL, size_t
#include "bits.h"                        // for BITS_RIGHT_MSB_LEAST, UNSIG...
#include "chunk.h"                       // for CHUNK_SIZE
#include <faultline/fl_exception_service.h>        // for fl_internal_error

/**
 * @brief DigitalSearchTree is a binary tree where the individual bits of the size of
 * each node is used to determine which child to traverse. Nodes with the same size are
 * stored in a doubly-linked list.
 *
 * @param size the key used to search the left/right subtrees. The bits of size are used
 * to determine which child to traverse.
 * @param siblings a doubly-linked list of nodes with the same size. When a node is
 * removed from the tree, and it has siblings and children, a sibling is preferred over
 * the children to replace the node.
 * @param parent the parent node of this node.
 * @param child the left and right children of this node.
 */
typedef struct DigitalSearchTree {
    size_t size;     // a value whose bits are used to search the left/right subtrees
    DList  siblings; // list of nodes with the same size
    struct DigitalSearchTree *parent;   // parent node
    struct DigitalSearchTree *child[2]; // left and right children
} DigitalSearchTree;

#define DST_HAS_PARENT(T)   ((T)->parent != (T))
#define DST_IS_ROOT(T)      ((T)->parent == (T))
#define DST_IS_LEAF(T)      ((T)->child[0] == (T) && (T)->child[1] == (T))
#define DST_HAS_SIBLINGS(T) (!DLIST_IS_EMPTY(&(T)->siblings))
#define DST_NEXT_SIBLING(T) \
    (FL_CONTAINER_OF(DLIST_NEXT(&(T)->siblings), DigitalSearchTree, siblings))
#define DST_PREV_SIBLING(T) \
    (FL_CONTAINER_OF(DLIST_PREVIOUS(&(T)->siblings), DigitalSearchTree, siblings))
#define DST_LEFT_CHILD(T)  ((T)->child[0])
#define DST_RIGHT_CHILD(T) ((T)->child[1])

#define DST_DISPLAY_NODE(DST, TAG)                                                      \
    do {                                                                                \
        DigitalSearchTree *node_ = (DST);                                               \
        LOG_DEBUG(                                                                      \
            "DIGITAL SEARCH TREE",                                                      \
            "DBG %s: %d\n  %s:   0x%p\n  parent: 0x%p\n  left:   0x%p\n  right:  0x%p", \
            __FILE__, __LINE__, (TAG), node_, node_->parent, node_->child[0],           \
            node_->child[1]);                                                           \
    } while (0)

/**
 * @brief Initialize a DigitalSearchTree.
 *
 * @param dst the DigitalSearchTree object to be initialized
 * @param size the key used for the DigitalSearchTree
 */
static inline void dst_init(DigitalSearchTree *dst, size_t size) {
    dst->size = size;
    DLIST_INIT(&dst->siblings);
    dst->parent   = dst;
    dst->child[0] = dst;
    dst->child[1] = dst;
}

/**
 * @brief Initialize a DigitalSearchTree where the size and siblings are already set.
 *
 * @param dst the DigitalSearchTree object to be initialized
 */
static inline void dst_init_minimal(DigitalSearchTree *dst) {
    dst->parent   = dst;
    dst->child[0] = dst;
    dst->child[1] = dst;
}

/**
 * @brief get the leftmost child.
 *
 * @param dst the address of a DigitalSearchTree node.
 * @return DigitalSearchTree* the address of the leftmost child or the node itself if it
 * has no children.
 */
static inline DigitalSearchTree *dst_leftmost_child(DigitalSearchTree *dst) {
    return dst->child[0] != dst ? dst->child[0] : dst->child[1];
}

/**
 * @brief get the rightmost child.
 *
 * @param dst the address of a DigitalSearchTree node.
 * @return DigitalSearchTree* the address of the rightmost child or the node itself if it
 * has no children.
 */
static inline DigitalSearchTree *dst_rightmost_child(DigitalSearchTree *dst) {
    return dst->child[1] != dst ? dst->child[1] : dst->child[0];
}

/**
 * @brief Find the leftmost leaf of a node.
 *
 * @param dst the node to search.
 * @return DigitalSearchTree* the address of the leftmost child or the node itself if it
 * has no children.
 */
DigitalSearchTree *dst_leftmost_leaf(DigitalSearchTree *dst);

/**
 * @brief Find the rightmost leaf of a node.
 *
 * @param dst the node to search.
 * @return DigitalSearchTree* the address of the rightmost child or the node itself if it
 * has no children.
 */
DigitalSearchTree *dst_rightmost_leaf(DigitalSearchTree *dst);

/**
 * @brief Insert a node into a digital search tree.
 *
 * If the node's size matches an existing node, it is added as a sibling. Otherwise it is
 * placed in the left or right subtree based on the bits of its size.
 *
 * @param dst the root of the tree (or subtree) to insert into.
 * @param node the node to insert. Must be a root leaf (no parent, no children).
 * @param left_shift the number of bits to left-shift the key to place the discriminator
 * bit in the most significant position.
 */
void dst_insert(DigitalSearchTree *restrict dst, DigitalSearchTree *restrict node,
                flag64 left_shift);

/**
 * @brief replace a node in a DigitalSearchTree with another node.
 *
 * @param node the node to be replaced. It will be detached from the tree.
 * @param replacement the replacement node. Must be a leaf.
 */
static inline void dst_replace_node(DigitalSearchTree *node,
                                    DigitalSearchTree *replacement) {
    FL_ASSERT_TRUE(DST_IS_LEAF(replacement));
    DigitalSearchTree *parent;

    if (DST_HAS_SIBLINGS(node)) {
        // remove node from the list of siblings
        FL_ASSERT_EQ_PTR(DST_NEXT_SIBLING(node), replacement);
        DLIST_REMOVE(&node->siblings);
    } else {
        // The replacement came from within the tree, because node has no siblings.
        parent = replacement->parent;

        // Detach the replacement from its parent.
        replacement->parent = replacement;
        if (parent->child[0] == replacement) {
            parent->child[0] = parent;
        } else if (parent->child[1] == replacement) {
            parent->child[1] = parent;
        } else {
            FL_THROW_DETAILS(fl_internal_error,
                             "Expected parent 0x%p to have replacement 0x%p as child. "
                             "Actual left: 0x%p, right: 0x%p",
                             parent, replacement, parent->child[0], parent->child[1]);
        }
    }

    // set up parent relationship
    if (DST_HAS_PARENT(node)) {
        parent              = node->parent;
        node->parent        = node;   // remove node from parent
        replacement->parent = parent; // set parent as replacement's parent
        if (parent->child[0] == node) {
            parent->child[0] = replacement; // set replacement as parent's left child
        } else if (parent->child[1] == node) {
            parent->child[1] = replacement; // set replacement as parent's right child
        } else {
            FL_THROW_DETAILS(fl_internal_error,
                             "Expected parent 0x%p to have node 0x%p as child. Actual "
                             "left: 0x%p, right: 0x%p",
                             parent, node, parent->child[0], parent->child[1]);
        }
    }

    // set up left child
    if (node->child[0] != node) {
        replacement->child[0]  = node->child[0];
        node->child[0]->parent = replacement;
        node->child[0]         = node;
    }

    // set up right child
    if (node->child[1] != node) {
        replacement->child[1]  = node->child[1];
        node->child[1]->parent = replacement;
        node->child[1]         = node;
    }
}

/**
 * @brief get the replacement node for a DigitalSearchTree node. The replacement node is
 * either the leftmost or rightmost leaf of the DigitalSearchTree.
 *
 * @param dst the address of a DigitalSearchTree node.
 * @param leaf_finder a function that returns the leftmost or rightmost leaf of a
 * DigitalSearchTree.
 * @return DigitalSearchTree* the address of the replacement node or NULL if there is no
 * replacement.
 */
static inline DigitalSearchTree *
dst_get_replacement(DigitalSearchTree *dst,
                    DigitalSearchTree *(*leaf_finder)(DigitalSearchTree *)) {
    DigitalSearchTree *replacement = NULL;

    if (DST_HAS_SIBLINGS(dst)) {
        replacement
            = FL_CONTAINER_OF(DLIST_FIRST(&dst->siblings), DigitalSearchTree, siblings);
        if (DST_HAS_PARENT(replacement) || !DST_IS_LEAF(replacement)) {
            FL_THROW(fl_internal_error);
        }
    } else if (!DST_IS_LEAF(dst)) {
        // not a leaf and no siblings, so find a replacement among its children.
        replacement = leaf_finder(dst);
        if (replacement == dst) {
            replacement = NULL;
        }
    }

    return replacement;
}

/**
 * @brief remove the given node from its tree.
 *
 * - If this node has siblings, replace it with its first sibling.
 * - If this node has no siblings, but has children, then replace it with its leftmost
 * leaf.
 *
 * @param dst the node to remove from a tree.
 * @return the address of the replacement node, if any. Otherwise dst.
 */
static inline DigitalSearchTree *dst_remove_leftmost(DigitalSearchTree *dst) {
    DigitalSearchTree *replacement = dst_get_replacement(dst, dst_leftmost_leaf);
    if (replacement != NULL) {
        dst_replace_node(dst, replacement);
    } else {
        // remove the leaf
        DigitalSearchTree *parent = dst->parent;
        if (DST_HAS_PARENT(dst)) {
            dst->parent = dst;
            if (parent->child[0] == dst) {
                parent->child[0] = parent;
            } else {
                parent->child[1] = parent;
            }
        }
    }

    return replacement;
}

/**
 * @brief remove the given node from its tree.
 *
 * - If this node has siblings, replace it with its first sibling.
 * - If this node has no siblings, but has children, then replace it with its rightmost
 * leaf.
 *
 * @param dst the node to remove from a tree.
 * @return the address of the replacement node, if any. Otherwise dst.
 */
static inline DigitalSearchTree *dst_remove_rightmost(DigitalSearchTree *dst) {
    DigitalSearchTree *replacement = dst_get_replacement(dst, dst_rightmost_leaf);
    if (replacement != NULL) {
        dst_replace_node(dst, replacement);
    } else {
        // remove the leaf
        DigitalSearchTree *parent = dst->parent;
        if (DST_HAS_PARENT(dst)) {
            dst->parent = dst;
            if (parent->child[0] == dst) {
                parent->child[0] = parent;
            } else {
                parent->child[1] = parent;
            }
        }
    }

    return replacement;
}

/**
 * @brief Find the node with the smallest size larger than the given one.
 *
 * An arena can use this function to find the smallest chunk that is large enough to
 * satisfy a request. If the return value is NOT NULL, then the caller can call
 * dst_remove_leftmost(node) or dst_remove_rightmost(node) to remove the node from its
 * bin.
 *
 * @param root the root of a DST
 * @param size the desired size of a DST node to search for
 * @param left_shift the number of bits to left-shift the key to place the discriminator
 * bit in the most significant position.
 * @return DigitalSearchTree* the address of a node or NULL if all keys in the DST are
 * smaller than the given key.
 */
static inline DigitalSearchTree *
dst_find_smallest_large_node(DigitalSearchTree *root, size_t size, flag64 left_shift) {
    size_t             remainder_size    = UNSIGNED_NEGATION(size);
    DigitalSearchTree *smallest          = NULL;
    DigitalSearchTree *rightmost_subtree = NULL; // the deepest, untaken right subtree
    flag64             next              = BITS_RIGHT_MSB_LEAST(size, left_shift, 1);

    do {
        size_t root_remainder_size = CHUNK_SIZE(root) - size;
        // If root is at least size bytes, set smallest to root and remainder_size to the
        // difference between root's size and the desired size.
        if (root_remainder_size < remainder_size) {
            smallest       = root;
            remainder_size = root_remainder_size;
        }

        if (remainder_size != 0) {
            // update rightmost_subtree if the right subtree exists and we're traversing
            // to the left.
            DigitalSearchTree *right_child = root->child[1];
            if (right_child != root && next != 1) {
                // save unexplored right subtree in case we run into a deadend, so search
                // can continue here.
                rightmost_subtree = right_child;
            }

            if (root == root->child[next]) {
                // End of search down this path. Continue from the unexplored right
                // subtree, consuming it so we don't revisit it.
                root              = rightmost_subtree;
                rightmost_subtree = NULL;
            } else {
                root = root->child[next];
            }

            left_shift++;
            next = BITS_RIGHT_MSB_LEAST(size, left_shift, 1);
        }
    } while (remainder_size != 0 && root != NULL);

    return smallest;
}

#endif // DIGITAL_SEARCH_TREE_H_
