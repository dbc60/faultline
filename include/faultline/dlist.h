#ifndef FAULTLINE_DLIST_H_
#define FAULTLINE_DLIST_H_

/**
 * @file dlist.h
 * @author Douglas Cuthbertson
 * @brief FaultLine Doubly-Linked List Library - Public API
 * @version 0.2.0
 * @date 2022-02-12
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * A circular, intrusive doubly-linked list implementation.
 * This code is NOT thread-safe. Wrap DList in a struct that contains a mutex
 * for concurrent access.
 */

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Doubly-linked list node.
 *
 * This is an intrusive list - embed this structure within your own
 * data structures. Use FL_CONTAINER_OF macro to get back to your structure.
 */
typedef struct DList {
    struct DList *left;  ///< Previous node in the list
    struct DList *right; ///< Next node in the list
} DList;

// ============================================================================
// List Initialization
// ============================================================================

/**
 * @brief Initialize an empty circular doubly-linked list.
 *
 * @param DL Pointer to the list head.
 */
#define DLIST_INIT(DL) ((DL)->right = (DL)->left = (DL))

// ============================================================================
// List Insertion
// ============================================================================

/**
 * @brief Insert a node before DL (at the tail when DL is the list head).
 *
 * @param DL Pointer to the list head.
 * @param ND Pointer to the node to insert.
 */
#define DLIST_INSERT_PREVIOUS(DL, ND)   \
    do {                                \
        (ND)->left        = (DL)->left; \
        (ND)->right       = (DL);       \
        (ND)->left->right = (ND);       \
        (DL)->left        = (ND);       \
    } while (0)

/**
 * @brief Insert a node after DL (at the head when DL is the list head).
 *
 * @param DL Pointer to the list head.
 * @param ND Pointer to the node to insert.
 */
#define DLIST_INSERT_NEXT(DL, ND)        \
    do {                                 \
        (ND)->right       = (DL)->right; \
        (ND)->left        = (DL);        \
        (DL)->right->left = (ND);        \
        (DL)->right       = (ND);        \
    } while (0)

// ============================================================================
// List Removal
// ============================================================================

/**
 * @brief Remove a node from the list and reset its links.
 *
 * After removal, the node points to itself (can be safely used as a list head).
 *
 * @param DL Pointer to the node to remove.
 */
#define DLIST_REMOVE(DL)                 \
    do {                                 \
        (DL)->left->right = (DL)->right; \
        (DL)->right->left = (DL)->left;  \
        (DL)->left        = (DL);        \
        (DL)->right       = (DL);        \
    } while (0)

/**
 * @brief Remove a node from the list without resetting its links.
 *
 * Faster than DLIST_REMOVE but the removed node's links are left dangling.
 * Only use if you're about to free the node or reinitialize it.
 *
 * @param DL Pointer to the node to remove.
 */
#define DLIST_REMOVE_SIMPLE(DL)          \
    do {                                 \
        (DL)->left->right = (DL)->right; \
        (DL)->right->left = (DL)->left;  \
    } while (0)

// ============================================================================
// List Queries
// ============================================================================

/**
 * @brief Check if the list is empty.
 *
 * @param DL Pointer to the list head.
 * @return Non-zero if empty, zero if not empty.
 */
#define DLIST_IS_EMPTY(DL) ((DL)->left == (DL))

// ============================================================================
// List Navigation
// ============================================================================

/**
 * @brief Get the next node.
 *
 * @param DL Pointer to current node.
 * @return Pointer to next node.
 */
#define DLIST_NEXT(DL) ((DL)->right)

/**
 * @brief Get the previous node.
 *
 * @param DL Pointer to current node.
 * @return Pointer to previous node.
 */
#define DLIST_PREVIOUS(DL) ((DL)->left)

/**
 * @brief Get the first node in the list.
 *
 * @param DL Pointer to the list head.
 * @return Pointer to first node.
 */
#define DLIST_FIRST(DL) ((DL)->right)

/**
 * @brief Get the last node in the list.
 *
 * @param DL Pointer to the list head.
 * @return Pointer to last node.
 */
#define DLIST_LAST(DL) ((DL)->left)

// ============================================================================
// Usage Notes
// ============================================================================

/**
 * Example: Embedding DList in a struct
 *
 * Use FL_CONTAINER_OF from fl_macros.h to recover the containing struct.
 *
 * ```c
 * typedef struct MyData {
 *     int value;
 *     DList link;  // Intrusive list node
 * } MyData;
 *
 * DList list_head;
 * DLIST_INIT(&list_head);
 *
 * MyData *data = malloc(sizeof(MyData));
 * data->value = 42;
 * DLIST_INSERT_NEXT(&list_head, &data->link);
 *
 * // Iterate
 * DList *entry = DLIST_FIRST(&list_head);
 * while (entry != &list_head) {
 *     MyData *item = FL_CONTAINER_OF(entry, MyData, link);
 *     printf("%d\n", item->value);
 *     entry = DLIST_NEXT(entry);
 * }
 * ```
 */

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_DLIST_H_
