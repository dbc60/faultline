/**
 * @file arena_dbg.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2024-12-01
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "arena_dbg.h"
#include <faultline/fl_abbreviated_types.h>        // for u32, flag64
#include <faultline/fl_macros.h>                   // for FL_THREAD_LOCAL
#include <stdio.h>                                 // for fprintf, stderr, snprintf
#include "arena_internal.h"                        // for Arena, ARENA_DST_TO_FREE_CHUNK
#include "chunk.h"                                 // for Chunk, CHUNK_SIZE, free_chu...
#include <faultline/dlist.h>                       // for DList, DLIST_PREVIOUS, DLIS...
#include "digital_search_tree.h"                   // for DigitalSearchTree, DST_NEXT...
#include <faultline/fl_exception_service_assert.h> // for FL_ASSERT_FILE_LINE, FL_ASS...
#include <faultline/fl_log.h>                      // for LOG_DEBUG
#include "index.h"                                 // for INDEX_BIN_TO_LOWER_LIMIT
#include "region.h"                                // for MEM_TO_REGION, Region
#include <faultline/size.h>                        // for SIZE_T_BITSIZE, SIZE_T_ONE

// Forward declaration to support recursive calls
void dbg_display_chunk(Arena *arena, Chunk *ch, char const *note, bool details,
                       char const *file, int line);

static void dbg_display_siblings(Arena *arena, FreeChunk *ch, char const *file,
                                 int line) {
    char const *base = (char *)arena->base;
    ptrdiff_t   rel  = (char *)ch - base; // relative address
    char        buf[48];

    if (free_chunk_next_sibling(ch) == free_chunk_previous_sibling(ch)) {
        if (ch != free_chunk_next_sibling(ch)) {
            snprintf(buf, sizeof buf, "    the sibling of 0x%08zx", rel);
            dbg_display_chunk(arena, (Chunk *)free_chunk_next_sibling(ch), buf, true,
                              file, line);
        }
    } else {
        snprintf(buf, sizeof buf, "    next sibling of 0x%08zx", rel);
        dbg_display_chunk(arena, (Chunk *)free_chunk_next_sibling(ch), buf, true, file,
                          line);

        snprintf(buf, sizeof buf, "    previous sibling of 0x%08zx", rel);
        dbg_display_chunk(arena, (Chunk *)free_chunk_previous_sibling(ch), buf, true,
                          file, line);
    }
}

/**
 * @brief display addresses relative to the arena
 *
 * @param ch
 * @param note
 */
void dbg_display_chunk(Arena *arena, Chunk *ch, char const *note, bool details,
                       char const *file, int line) {
    ptrdiff_t rel = (char *)(ch) - (char *)arena->base;
    if (note == NULL) {
        note = "chunk";
    }

    size_t sz   = CHUNK_SIZE(ch);
    size_t foot = free_chunk_footer_size((FreeChunk *)ch);

    fprintf(stderr, "DBG %s: arena:     0x%p\n", note, arena);
    fprintf(stderr, "DBG %s: arena top: 0x%08zx\n", note,
            (Chunk *)arena->top - arena->base);
    fprintf(stderr, "DBG %s:                   0x%08zx [%s: %d]\n", note, rel, file,
            line);
    fprintf(stderr, "DBG %s: size:               % 8zu\n", note, sz);
    fprintf(stderr, "DBG %s: footer size:        % 8zu\n", note, foot);
    if (!CHUNK_IS_INUSE(ch)) {
        FreeChunk *fch      = (FreeChunk *)ch;
        FreeChunk *prev_sib = free_chunk_previous_sibling(fch);
        FreeChunk *next_sib = free_chunk_next_sibling(fch);
        fprintf(stderr, "DBG %s: previous sibling: 0x%08zx\n", note,
                ARENA_RELATIVE_ADDRESS(arena, prev_sib));
        fprintf(stderr, "DBG %s: next sibling:     0x%08zx\n", note,
                ARENA_RELATIVE_ADDRESS(arena, next_sib));
    }
    fprintf(stderr, "DBG %s: in-use:           %s\n", note,
            CHUNK_IS_INUSE(ch) ? "true" : "false");
    fprintf(stderr, "DBG %s: prev in-use:      %s\n", note,
            CHUNK_IS_PREVIOUS_INUSE(ch) ? "true" : "false");
    fprintf(stderr, "\n");
    FL_ASSERT_TRUE(details && sz != foot);

    static int level;
    if (details && level == 0) {
        level++;
        dbg_display_siblings(arena, (FreeChunk *)ch, file, line);
        level--;
    }
}

