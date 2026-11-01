/**
 * @file dst_demo.c
 * @author Douglas Cuthbertson
 * @brief WebAssembly exports over the arena's digital search tree, for a browser demo.
 *
 * Compiles the real digital_search_tree.c around one tree that stands in for a large
 * bin. Callers name nodes by integer ids from a fixed pool, because JavaScript cannot
 * hold a DigitalSearchTree pointer in any useful way.
 *
 * Add, remove and find exports change or search the tree. The read-only exports let a
 * caller walk it:
 *
 *   root() -> id
 *   key(id), left(id), right(id), next_sibling(id)
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "digital_search_tree.c"

#include <faultline/fl_exception.h> // FL_THROW_EXCEPTION_FN, fl_internal_error

#include <stdbool.h> // bool
#include <stddef.h>  // NULL, size_t

#define DEMO_EXPORT(NAME) __attribute__((export_name(NAME)))

/// Returned when an id, key or pool slot is not available.
#define DEMO_NO_NODE (-1)

#define DEMO_CAPACITY 64

// The keys of a 64-bit arena's first large bin, on chunk boundaries.
#define DEMO_KEY_MIN  1024
#define DEMO_KEY_MAX  1535
#define DEMO_KEY_STEP 16

/// ARENA_LEFT_SHIFT(0) on a 64-bit arena: the first decision bit is bit 8. This is fixed
/// rather than taken from the macro, which gives another value on a 32-bit target.
#define DEMO_LEFT_SHIFT 55

static DigitalSearchTree  s_pool[DEMO_CAPACITY];
static bool               s_in_use[DEMO_CAPACITY];
static DigitalSearchTree *s_root;

/* Assertions are the only source of throws here. Nothing catches them, so stop. */
FLExceptionReason fl_internal_error = "internal error";

FL_THROW_EXCEPTION_FN(fl_throw) {
    FL_UNUSED(reason);
    FL_UNUSED(details);
    FL_UNUSED(file);
    FL_UNUSED(line);
    __builtin_trap();
}

FL_THROW_EXCEPTION_FN(fl_throw_assertion) {
    fl_throw(reason, details, file, line);
}

/*
 * Assertions format their details with snprintf. libc's version imports WASI functions
 * a browser does not provide, and fl_throw discards the text, so this one writes none.
 */
int snprintf(char *buffer, size_t size, char const *format, ...) {
    FL_UNUSED(format);

    if (size > 0) {
        buffer[0] = '\0';
    }

    return 0;
}

static int node_id(DigitalSearchTree const *node) {
    return node == NULL ? DEMO_NO_NODE : (int)(node - s_pool);
}

static bool id_in_use(int id) {
    return id >= 0 && id < DEMO_CAPACITY && s_in_use[id];
}

static bool key_is_valid(unsigned key) {
    return key >= DEMO_KEY_MIN && key <= DEMO_KEY_MAX && key % DEMO_KEY_STEP == 0;
}

static int free_slot(void) {
    int id = 0;

    while (id < DEMO_CAPACITY && s_in_use[id]) {
        id++;
    }

    return id < DEMO_CAPACITY ? id : DEMO_NO_NODE;
}

/// A child link that points at its own node means there is no child.
static int child_id(int id, int side) {
    if (!id_in_use(id) || s_pool[id].child[side] == &s_pool[id]) {
        return DEMO_NO_NODE;
    }

    return node_id(s_pool[id].child[side]);
}

DEMO_EXPORT("reset") void demo_reset(void) {
    for (int id = 0; id < DEMO_CAPACITY; id++) {
        s_in_use[id] = false;
    }

    s_root = NULL;
}

DEMO_EXPORT("capacity") int demo_capacity(void) {
    return DEMO_CAPACITY;
}

/**
 * @brief Insert a node with the given key.
 *
 * @return the new node's id, or DEMO_NO_NODE if the key is invalid or the pool is full.
 */
DEMO_EXPORT("add") int demo_add(unsigned key) {
    int id = free_slot();

    if (!key_is_valid(key) || id == DEMO_NO_NODE) {
        return DEMO_NO_NODE;
    }

    s_in_use[id] = true;
    dst_init(&s_pool[id], key);

    if (s_root == NULL) {
        s_root = &s_pool[id];
    } else {
        dst_insert(s_root, &s_pool[id], DEMO_LEFT_SHIFT);
    }

    return id;
}

/**
 * @brief Remove a node, as arena_free_impl removes a free neighbor from its bin.
 *
 * A sibling has no tree links, so it only leaves its list. Any other node goes through
 * dst_remove_leftmost, and its replacement becomes the root if it was the root.
 *
 * @return the id of the removed node, or DEMO_NO_NODE if the id is not in use.
 */
DEMO_EXPORT("remove") int demo_remove(int id) {
    if (!id_in_use(id)) {
        return DEMO_NO_NODE;
    }

    DigitalSearchTree *node = &s_pool[id];

    if (s_root != node && DST_IS_ROOT(node)) {
        DLIST_REMOVE(&node->siblings);
    } else {
        DigitalSearchTree *replacement = dst_remove_leftmost(node);

        if (s_root == node) {
            s_root = replacement;
        }
    }

    s_in_use[id] = false;

    return id;
}

/**
 * @brief Find the best fit for a key.
 *
 * @return the id of a node with the smallest key at or above the given one, or
 * DEMO_NO_NODE if there is none or the key is invalid.
 */
DEMO_EXPORT("find") int demo_find(unsigned key) {
    if (s_root == NULL || !key_is_valid(key)) {
        return DEMO_NO_NODE;
    }

    return node_id(dst_find_smallest_large_node(s_root, key, DEMO_LEFT_SHIFT));
}

DEMO_EXPORT("root") int demo_root(void) {
    return node_id(s_root);
}

DEMO_EXPORT("key") int demo_key(int id) {
    return id_in_use(id) ? (int)CHUNK_SIZE(&s_pool[id]) : DEMO_NO_NODE;
}

DEMO_EXPORT("left") int demo_left(int id) {
    return child_id(id, 0);
}

DEMO_EXPORT("right") int demo_right(int id) {
    return child_id(id, 1);
}

/// The next node in a circular list of same-size nodes, or DEMO_NO_NODE if it has none.
DEMO_EXPORT("next_sibling") int demo_next_sibling(int id) {
    if (!id_in_use(id) || !DST_HAS_SIBLINGS(&s_pool[id])) {
        return DEMO_NO_NODE;
    }

    return node_id(DST_NEXT_SIBLING(&s_pool[id]));
}
