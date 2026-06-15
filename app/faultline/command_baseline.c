/**
 * @file command_baseline.c
 * @author Douglas Cuthbertson
 * @brief Implementation of the "baseline" command and its subcommands
 * @version 0.1
 * @date 2026-06-15
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "faultline_commands.h"
#include <faultline_sqlite.h>
#include <stdio.h>

/**
 * @brief Handler for "baseline reset"
 *
 * Re-pins each test's regression baseline to its latest observation, so that
 * `show regressions` compares against the current state. Scoped by the optional
 * --suite and --test filters.
 */
COMMAND_HANDLER(baseline_reset_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    char const *suite = get_string_option(cmd, "suite", NULL);
    char const *test  = get_string_option(cmd, "test", NULL);

    int n = faultline_reset_baselines(ectx->db, suite, test);
    if (n < 0) {
        printf("Error re-pinning baselines\n");
    } else {
        printf("Re-pinned %d baseline%s to the latest run.\n", n, n == 1 ? "" : "s");
    }
}

/**
 * @brief Handler for the top-level "baseline" command
 *
 * Delegates to the subcommand handler, propagating the full ExecutionContext so
 * subcommand handlers can access db, fctx, arena, etc.
 */
COMMAND_HANDLER(baseline_cmd) {
    if (cmd->subcommand != NULL) {
        ExecutionContext *parent  = (ExecutionContext *)cmd;
        ExecutionContext  sub_ctx = *parent;
        sub_ctx.cmd               = *cmd->subcommand;
        sub_ctx.cmd.command->handler((RuntimeCommand *)&sub_ctx);
    } else {
        fprintf(stderr, "Error: 'baseline' requires a subcommand (reset)\n");
        fprintf(stderr, "Try 'faultline help baseline' for usage information\n");
    }
}
