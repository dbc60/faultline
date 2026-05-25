/**
 * @file flp_memory_service_sa_test.c
 * @author Douglas Cuthbertson
 * @brief Standalone test for the platform-side memory service.
 * @version 0.1
 * @date 2026-05-19
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty (no dllimport on a locally-defined
 * function). Use /DDLL_BUILD instead when compiling into an actual DLL.
 *
 * Exit code: 0 = all passed, 1 = one or more failures.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

// CRT headers must come before fla_memory_service.h, which redefines malloc,
// calloc, free, realloc, and aligned_alloc as macros. If CRT headers are
// included after those macros are active, the compiler tries to declare e.g.
// "void* __cdecl malloc(size_t)" and the macro expansion produces an invalid
// declaration (C4229 / C2220 in MSVC).
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <faultline/arena.h>
#include <faultline/fault_injector.h>
#include <faultline/fl_try.h>
#include <flp_memory_service.h>

// Private headers — requires /I pointing to the src directory.
#include "fault_injector_internal.h"
#include "flp_memory_context.h"

// Must be last: redefines malloc/calloc/free/realloc to route through
// g_fla_memory_service once flp_init_memory_service has injected the service.
#include <faultline/fla_memory_service.h>

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

#define SECTION(name) fprintf(stdout, "  %s\n", (name))

int main(void) {
    fprintf(stdout, "flp_memory_service standalone tests\n");

    Arena          *arena = new_arena(0, 0);
    FaultInjector   fi    = {0};
    FLMemoryContext ctx   = {0};

    FL_TRY {
        fault_injector_init(&fi, arena);
        flp_init_memory_context(&ctx, arena, &fi);
        flp_init_memory_service(fla_set_memory_service, &ctx);

        SECTION("service injection");
        CHECK(g_fla_memory_service.ctx != NULL);
        CHECK(g_fla_memory_service.fl_aligned_alloc != NULL);
        CHECK(g_fla_memory_service.fl_calloc != NULL);
        CHECK(g_fla_memory_service.fl_free != NULL);
        CHECK(g_fla_memory_service.fl_malloc != NULL);
        CHECK(g_fla_memory_service.fl_realloc != NULL);

        SECTION("malloc and free");
        {
            void *p = malloc(64);
            CHECK(p != NULL);
            if (p != NULL) {
                free(p);
            }
        }

        SECTION("calloc");
        {
            int *arr = calloc(4, sizeof(int));
            CHECK(arr != NULL);
            if (arr != NULL) {
                CHECK(arr[0] == 0);
                CHECK(arr[3] == 0);
                free(arr);
            }
        }

        SECTION("realloc");
        {
            void *p = malloc(32);
            CHECK(p != NULL);
            if (p != NULL) {
                void *q = realloc(p, 64);
                CHECK(q != NULL);
                if (q != NULL) {
                    free(q);
                } else {
                    free(p);
                }
            }
        }

        SECTION("fault injection fails malloc");
        {
            fault_injector_enable(&fi);
            fault_injector_set_threshold(&fi, 1);
            void *p = malloc(32);
            CHECK(p == NULL);
            CHECK(fault_injector_triggered(&fi));
            fault_injector_disable(&fi);
        }
    }
    FL_CATCH_ALL {
        fprintf(stderr, "FAIL: unexpected exception\n");
        g_failures++;
    }
    FL_END_TRY;

    fault_injector_uninit(&fi);
    release_arena(&arena);

    if (g_failures == 0) {
        fprintf(stdout, "All tests passed.\n");
    } else {
        fprintf(stderr, "%d test(s) FAILED.\n", g_failures);
    }

    return g_failures > 0 ? 1 : 0;
}
