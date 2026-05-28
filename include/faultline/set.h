#ifndef FAULTLINE_SET_H_
#define FAULTLINE_SET_H_

/**
 * @file set.h
 * @author Landon Curt Noll and Douglas Cuthbertson
 * @brief FaultLine Set Library - Public API
 *
 * Douglas Cuthbertson adapted Noll's set implementation for Faultline, but claims no
 * copyright on this code. Noll's statement regarding this code being in the public doman
 * is below.
 *
 * A hash table-based set implementation using the Fowler/Noll/Vo (FNV)
 * hash algorithm, version 1a (FNV-1a).
 *
 * @version 0.2.0
 * @date 2025-03-27
 *
 * This set implementation is based on the FNV hash algorithm. Because the
 * hash algorithm is in the public domain, I'm placing this set implementation in the
 * public domain.
 *
 ---------------------------------------------------------------------------------------
 The software in this file is the public domain. The following is from Landon Curt Noll,
 one of the creators of the FNV set of hash algorithms.
 ---------------------------------------------------------------------------------------
 *
 * Please do not copyright this code.  This code is in the public domain.
 *
 * LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * By:
 *  chongo <Landon Curt Noll> /\oo/\
 *      http://www.isthe.com/chongo/
 *
 * Share and Enjoy!  :-)
 *
 */
#include <faultline/arena.h>                       // for Arena, ARENA_FREE_THROW
#include <faultline/dlist.h>                       // for DLIST_NEXT, DLIST_INIT, DLI...
#include <faultline/fl_abbreviated_types.h>        // for u64
#include <faultline/fl_exception_service_assert.h> // for FL_ASSERT_NOT_NULL
#include <faultline/fl_try.h>                      // for FL_THROW
#include <stdbool.h>                               // for bool, false, true
#include <string.h>                                // for size_t, NULL, memset
#include <faultline/fl_exception_types.h>          // for FLExceptionReason
#include <faultline/fl_macros.h>                   // for FL_MAYBE_UNUSED

#if defined(__cplusplus)
extern "C" {
#endif

extern FLExceptionReason set_release_bucket_not_allowed;

/**
 * @brief A doubly-linked list of values that form a collision list.
 *
 * @param link a possibly empty list of type-specific values in a set.
 */
typedef struct SetEntry {
    DList link;
    // type-specific data follows
} SetEntry;

typedef struct Set Set;

// The type for type-specific hash functions
typedef u64 (*hash_fn_t)(Set const *set, void const *value);

// The type for type-specific comparison functions.
typedef bool (*compare_fn_t)(Set const *set, void const *a, void const *b);

// Set is a hash table; create with create_set(), free with destroy_set().
struct Set {
    Arena    *arena;
    SetEntry *buckets;      //< A table of SetEntry objects.
    size_t    capacity;     //< the number of buckets
    size_t    element_size; //< size of each element in bytes
    size_t    count;        //< number of items in the set
    // Function pointers for operations
    hash_fn_t    hash_fn;
    compare_fn_t compare_fn;
    bool         initialized; ///< True if this set has been initialized
    bool created; ///< True if this set was allocated by new_set or new_set_custom and
                  ///< false otherwise
};

// make it prime: 1021
#define SET_DEFAULT_CAPACITY 1021

#define SET_ENTRY_GET_NEXT(ENTRY) ((SetEntry *)DLIST_NEXT(&(ENTRY)->link))

/**
 * @brief Define a custom Set type
 *
 * @param VT the type of the values to store
 * @param PRE a unique name to use as a prefix for the set function names
 */
#define DEFINE_TYPED_SET(VT, PRE)                                                      \
    typedef Set VT##Set;                                                               \
                                                                                       \
    static inline FL_MAYBE_UNUSED void init_##PRE##_set(VT##Set *set, Arena *arena,    \
                                                        size_t capacity) {             \
        init_set((Set *)set, arena, capacity, sizeof(VT));                             \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED void init_##PRE##_set_custom(VT##Set     *set,       \
                                                               Arena       *arena,     \
                                                               size_t       capacity,  \
                                                               hash_fn_t    hash,      \
                                                               compare_fn_t compare) { \
        init_set_custom((Set *)set, arena, capacity, sizeof(VT), hash, compare);       \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED VT##Set *new_##PRE##_set(Arena *arena,               \
                                                           size_t capacity) {          \
        return (VT##Set *)new_set(arena, capacity, sizeof(VT));                        \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED VT##Set *new_##PRE##_set_custom(                     \
        Arena *arena, size_t capacity, hash_fn_t hash_fn, compare_fn_t compare_fn) {   \
        return (VT##Set *)new_set_custom(arena, capacity, sizeof(VT), hash_fn,         \
                                         compare_fn);                                  \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED void destroy_##PRE##_set(VT##Set *set) {             \
        destroy_set((Set *)set);                                                       \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED void release_##PRE##_set(Set **setptr) {             \
        release_set(setptr);                                                           \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED bool PRE##_set_is_initialized(VT##Set const *set) {  \
        return set_is_initialized((Set const *)set);                                   \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED void PRE##_set_clear(VT##Set *set) {                 \
        set_clear((Set *)set);                                                         \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED void PRE##_set_release(VT##Set *set) {               \
        set_release((Set *)set);                                                       \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED bool PRE##_set_contains(VT##Set const *set,          \
                                                          VT const      *element) {         \
        return set_contains((Set const *)set, element);                                \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED bool PRE##_set_is_empty(VT##Set const *set) {        \
        return set_is_empty((Set const *)set);                                         \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED size_t PRE##_set_capacity(VT##Set const *set) {      \
        return set_capacity((Set const *)set);                                         \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED size_t PRE##_set_size(VT##Set const *set) {          \
        return set_size((Set const *)set);                                             \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED bool PRE##_set_insert(VT##Set  *set,                 \
                                                        VT const *value) {             \
        return set_insert((Set *)set, value);                                          \
    }                                                                                  \
                                                                                       \
    static inline FL_MAYBE_UNUSED bool PRE##_set_remove(VT##Set  *set,                 \
                                                        VT const *value) {             \
        return set_remove((Set *)set, value);                                          \
    }