/**
 * @brief check the properties of any chunk, whether free, in use, etc.
 *
 * - address is aligned
 * - address is between the base address and top
 *
 * @param arena the address of an arena
 * @param ch the address of a chunk
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 */
static void check_any_chunk(Arena *arena, Chunk *ch, char const *file, int line) {
    FL_ASSERT_FILE_LINE(CHUNK_IS_PREVIOUS_INUSE(arena->base), file, line);
    FL_ASSERT_FILE_LINE(CHUNK_IS_ALIGNED(ch), file, line);
    FL_ASSERT_FILE_LINE(arena_address_ok(arena, ch), file, line);
}

/**
 * @brief check the properties of the top chunk.
 *
 * - address is aligned
 * - address is >= base
 *
 * @param arena the address of the arena holding the chunk to check
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 */
void dbg_check_top_chunk(Arena *arena, char const *file, int line) {
    FreeChunk *top    = arena->top;
    size_t     sz     = CHUNK_SIZE(top);
    Region    *region = MEM_TO_REGION(arena);
    char      *end_committed;

    // end_committed is top + CHUNK_SIZE(top) + CHUNK_SENTINEL_SIZE if top != sentinel,
    // otherwise it's top + CHUNK_SIZE(top)
    if (CHUNK_SIZE(top) == CHUNK_SENTINEL_SIZE) {
        end_committed = (char *)top + CHUNK_SIZE(top);
    } else {
        end_committed = (char *)top + CHUNK_SIZE(top) + CHUNK_SENTINEL_SIZE;
    }
    FL_ASSERT_FILE_LINE(region->end_committed == end_committed, file, line);

    FL_ASSERT_FILE_LINE(CHUNK_IS_PREVIOUS_INUSE(arena->base), file, line);
    FL_ASSERT_FILE_LINE(CHUNK_IS_ALIGNED(top), file, line);
    FL_ASSERT_FILE_LINE((Chunk *)top >= arena->base, file, line);
    FL_ASSERT_FILE_LINE(CHUNK_IS_PREVIOUS_INUSE(top), file, line);

    // The sentinel's size is always zero, so if CHUNK_SIZE(top) is zero it is pointing
    // to the sentinel. If top is not zero bytes, then the sentinel is the next chunk.
    // The sentinel is always in use to prevent it from being consolidated with top.
    Chunk *sentinel;
    if (sz == CHUNK_SENTINEL_SIZE) {
        sentinel = (Chunk *)top;
        FL_ASSERT_FILE_LINE(CHUNK_IS_PREVIOUS_INUSE(sentinel), file, line);
    } else {
        sentinel = CHUNK_NEXT(top);
        FL_ASSERT_FILE_LINE(!CHUNK_IS_INUSE(top), file, line);
    }
    FL_ASSERT_FILE_LINE(CHUNK_IS_INUSE(sentinel), file, line);
}

/**
 * @brief check the properties of in-use chunks
 *
 * @param arena the address of an arena
 * @param ch the address of a chunk
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 */
void dbg_check_inuse_chunk(Arena *arena, Chunk *ch, char const *file, int line) {
    check_any_chunk(arena, ch, file, line);
    FL_ASSERT_FILE_LINE(CHUNK_IS_INUSE(ch), file, line);
    Chunk *next = CHUNK_NEXT(ch);

    // ch must be in use, so next should show that its previous chunk (ch) is in use.
    FL_ASSERT_FILE_LINE(CHUNK_IS_PREVIOUS_INUSE(next), file, line);

    // If the previous chunk is NOT in use, then this verifies that the previous chunk's
    // size matches the size in its footer.
    FL_ASSERT_FILE_LINE(CHUNK_IS_PREVIOUS_INUSE(ch)
                            || ch == CHUNK_NEXT(chunk_get_previous(ch)),
                        file, line);
}

