/**
 * @file command_help.c
 * @author Douglas Cuthbertson
 * @brief Implementation of "help" and "version" command handlers
 * @version 0.1
 * @date 2026-01-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "faultline_commands.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Display help for a specific command
 *
 * @param cmd the FormalCommand to display help for
 * @param show if true, also show subcommands
 */
static void display_command_help(FormalCommand const *cmd, bool show) {
    printf("Command: %s\n", cmd->name);
    printf("  %s\n\n", cmd->help);

    // Display options
    if (cmd->options != NULL) {
        printf("Options:\n");
        for (int i = 0;
             cmd->options[i].long_form != NULL || cmd->options[i].short_form != NULL;
             i++) {
            printf("  ");

            if (cmd->options[i].short_form != NULL) {
                printf("-%s", cmd->options[i].short_form);
                if (cmd->options[i].long_form != NULL) {
                    printf(", ");
                }
            }

            if (cmd->options[i].long_form != NULL) {
                printf("--%s", cmd->options[i].long_form);
            }

            if (cmd->options[i].requires_arg) {
                printf(" <value>");
            }

            printf("\n      %s\n", cmd->options[i].help);
        }
        printf("\n");
    }

    // Display positional arguments
    if (cmd->arguments != NULL) {
        printf("Arguments:\n");
        printf("  %s\n\n", cmd->arguments);
    }

    // Display subcommands
    if (show && cmd->subcommands != NULL) {
        printf("Subcommands:\n");
        for (int i = 0; cmd->subcommands[i].name != NULL; i++) {
            printf("  %-15s %s\n", cmd->subcommands[i].name, cmd->subcommands[i].help);
        }
        printf("\n");
    }
}

/**
 * @brief Handler for "help" command
 *
 * Displays help information for cmds
 * Positional argument (optional): command_name
 */
COMMAND_HANDLER(help_cmd) {
    // Get the command table
    FormalCommand const *cmds = get_faultline_commands();

    // If a command name was provided as argument, show help for that command
    if (cmd->argc > 0) {
        char const *cmd_name = cmd->args[0];

        // Find the command
        for (int i = 0; cmds[i].name != NULL; i++) {
            if (strcmp(cmds[i].name, cmd_name) == 0) {
                display_command_help(&cmds[i], true);
                return;
            }
        }

        // Command not found
        fprintf(stderr, "Unknown command: %s\n", cmd_name);
        return;
    }

    // No specific command requested, show general help
    printf("FaultLine - Fault Injection Testing Framework\n\n");
    printf("Usage: faultline <command> [options] [arguments]\n\n");
    printf("Commands:\n");

    for (int i = 0; cmds[i].name != NULL; i++) {
        printf("  %-15s %s\n", cmds[i].name, cmds[i].help);
    }

    printf("\nFor help on a specific command, use: faultline help <command>\n");
    printf("Examples:\n");
    printf("  faultline help run\n");
    printf("  faultline help show\n");
}

/**
 * @brief Handler for "version" command
 *
 * Displays version information
 */
COMMAND_HANDLER(version_cmd) {
    FL_UNUSED(cmd);
    printf("FaultLine version 0.2.0\n");
    printf("Fault Injection Testing Framework\n");
    printf("Copyright (c) 2025 Douglas Cuthbertson\n");
}