/**
 * @brief Hash the value and return the hash in the *hash parameter.
 *
 * @param set
 * @param value
 * @return 64-bit hash of value
 * @throw fl_internal_error
 */
u64 set_do_hash(Set const *set, void const *value);

/**
 * @brief Compare a and b, return true if they match and false otherwise.
 *
 * @param set
 * @param a
 * @param b
 * @return true a and b match
 * @return false a and b are different
 */
bool set_compare(Set const *set, void const *a, void const *b);

static inline void set_initialize(Set *set, Arena *arena, size_t capacity,
                                  size_t element_size, hash_fn_t hash,
                                  compare_fn_t compare, bool created) {
    set->arena        = arena;
    set->capacity     = capacity;
    set->element_size = element_size;
    set->count        = 0;
    set->hash_fn      = hash;
    set->compare_fn   = compare;
    set->initialized  = true;
    set->created      = created;
}

/**
 * @brief initialize a Set in place. Allocate a Set's buckets from the heap, but not the
 * Set itself.
 *
 * This is for Set objects that are NOT allocated from the heap.
 *
 * @param set
 * @param capacity
 */
static inline void init_set_custom(Set *set, Arena *arena, size_t capacity,
                                   size_t element_size, hash_fn_t hash,
                                   compare_fn_t compare) {
    set->buckets = ARENA_CALLOC_THROW(arena, capacity, sizeof(SetEntry));
    for (size_t i = 0; i < capacity; i++) {
        DLIST_INIT(&set->buckets[i].link);
    }
    set_initialize(set, arena, capacity, element_size, hash, compare, false);
}

static inline void init_set(Set *set, Arena *arena, size_t capacity,
                            size_t element_size) {
    init_set_custom(set, arena, capacity, element_size, set_do_hash, set_compare);
}

/**
 * @brief Create a set object. Allocate it and its buckets from the heap.
 *
 * new_set_custom allocates a Set and its buckets in one contiguous block of memory.
 *
 * @param capacity
 * @return Set*
 */
Set *new_set_custom(Arena *arena, size_t capacity, size_t element_size, hash_fn_t hash,
                    compare_fn_t compare);

/**
 * @brief Create a set object with the default hash and compare functions.
 *
 * @param capacity
 * @param element_size
 * @return Set*
 */
static inline Set *new_set(Arena *arena, size_t capacity, size_t element_size) {
    return new_set_custom(arena, capacity, element_size, set_do_hash, set_compare);
}

/**
 * @brief Release all memory held by the Set itself and its buckets.
 *
 * @param set
 */