/**
 * @brief check the properties of free chunks.
 *
 * @param arena the address of an arena
 * @param ch the address of a chunk
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 */
void dbg_check_free_chunk(Arena *arena, FreeChunk *ch, char const *file, int line) {
    size_t sz   = CHUNK_SIZE(ch);
    Chunk *next = CHUNK_NEXT(ch);

    check_any_chunk(arena, (Chunk *)ch, file, line);
    FL_ASSERT_FILE_LINE(!CHUNK_IS_INUSE(ch), file, line);
    FL_ASSERT_FILE_LINE(!CHUNK_IS_PREVIOUS_INUSE(next), file, line);
    if (ch != arena->fast && ch != arena->top) {
        FL_ASSERT_FILE_LINE(sz >= CHUNK_MIN_SIZE, file, line);
        FL_ASSERT_FILE_LINE((sz & CHUNK_ALIGNMENT_MASK) == 0, file, line);
        FL_ASSERT_FILE_LINE(CHUNK_IS_ALIGNED(CHUNK_TO_MEMORY(ch)), file, line);
        FL_ASSERT_FILE_LINE(chunk_previous_size(next) == sz, file, line);
        FL_ASSERT_FILE_LINE(CHUNK_IS_PREVIOUS_INUSE(ch), file, line);
        FL_ASSERT_FILE_LINE(next == (Chunk *)arena->top || CHUNK_IS_INUSE(next), file,
                            line);
        FL_ASSERT_FILE_LINE(free_chunk_previous_sibling(free_chunk_next_sibling(ch))
                                == ch,
                            file, line);
        FL_ASSERT_FILE_LINE(free_chunk_next_sibling(free_chunk_previous_sibling(ch))
                                == ch,
                            file, line);
    }
}

/**
 * @brief check properties of allocated chunks at the point they are allocated.
 *
 * @param arena the address of an arena
 * @param mem an address of allocated memory
 * @param s the size of the allocated memory block
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 */
void dbg_check_allocated_chunk(Arena *arena, Chunk *ch, size_t s, char const *file,
                               int line) {
    if (ch != NULL) {
        size_t sz = CHUNK_SIZE(ch);
        LOG_DEBUG("Arena Debug", "Check in-use");
        dbg_check_inuse_chunk(arena, ch, file, line);
        LOG_DEBUG("Arena Debug", "Assert alignment");
        FL_ASSERT_FILE_LINE((sz & CHUNK_ALIGNMENT_MASK) == 0, file, line);
        LOG_DEBUG("Arena Debug", "Assert min size");
        FL_ASSERT_FILE_LINE(sz >= CHUNK_MIN_SIZE, file, line);
        LOG_DEBUG("Arena Debug", "Assert sufficient size");
        FL_ASSERT_FILE_LINE(sz >= s, file, line);
        LOG_DEBUG("Arena Debug", "Assert max size");
        FL_ASSERT_FILE_LINE(sz < (s + CHUNK_MIN_SIZE), file, line);
    }
}

