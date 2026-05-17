/**
 * @file set.c
 * @author Douglas Cuthbertson
 * @brief a set implementation based on the Fowler/Noll/Vo hash algorithm and
 * specifically designed for sets of uintptr_t values, like memory addresses.
 * @version 0.1
 * @date 2025-03-27
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/dlist.h>                // for DLIST_INIT, DLIST_REMOVE_SIMPLE
#include <faultline/set.h>                  // for Set, SetEntry, SET_ENTRY_GET_NEXT
#include <faultline/fl_abbreviated_types.h> // for u64
#include <faultline/fl_exception_service.h> // for fl_invalid_value, fl_internal_error
#include <faultline/fl_try.h>               // for FL_THROW
#include <fnv/FNV64.h>                      // for FNV64block
#include <fnv/FNVErrorCodes.h>              // for fnvSuccess
#include <faultline/arena.h>                // for ARENA_FREE_THROW, ARENA_CALLOC_THROW
#include <faultline/size.h>                 // for SIZE_T_SIZE
#include <stdbool.h>                        // for bool, false, true
#include <stdint.h>                         // for uint8_t
#include <string.h>                         // for size_t, memcmp, memcpy
#include "bits.h"                           // for ALIGN_UP
#include <faultline/fl_exception_types.h>   // for FLExceptionReason

FLExceptionReason set_release_bucket_not_allowed
    = "cannot release bucket memory for an allocated set, call set_clear() instead";

u64 set_do_hash(Set const *set, void const *value) {
    u64 hash   = 0;
    int result = FNV64block(value, (long)(set->element_size), (uint8_t *)&hash);

    if (result != fnvSuccess) {
        FL_THROW(fl_internal_error);
    }

    return hash;
}

bool set_compare(Set const *set, void const *a, void const *b) {
    return memcmp(a, b, set->element_size) == 0;
}

Set *new_set_custom(Arena *arena, size_t capacity, size_t element_size, hash_fn_t hash,
                    compare_fn_t compare) {
    if (capacity == 0) {
        capacity = SET_DEFAULT_CAPACITY;
    }

    element_size = ALIGN_UP(element_size, SIZE_T_SIZE);

    // Allocate a Set plus capacity buckets where each bucket is a SetEntry
    Set *set = ARENA_CALLOC_THROW(arena, 1, sizeof(*set) + capacity * sizeof(SetEntry));
    set->buckets = (SetEntry *)(set + 1);

    for (size_t i = 0; i < capacity; i++) {
        DLIST_INIT(&set->buckets[i].link);
    }

    set_initialize(set, arena, capacity, element_size, hash, compare, true);
    return set;
}

/**
 * @brief check each bucket and free the entries in it.
 *
 * @param set
 */
void destroy_set(Set *set) {
    for (size_t i = 0; i < set->capacity; i++) {
        SetEntry *bucket = &set->buckets[i];
        while (!DLIST_IS_EMPTY(&bucket->link)) {
            // get the first entry on the collision list
            SetEntry *entry = SET_ENTRY_GET_NEXT(bucket);
            DLIST_REMOVE_SIMPLE(&entry->link);
            ARENA_FREE_THROW(set->arena, entry);
        }
    }
    if (set->created) {
        // release the set which includes its buckets
        ARENA_FREE_THROW(set->arena, set);
    } else {
        // release the buckets and uninitialize the set
        ARENA_FREE_THROW(set->arena, set->buckets);
        set->initialized = false;
    }
}

static bool contains(Set const *set, void const *value, u64 key) {
    if (set->capacity == 0) {
        FL_THROW(fl_invalid_value);
    }
    size_t    index  = key % set->capacity;
    SetEntry *bucket = &set->buckets[index];
    SetEntry *entry  = (SetEntry *)DLIST_NEXT(&bucket->link);

    while (entry != bucket) {
        if (set->compare_fn(set, entry + 1, value)) {
            // found it
            return true;
        }
        entry = SET_ENTRY_GET_NEXT(entry);
    }

    return false;
}

bool set_contains(Set const *set, void const *value) {
    u64 key = set->hash_fn(set, value);

    return contains(set, value, key);
}

bool set_insert(Set *set, void const *value) {
    if (set->capacity == 0) {
        FL_THROW(fl_invalid_value);
    }
    u64       key            = set->hash_fn(set, value);
    size_t    index          = key % set->capacity;
    SetEntry *bucket         = &set->buckets[index];
    bool      already_in_set = contains(set, value, key);

    if (!already_in_set) {
        SetEntry *entry
            = ARENA_MALLOC_THROW(set->arena, sizeof *entry + set->element_size);
        DLIST_INIT(&entry->link);
        memcpy(entry + 1, value, set->element_size);
        DLIST_INSERT_NEXT(&bucket->link, &entry->link);
        set->count++;
        return true;
    }

    return false;
}

/**
 * @brief Remove a value from the set, if it is there.
 *
 * @param set
 * @param value
 */
bool set_remove(Set *set, void const *value) {
    if (set->capacity > 0 && set->count > 0) {
        u64       key    = set->hash_fn(set, value);
        size_t    index  = key % set->capacity;
        SetEntry *bucket = &set->buckets[index];
        SetEntry *entry  = SET_ENTRY_GET_NEXT(bucket);

        while (entry != bucket && !set->compare_fn(set, entry + 1, value)) {
            entry = SET_ENTRY_GET_NEXT(entry);
        }

        if (entry != bucket) {
            DLIST_REMOVE_SIMPLE(&entry->link);
            ARENA_FREE_THROW(set->arena, entry);
            set->count--;
            return true;
        }
    }

    return false;
}