void destroy_set(Set *set);

/**
 * @brief release the memory resources acquired through new_set_custom and sets *set to
 * NULL.
 *
 * It is a checked runtime error for set or *set to be NULL.
 *
 * @param set the address of the variable pointing to a Set.
 * @throw fl_invalid_value
 */
static inline void release_set(Set **setptr) {
    FL_ASSERT_NOT_NULL(setptr);
    FL_ASSERT_NOT_NULL(*setptr);

    destroy_set(*setptr);
    *setptr = NULL;
}

static inline bool set_is_initialized(Set const *set) {
    return set->initialized;
}

/**
 * @brief Release memory used by collision lists and reset the number of entries in the
 * set.
 *
 * @param set
 */
static inline void set_clear(Set *set) {
    for (size_t i = 0; i < set->capacity; i++) {
        SetEntry *bucket = &set->buckets[i];
        while (!DLIST_IS_EMPTY(&bucket->link)) {
            // remove the first entry on the collision list and free it
            SetEntry *entry = SET_ENTRY_GET_NEXT(bucket);
            DLIST_REMOVE_SIMPLE(&entry->link);
            memset(entry, 0xDE, set->element_size);
            ARENA_FREE_THROW(set->arena, entry);
        }
    }
    set->count = 0;
}

/**
 * @brief Release the memory held by a set initialized via set_init().
 *
 * This function releases the memory allocated for items on collision lists as well as
 * the buckets. It is a checked runtime error to call this function for sets created by
 * create_set(). It's an error, because create_set() allocates the set and its buckets
 * are allocated in the same memory block.
 *
 * @param set
 */
static inline void set_release(Set *set) {
    if (set->created) {
        FL_THROW(set_release_bucket_not_allowed);
    }
    set_clear(set);
    if (set->buckets != NULL) {
        ARENA_FREE_THROW(set->arena, set->buckets);
        set->capacity = 0;
        set->buckets  = NULL;
    }
    set->initialized = false;
}

// Basic operations.
/**
 * @brief check if value is an element of the set.
 *
 * @param set
 * @param key
 * @return true
 * @return false
 * @throw fl_invalid_value if set->capacity == 0
 */
bool set_contains(Set const *set, void const *value);

/**
 * @brief check if the set is empty.
 *
 * @param set
 * @return true
 * @return false
 */
static inline bool set_is_empty(Set const *set) {
    return set->count == 0;
}

/**
 * @brief return the number of elements the hash table that backs the set can hold.
 *
 * @param set
 * @return size_t
 */
static inline size_t set_capacity(Set const *set) {
    return set->capacity;
}

/**
 * @brief return the number of elements in the set.
 *
 * @param set
 * @return size_t
 */
static inline size_t set_size(Set const *set) {
    return set->count;
}

/**
 * @brief Invoke fn(value, ctx) for every element stored in the set.
 *
 * The iteration order is unspecified. The callback must not insert or remove
 * elements from the set during iteration.
 *
 * @param set the set to iterate
 * @param fn  callback receiving a pointer to each stored value and ctx
 * @param ctx opaque context pointer forwarded to every fn call
 */
typedef void (*set_foreach_fn_t)(void const *value, void *ctx);

static inline void set_foreach(Set const *set, set_foreach_fn_t fn, void *ctx) {
    for (size_t i = 0; i < set->capacity; i++) {
        SetEntry *bucket = &set->buckets[i];
        SetEntry *entry  = SET_ENTRY_GET_NEXT(bucket);
        while (entry != bucket) {
            fn(entry + 1, ctx);
            entry = SET_ENTRY_GET_NEXT(entry);
        }
    }
}

/**
 * @brief Insert a value into the set, if it is not already there.
 *
 * When a value is added to a set, the value is copied. The set does not hold a reference
 * to the original value. It is an unchecked error to add a value to a set whose size in
 * bytes is not set->element_size.
 *
 * @param set
 * @param value
 * @return true if the value was added to the set
 * @return false if the value was already in the set
 * @throw fl_invalid_value if set->capacity == 0
 */
bool set_insert(Set *set, void const *value);

/**
 * @brief Remove a value from the set, if it is there.
 *
 * @param set
 * @param value
 * @return true if value was found and removed
 * @return false if value wasn't found
 */
bool set_remove(Set *set, void const *value);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_SET_H_