/**
 * @brief check a tree and its subtrees
 *
 * @param arena the address of an arena
 * @param t the address of a tree
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 */
static void check_tree(Arena *arena, DigitalSearchTree *t, char const *file, int line) {
    DigitalSearchTree *head  = NULL;
    DigitalSearchTree *u     = t;
    size_t             tsize = CHUNK_SIZE(t);
    u32                idx;

    // compute the tree index
    INDEX_BY_VALUE64(tsize, ARENA_LARGE_BIN_COUNT, ARENA_LOG2_MIN_LARGE_CHUNK, idx);
    FL_ASSERT_FILE_LINE(tsize >= ARENA_MIN_LARGE_CHUNK, file, line);
    FL_ASSERT_FILE_LINE(tsize
                            >= INDEX_BIN_TO_LOWER_LIMIT(idx, ARENA_LOG2_MIN_LARGE_CHUNK),
                        file, line);
    FL_ASSERT_FILE_LINE((idx == ARENA_LARGE_BIN_COUNT - 1)
                            || (tsize
                                < INDEX_BIN_TO_LOWER_LIMIT(idx + 1,
                                                           ARENA_LOG2_MIN_LARGE_CHUNK)),
                        file, line);
    do { /* traverse through the chain of same-sized nodes */
        FreeChunk *ch = (FreeChunk *)ARENA_DST_TO_FREE_CHUNK(u);
        check_any_chunk(arena, (Chunk *)ch, file, line);
        FL_ASSERT_FILE_LINE(CHUNK_SIZE(u) == tsize, file, line);
        FL_ASSERT_FILE_LINE(!CHUNK_IS_INUSE(ch), file, line);
        FL_ASSERT_FILE_LINE(!CHUNK_IS_PREVIOUS_INUSE(CHUNK_NEXT(ch))
                                || ch != chunk_get_previous(CHUNK_NEXT(ch)),
                            file, line);
        FL_ASSERT_FILE_LINE(free_chunk_previous_sibling(free_chunk_next_sibling(ch))
                                == ch,
                            file, line);
        FL_ASSERT_FILE_LINE(free_chunk_next_sibling(free_chunk_previous_sibling(ch))
                                == ch,
                            file, line);
        if (u->parent == u) {
            FL_ASSERT_FILE_LINE(u->child[0] == u, file, line);
            FL_ASSERT_FILE_LINE(u->child[1] == u, file, line);
        } else {
            // only one node in the sibling list has a parent
            FL_ASSERT_FILE_LINE(head == NULL, file, line);
            head = u;
            FL_ASSERT_FILE_LINE(u->parent != u, file, line);
            FL_ASSERT_FILE_LINE(u->parent->child[0] == u || u->parent->child[1] == u
                                    || u->parent == u,
                                file, line);
            if (u->child[0] != u) {
                FL_ASSERT_FILE_LINE(u->child[0]->parent == u, file, line);
                FL_ASSERT_FILE_LINE(u->child[0] != u, file, line);
                check_tree(arena, u->child[0], file, line);
            }
            if (u->child[1] != u) {
                FL_ASSERT_FILE_LINE(u->child[1]->parent == u, file, line);
                FL_ASSERT_FILE_LINE(u->child[1] != u, file, line);
                check_tree(arena, u->child[1], file, line);
            }
            if (u->child[0] != u && u->child[1] != u) {
                FL_ASSERT_FILE_LINE(CHUNK_SIZE(u->child[0]) < CHUNK_SIZE(u->child[1]),
                                    file, line);
            }
        }
        u = DST_NEXT_SIBLING(u);
    } while (u != t);
    FL_ASSERT_FILE_LINE(head != 0, file, line);
}

/**
 * @brief check all the chunks in a tree bin
 *
 * @param arena the address of an arena
 * @param i the index of a large bin
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 */
static void check_tree_bin(Arena *arena, u32 i, char const *file, int line) {
    DigitalSearchTree **bin   = ARENA_LARGE_BIN_AT(arena, i);
    DigitalSearchTree  *t     = *bin;
    bool                empty = ARENA_LARGE_MAP_IS_MARKED(arena, i);
    if (t == NULL) {
        FL_ASSERT_FILE_LINE(empty, file, line);
    }
    if (!empty) {
        check_tree(arena, t, file, line);
    }
}

/**
 * @brief check all the chunks in a small bin
 *
 * @param arena the address of an arena
 * @param i the index of a small bin
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 */
static void check_small_bin(Arena *arena, u32 i, char const *file, int line) {
    DList     *b     = ARENA_SMALL_BIN_AT(arena, i);
    DList     *node  = DLIST_PREVIOUS(b);
    FreeChunk *ch    = FREE_CHUNK_FROM_SIBLINGS(node);
    bool       empty = (arena->small_map && (1U << i)) == 0;
    if (node == b) {
        FL_ASSERT_FILE_LINE(empty, file, line);
    }

    if (!empty) {
        for (; node != b; node = DLIST_PREVIOUS(node)) {
            ch          = FREE_CHUNK_FROM_SIBLINGS(node);
            size_t size = CHUNK_SIZE(ch);
            // each chunk claims to be free
            dbg_check_free_chunk(arena, ch, file, line);
            // chunk belongs in bin
            FL_ASSERT_FILE_LINE(ARENA_SMALL_INDEX(size) == i, file, line);
            FL_ASSERT_FILE_LINE(DLIST_PREVIOUS(node) == b
                                    || CHUNK_SIZE(free_chunk_previous_sibling(ch))
                                           == CHUNK_SIZE(ch),
                                file, line);
            // chunk is followed by an in-use chunk
            Chunk *q = CHUNK_NEXT(ch);
            dbg_check_inuse_chunk(arena, q, file, line);
        }
    }
}

/**
 * @brief find chunk ch in a bin.
 *
 * @param arena the address of an arena
 * @param ch the address of the chunk to find in the arena.
 * @return  true if ch is in a bin and false otherwise.
 */
