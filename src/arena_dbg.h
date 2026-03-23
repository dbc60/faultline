#ifndef ARENA_DBG_H_
#define ARENA_DBG_H_

/**
 * @file arena_dbg.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2024-12-01
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "arena_internal.h"      // for Arena
#include "chunk.h"               // for Chunk, FreeChunk
#include "digital_search_tree.h" // for DigitalSearchTree
#include <stdbool.h>             // for bool
#include <stddef.h>              // for size_t

// debug checks
#if defined(_DEBUG) || defined(DEBUG)
#define ARENA_CHECK_FREE_CHUNK(AR, CH, FILE, LINE) \
    dbg_check_free_chunk(AR, CH, FILE, LINE)
#define ARENA_CHECK_INUSE_CHUNK(AR, CH) dbg_check_inuse_chunk(AR, CH, __FILE__, __LINE__)
#define ARENA_CHECK_INUSE_CHUNK_FILE_LINE(AR, CH, FILE, LINE) \
    dbg_check_inuse_chunk(AR, CH, FILE, LINE)
#define ARENA_CHECK_TOP_CHUNK(AR) dbg_check_top_chunk(AR, __FILE__, __LINE__)
#define ARENA_CHECK_TOP_CHUNK_FILE_LINE(AR, FILE, LINE) \
    dbg_check_top_chunk(AR, FILE, LINE)
#define ARENA_CHECK_ALLOCATED_CHUNK(AR, CH, SZ, FILE, LINE) \
    dbg_check_allocated_chunk(AR, CH, SZ, FILE, LINE)
#define ARENA_CHECK_ARENA(AR, FILE, LINE) dbg_check_arena(AR, FILE, LINE)
#else
// Avoid warning "local variable is initialized but not referenced" by using the variable
// in a macro.
#define ARENA_CHECK_FREE_CHUNK(AR, CH, FILE, LINE) \
    do {                                           \
        FL_UNUSED(AR);                             \
        FL_UNUSED(CH);                             \
        FL_UNUSED(FILE);                           \
        FL_UNUSED(LINE);                           \
    } while (0)
#define ARENA_CHECK_INUSE_CHUNK(AR, CH) \
    do {                                \
        FL_UNUSED(AR);                  \
        FL_UNUSED(CH);                  \
    } while (0)
#define ARENA_CHECK_INUSE_CHUNK_FILE_LINE(AR, CH, FILE, LINE) \
    do {                                                      \
        FL_UNUSED(AR);                                        \
        FL_UNUSED(CH);                                        \
        FL_UNUSED(FILE);                                      \
        FL_UNUSED(LINE);                                      \
    } while (0)
#define ARENA_CHECK_TOP_CHUNK(AR) \
    do {                          \
        FL_UNUSED(AR);            \
    } while (0)
#define ARENA_CHECK_TOP_CHUNK_FILE_LINE(AR, FILE, LINE) \
    do {                                                \
        FL_UNUSED(AR);                                  \
        FL_UNUSED(FILE);                                \
        FL_UNUSED(LINE);                                \
    } while (0)
#define ARENA_CHECK_ALLOCATED_CHUNK(AR, CH, N, FILE, LINE) \
    do {                                                   \
        FL_UNUSED(AR);                                     \
        FL_UNUSED(CH);                                     \
        FL_UNUSED(FILE);                                   \
        FL_UNUSED(LINE);                                   \
    } while (0)
#define ARENA_CHECK_ARENA(AR, FILE, LINE) \
    do {                                  \
        FL_UNUSED(AR);                    \
        FL_UNUSED(FILE);                  \
        FL_UNUSED(LINE);                  \
    } while (0)
#endif // _DEBUG

#define ARENA_RELATIVE_ADDRESS(AR, ADDR) ((size_t)((char *)(ADDR) - (size_t)(AR)->base))

extern void dbg_check_top_chunk(Arena *arena, char const *file, int line);
extern void dbg_check_inuse_chunk(Arena *arena, Chunk *ch, char const *file, int line);
extern void dbg_check_free_chunk(Arena *arena, FreeChunk *ch, char const *file,
                                 int line);
extern void dbg_check_allocated_chunk(Arena *arena, Chunk *ch, size_t s,
                                      char const *file, int line);
extern void dbg_check_arena(Arena *arena, char const *file, int line);

extern void dbg_display_chunk(Arena *arean, Chunk *ch, char const *note, bool details,
                              char const *file, int line);
extern void arena_tree_dbg_display(Arena *arena, DigitalSearchTree *tc, char const *note,
                                   char const *file, int line);

#endif // ARENA_DBG_H_
