/**
 * @file arena_trace_main.c
 * @author Douglas Cuthbertson
 * @brief Runs a scenario against the real arena and writes its steps as JSON.
 *
 * Reads a scenario from stdin and writes a trace to stdout:
 *
 *   arena_trace < large_bins.txt > large_bins.json
 *
 * A scenario has one command per line. Blank lines and lines starting with # are skipped.
 *
 *   title TEXT       names the scenario
 *   note TEXT        adds a caption for the reader
 *   quiet on|off     runs the commands between them without recording their steps
 *   NAME = malloc N  allocates N bytes and calls the block NAME
 *   free NAME        frees the block called NAME
 *
 * The trace is { "frames": [ ... ] }. A chunk is named by its offset from arena->base,
 * so a trace does not depend on where the system placed the arena. An arena frame and an
 * operation frame carry a "state" snapshot. A tree frame does not, and the reader keeps
 * the previous snapshot until the next one: see ArenaTraceEvent.
 *
 * Bin geometry depends on the size of a pointer, so only an x64 build matches the
 * demo's WebAssembly tree.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

// Must be defined before any headers are included: the first two select the platform
// implementations of the log and exception services, and the others compile the hooks.
#ifndef FL_PLATFORM_BUILD
#define FL_PLATFORM_BUILD
#endif
#define ARENA_TRACE_ENABLED
#define DST_TRACE_ENABLED

#include <faultline/fl_log.h> // LOG_* (selects flp_ backend via FL_PLATFORM_BUILD)

#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c" // FL_MALLOC/FL_FREE backing, needed by flp_file_service.c
#include "digital_search_tree.c"
#include "fl_exception.c"
#include "fl_threads.c"
#include "flp_file_service.c"
#include "flp_log_service.c"    // calls flp_stream_open/write/close
#include "flp_memory_service.c" // FL_MALLOC/FL_FREE backing (flp_malloc/flp_free)
#include "flp_stream_service.c"
#include "lock_os.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"

#include <faultline/fl_try.h> // FL_TRY, FL_CATCH_ALL, FL_END_TRY

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACE_COMMIT     (64 * 1024)
#define MAX_BLOCKS       64
#define MAX_NAME         16
#define MAX_LINE         512
#define MAX_TREE_DEPTH   64
#define EXIT_BAD_SCRIPT  2
#define EXIT_ARENA_THREW 3

typedef struct Block {
    char  name[MAX_NAME];
    void *mem;
} Block;

/// Indexed by ArenaTraceEvent.
static char const *const s_event_names[] = {
    "path_exact_fit",       "path_fast",      "path_next_small_bin",
    "path_large_for_small", "path_large_bin", "path_top",
    "path_extend",          "small_insert",   "large_insert",
    "large_remove",         "split",          "merge_back",
    "merge_forward",        "merge_top",      "allocated",
};

FL_STATIC_ASSERT(sizeof(s_event_names) / sizeof(s_event_names[0])
                     == ARENA_TRACE_EVENT_COUNT,
                 "s_event_names does not name every ArenaTraceEvent");

static Arena *s_arena;
static Block  s_blocks[MAX_BLOCKS];
static int    s_block_count;
static bool   s_quiet;
static bool   s_wrote_frame;
static int    s_script_line;

static void fail_script(char const *message) {
    fprintf(stderr, "arena_trace: line %d: %s\n", s_script_line, message);
    exit(EXIT_BAD_SCRIPT);
}

static unsigned long long chunk_offset(void const *chunk) {
    return (unsigned long long)((char const *)chunk - (char const *)s_arena->base);
}

static void write_string(char const *text) {
    putchar('"');

    for (; *text != '\0'; text++) {
        if (*text == '"' || *text == '\\') {
            putchar('\\');
        }
        putchar(*text);
    }

    putchar('"');
}

static void write_chunks_in_memory(void) {
    Chunk const *chunk = s_arena->base;
    bool         first = true;

    printf("\"heap\":[");

    while ((void const *)chunk < (void const *)s_arena->top) {
        printf("%s[%llu,%llu,%d]", first ? "" : ",", chunk_offset(chunk),
               (unsigned long long)CHUNK_SIZE(chunk), CHUNK_IS_INUSE(chunk) ? 1 : 0);
        first = false;
        chunk = CHUNK_NEXT(chunk);
    }

    printf("]");
}

static void write_small_bins(void) {
    bool first = true;

    printf("\"small\":[");

    for (u32 index = 0; index < ARENA_SMALL_BIN_COUNT; index++) {
        DList const *bin = &s_arena->small_bins[index];

        if (!ARENA_SMALL_MAP_IS_MARKED(s_arena, index)) {
            continue;
        }

        printf("%s{\"index\":%u,\"chunks\":[", first ? "" : ",", index);
        first = false;

        for (DList *entry = DLIST_NEXT(bin); entry != bin; entry = DLIST_NEXT(entry)) {
            printf("%s%llu", DLIST_NEXT(bin) == entry ? "" : ",",
                   chunk_offset(FREE_CHUNK_FROM_SIBLINGS(entry)));
        }

        printf("]}");
    }

    printf("]");
}

static void write_sibling_ids(DigitalSearchTree *node) {
    DigitalSearchTree *next = node;

    if (!DST_HAS_SIBLINGS(node)) {
        return;
    }

    for (next = DST_NEXT_SIBLING(node); next != node; next = DST_NEXT_SIBLING(next)) {
        printf("%s%llu", next == DST_NEXT_SIBLING(node) ? "" : ",", chunk_offset(next));
    }
}

/// Writes a node and its subtrees in the snapshot form that dst_wasm.js produces.
static void write_tree(DigitalSearchTree *node, char *path, int depth, bool *first) {
    if (depth >= MAX_TREE_DEPTH) {
        fail_script("tree is deeper than a size has bits");
    }

    path[depth] = '\0';
    printf("%s{\"id\":%llu,\"key\":%llu,\"path\":\"%s\",\"stack\":[", *first ? "" : ",",
           chunk_offset(node), (unsigned long long)CHUNK_SIZE(node), path);
    *first = false;

    write_sibling_ids(node);
    printf("]}");

    for (int side = 0; side < 2; side++) {
        if (node->child[side] != node) {
            path[depth] = side == 0 ? '0' : '1';
            write_tree(node->child[side], path, depth + 1, first);
        }
    }
}

static void write_large_bins(void) {
    char path[MAX_TREE_DEPTH + 1];
    bool first_bin = true;

    printf("\"large\":[");

    for (u32 index = 0; index < ARENA_LARGE_BIN_COUNT; index++) {
        bool first_node = true;

        if (s_arena->large_bins[index] == NULL) {
            continue;
        }

        printf("%s{\"index\":%u,\"nodes\":[", first_bin ? "" : ",", index);
        first_bin = false;
        write_tree(s_arena->large_bins[index], path, 0, &first_node);
        printf("]}");
    }

    printf("]");
}

static void write_state(void) {
    printf("\"state\":{\"top\":[%llu,%llu],", chunk_offset(s_arena->top),
           (unsigned long long)CHUNK_SIZE(s_arena->top));

    if (s_arena->fast == NULL) {
        printf("\"fast\":null,");
    } else {
        printf("\"fast\":[%llu,%llu],", chunk_offset(s_arena->fast),
               (unsigned long long)s_arena->fast_size);
    }

    printf("\"small_map\":\"%llx\",\"large_map\":\"%llx\",",
           (unsigned long long)s_arena->small_map,
           (unsigned long long)s_arena->large_map);
    write_chunks_in_memory();
    putchar(',');
    write_small_bins();
    putchar(',');
    write_large_bins();
    putchar('}');
}

static void begin_frame(char const *event) {
    printf("%s\n{\"event\":\"%s\"", s_wrote_frame ? "," : "", event);
    s_wrote_frame = true;
}

void dst_trace(DSTTraceEvent event, DigitalSearchTree const *node, size_t detail) {
    if (s_quiet) {
        return;
    }

    begin_frame(DST_TRACE_EVENT_NAMES[event]);
    printf(",\"node\":%llu,\"detail\":%llu}", chunk_offset(node),
           (unsigned long long)detail);
}

void arena_trace(Arena const *arena, ArenaTraceEvent event, void const *chunk, int line) {
    FL_UNUSED(arena);

    if (s_quiet) {
        return;
    }

    begin_frame(s_event_names[event]);
    printf(",\"line\":%d,", line);

    if (chunk != NULL) {
        printf("\"chunk\":%llu,\"size\":%llu,", chunk_offset(chunk),
               (unsigned long long)CHUNK_SIZE((Chunk const *)chunk));
    }

    write_state();
    putchar('}');
}

/// Text frames carry a snapshot too, because quiet commands may have run before them.
static void write_text_frame(char const *event, char const *text) {
    if (s_quiet) {
        return;
    }

    begin_frame(event);
    printf(",\"text\":");
    write_string(text);
    putchar(',');
    write_state();
    putchar('}');
}

static void write_op_frame(char const *event, char const *name, size_t request,
                           size_t size) {
    if (s_quiet) {
        return;
    }

    begin_frame(event);
    printf(",\"name\":");
    write_string(name);
    printf(",\"request\":%llu,\"size\":%llu,", (unsigned long long)request,
           (unsigned long long)size);
    write_state();
    putchar('}');
}

static Block *find_block(char const *name) {
    for (int i = 0; i < s_block_count; i++) {
        if (strcmp(s_blocks[i].name, name) == 0) {
            return &s_blocks[i];
        }
    }

    return NULL;
}

static void run_malloc(char const *name, char const *request_text) {
    char  *end     = NULL;
    size_t request = (size_t)strtoull(request_text, &end, 10);

    if (end == request_text || *end != '\0') {
        fail_script("malloc needs a size in bytes");
    }

    if (strlen(name) >= MAX_NAME) {
        fail_script("block name is too long");
    }

    if (find_block(name) != NULL) {
        fail_script("block name is already in use");
    }

    if (s_block_count == MAX_BLOCKS) {
        fail_script("too many blocks");
    }

    write_op_frame("op_malloc", name, request, CHUNK_SIZE_FROM_REQUEST(request));

    Block *block = &s_blocks[s_block_count++];
    block->mem   = arena_malloc_throw(s_arena, request, __FILE__, __LINE__);
    (void)memcpy(block->name, name, strlen(name) + 1);
}

static void run_free(char const *name) {
    Block *block = find_block(name);

    if (block == NULL) {
        fail_script("free names an unknown block");
    }

    size_t size = CHUNK_SIZE(CHUNK_FROM_MEMORY(block->mem));

    write_op_frame("op_free", name, 0, size);
    arena_free_throw(s_arena, block->mem, __FILE__, __LINE__);

    // A later malloc may reuse the name, and this block's memory is gone either way.
    *block = s_blocks[--s_block_count];
}

/// Returns the text after a leading word, or NULL if the line starts with another one.
static char *after_word(char *line, char const *word) {
    size_t length = strlen(word);

    if (strncmp(line, word, length) != 0 || line[length] != ' ') {
        return NULL;
    }

    return line + length + 1;
}

static void run_assignment(char *line) {
    char *name   = line;
    char *equals = strstr(line, " = ");

    if (equals == NULL) {
        fail_script("unknown command");
    }

    *equals = '\0';

    char *request = after_word(equals + 3, "malloc");

    if (request == NULL) {
        fail_script("only malloc can be assigned to a name");
    }

    run_malloc(name, request);
}

static void run_line(char *line) {
    char *text = NULL;

    line[strcspn(line, "\r\n")] = '\0';

    if (line[0] == '\0' || line[0] == '#') {
        return;
    }

    if ((text = after_word(line, "title")) != NULL) {
        write_text_frame("title", text);
    } else if ((text = after_word(line, "note")) != NULL) {
        write_text_frame("note", text);
    } else if ((text = after_word(line, "free")) != NULL) {
        run_free(text);
    } else if ((text = after_word(line, "quiet")) != NULL) {
        s_quiet = strcmp(text, "on") == 0;
    } else {
        run_assignment(line);
    }
}

int main(void) {
    char line[MAX_LINE];
    int volatile status = EXIT_SUCCESS;

    // The log goes to a file, because stdout carries the trace.
    flp_log_init_custom(LOG_LEVEL_WARN, "arena_trace.log");

    FL_TRY {
        s_arena = new_arena(TRACE_COMMIT, 0);
        printf("{\"frames\":[");

        while (fgets(line, sizeof line, stdin) != NULL) {
            s_script_line++;
            run_line(line);
        }

        printf("\n]}\n");

        if (!DLIST_IS_EMPTY(&s_arena->region_list)) {
            fprintf(stderr, "arena_trace: scenario left the first region, which a "
                            "chunk offset cannot name\n");
            status = EXIT_BAD_SCRIPT;
        }

        release_arena(&s_arena);
    }
    FL_CATCH_ALL {
        fprintf(stderr, "arena_trace: arena threw: %s\n", FL_REASON);
        status = EXIT_ARENA_THREW;
    }
    FL_END_TRY;

    return status;
}