static bool find_bin(Arena *arena, FreeChunk *ch) {
    size_t size = CHUNK_SIZE(ch);

    if (ARENA_IS_SIZE_SMALL(size)) {
        u32    sidx = ARENA_SMALL_INDEX(size);
        DList *b    = ARENA_SMALL_BIN_AT(arena, sidx);
        if (ARENA_SMALL_MAP_IS_MARKED(arena, sidx)) {
            DList *node = b;
            do {
                if (node == FREE_CHUNK_TO_SIBLINGS(ch)) {
                    return true;
                }
            } while ((node = DLIST_NEXT(node)) != b);
        }
    } else {
        u32 tidx;
        // compute the tree index
        INDEX_BY_VALUE64(size, ARENA_LARGE_BIN_COUNT, ARENA_LOG2_MIN_LARGE_CHUNK, tidx);
        if (ARENA_LARGE_MAP_IS_MARKED(arena, tidx)) {
            DigitalSearchTree *t          = *ARENA_LARGE_BIN_AT(arena, tidx);
            flag64             left_shift = ARENA_LEFT_SHIFT(tidx);
            size_t             size_bits  = size << left_shift;
            // N.B.: this is how to traverse a tree when parent and child pointers can't
            // be NULL/zero.
            DigitalSearchTree *next
                = t->child[(size_bits >> (SIZE_T_BITSIZE - SIZE_T_ONE)) & 1];
            while (t != next && CHUNK_SIZE(t) != size) {
                t = next;
                size_bits <<= 1;
                next = t->child[(size_bits >> (SIZE_T_BITSIZE - SIZE_T_ONE)) & 1];
            }
            if (t != next) {
                DigitalSearchTree *u = t;
                do {
                    if (u == ARENA_CHUNK_TO_DST(ch)) {
                        return true;
                    }
                } while ((u = ARENA_CHUNK_TO_DST(
                              free_chunk_next_sibling(ARENA_DST_TO_FREE_CHUNK(u))))
                         != t);
            }
        }
    }

    return false;
}

/**
 * @brief check each chunk and return the total number of bytes checked.
 *
 * @param arena the address of an arena
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 * @return the number of bytes from all of the checked chunks.
 */
static size_t dbg_traverse_and_check(Arena *arena, char const *file, int line) {
    size_t sum   = CHUNK_SIZE(arena->top);
    Chunk *q     = arena->base;
    Chunk *lastq = NULL;
    FL_ASSERT_FILE_LINE(CHUNK_IS_PREVIOUS_INUSE(q), file, line);
    while (q != (Chunk *)arena->top) {
        sum += CHUNK_SIZE(q);
        if (CHUNK_IS_INUSE(q)) {
            dbg_check_inuse_chunk(arena, q, file, line);
        } else {
            FL_ASSERT_FILE_LINE((FreeChunk *)q == arena->fast
                                    || find_bin(arena, (FreeChunk *)q),
                                file, line);
            // not 2 consecutive free
            FL_ASSERT_FILE_LINE(lastq == NULL || CHUNK_IS_INUSE(lastq), file, line);
            dbg_check_free_chunk(arena, (FreeChunk *)q, file, line);
        }

        lastq = q;
        q     = CHUNK_NEXT(q);
    }

    return sum;
}

/**
 * @brief check all of the properties of an arena
 *
 * @param arena the address of an arena
 * @param file the file path from which this function was called.
 * @param line the line number in the file on which this function was called.
 */
void dbg_check_arena(Arena *arena, char const *file, int line) {
    FL_ASSERT_FILE_LINE(CHUNK_IS_PREVIOUS_INUSE(arena->base), file, line);

    // check bins
    for (u32 i = 0; i < ARENA_SMALL_BIN_COUNT; ++i) {
        check_small_bin(arena, i, file, line);
    }
    for (u32 i = 0; i < ARENA_LARGE_BIN_COUNT; ++i) {
        check_tree_bin(arena, i, file, line);
    }

    // check the fast node
    if (arena->fast != NULL) {
        check_any_chunk(arena, (Chunk *)arena->fast, file, line);
        FL_ASSERT_FILE_LINE(arena->fast_size == CHUNK_SIZE(arena->fast), file, line);
        FL_ASSERT_FILE_LINE(arena->fast_size >= CHUNK_MIN_SIZE, file, line);
        FL_ASSERT_FILE_LINE(!find_bin(arena, arena->fast), file, line);
    }

    // check top chunk
    dbg_check_top_chunk(arena, file, line);
    FL_ASSERT_FILE_LINE(CHUNK_SIZE(arena->top) > 0, file, line);
    FL_ASSERT_FILE_LINE(!find_bin(arena, arena->top), file, line);

    size_t total = dbg_traverse_and_check(arena, file, line);
    FL_ASSERT_FILE_LINE(total <= arena->footprint, file, line);
    FL_ASSERT_FILE_LINE(arena->footprint <= arena->max_footprint, file, line);
}

static void arena_tree_dbg_display_relatives(Arena *arena, DigitalSearchTree *tc,
                                             char const *file, int line) {
    char const *base = (char const *)arena->base;
    ptrdiff_t   rel  = (char *)tc - base; // relative address
    char        buf[48];

    if (tc != tc->parent) {
        snprintf(buf, sizeof buf, "    parent of 0x%08zx", rel);
        arena_tree_dbg_display(arena, tc->parent, buf, file, line);
    }
    if (tc != tc->child[0]) {
        snprintf(buf, sizeof buf, "    left child of 0x%08zx", rel);
        arena_tree_dbg_display(arena, tc->child[0], buf, file, line);
    }
    if (tc != tc->child[1]) {
        snprintf(buf, sizeof buf, "    right child of 0x%08zx", rel);
        arena_tree_dbg_display(arena, tc->child[1], buf, file, line);
    }
    if (DST_NEXT_SIBLING(tc) == DST_PREV_SIBLING(tc)) {
        if (tc != DST_NEXT_SIBLING(tc)) {
            snprintf(buf, sizeof buf, "    the sibling of 0x%08zx", rel);
            arena_tree_dbg_display(arena, DST_NEXT_SIBLING(tc), buf, file, line);
        }
    } else {
        snprintf(buf, sizeof buf, "    next sibling of 0x%08zx", rel);
        arena_tree_dbg_display(arena, DST_NEXT_SIBLING(tc), buf, file, line);

        snprintf(buf, sizeof buf, "    previous sibling of 0x%08zx", rel);
        arena_tree_dbg_display(arena, DST_PREV_SIBLING(tc), buf, file, line);
    }
}

void arena_tree_dbg_display(Arena *arena, DigitalSearchTree *tc, char const *note,
                            char const *file, int line) {
    char const *base = (char const *)arena->base;
    if (note == NULL) {
        note = "tc";
    }

    size_t sz   = CHUNK_SIZE(tc);
    size_t foot = *free_chunk_get_footer(ARENA_DST_TO_FREE_CHUNK(tc));

    DigitalSearchTree *prev_sib = DST_PREV_SIBLING(tc);
    DigitalSearchTree *next_sib = DST_NEXT_SIBLING(tc);
    fprintf(stderr, "DBG %s: arena top: 0x%08zx\n", note, (char *)arena->top - base);
    fprintf(stderr, "DBG %s:                   0x%08zx [%s: %d]\n", note,
            (char *)tc - base, file, line);
    fprintf(stderr, "DBG %s: size:               % 8zu\n", note, sz);
    fprintf(stderr, "DBG %s: footer size:        % 8zu\n", note, foot);
    fprintf(stderr, "DBG %s: previous sibling: 0x%08zx\n", note,
            (char *)prev_sib - base);
    fprintf(stderr, "DBG %s: next sibling:     0x%08zx\n", note,
            (char *)next_sib - base);
    fprintf(stderr, "DBG %s: left-child:       0x%08zx\n", note,
            (char *)tc->child[0] - base);
    fprintf(stderr, "DBG %s: right-child:      0x%08zx\n", note,
            (char *)tc->child[1] - base);
    fprintf(stderr, "DBG %s: parent:           0x%08zx\n", note,
            (char *)tc->parent - base);
    fprintf(stderr, "DBG %s: in-use:           %s\n", note,
            CHUNK_IS_INUSE(ARENA_DST_TO_FREE_CHUNK(tc)) ? "true" : "false");
    fprintf(stderr, "DBG %s: prev in-use:      %s\n", note,
            CHUNK_IS_PREVIOUS_INUSE(ARENA_DST_TO_FREE_CHUNK(tc)) ? "true" : "false");
    fprintf(stderr, "\n");

    FL_ASSERT_NEQ_SIZE_T(sz, foot);

    static FL_THREAD_LOCAL int level;
    if (level == 0) {
        level++;
        arena_tree_dbg_display_relatives(arena, tc, file, line);
        level--;
    }
}
