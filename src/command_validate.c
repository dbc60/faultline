/**
 * @file command_validate.c
 * @author Douglas Cuthbertson
 * @brief Semantic validation for command-line commands
 * @version 0.1
 * @date 2026-01-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "command.h"
#include <faultline/fl_memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Add an option to a RuntimeCommand's options array
 *
 * This is used for auto-implication (e.g., adding --failures when --type is present)
 *
 * @param cmd the RuntimeCommand to modify
 * @param option the FormalOption to add
 * @param arg the option's argument (or NULL if none)
 */
static void add_option(RuntimeCommand *cmd, FormalOption const *option,
                       char const *arg) {
    if (cmd == NULL || option == NULL) {
        return;
    }

    // Count existing options (sentinel: .option == NULL)
    int option_count = 0;
    if (cmd->options != NULL) {
        while (cmd->options[option_count].option != NULL) {
            option_count++;
        }
    }

    // Allocate new array with room for one more option plus sentinel
    RuntimeOption *new_options = FL_MALLOC(sizeof(RuntimeOption) * (option_count + 2));

    // Copy existing options
    for (int i = 0; i < option_count; i++) {
        new_options[i] = cmd->options[i];
    }

    // Add new option
    new_options[option_count].option     = option;
    new_options[option_count].arg        = arg;
    new_options[option_count + 1].option = NULL; // sentinel
    new_options[option_count + 1].arg    = NULL;

    // Update command's options array
    cmd->options = new_options;
}

/**
 * @brief Find a FormalOption by name in a command's options
 *
 * @param cmd the FormalCommand to search
 * @param option_name the long-form name of the option (without --)
 * @return FormalOption const * the matching option, or NULL if not found
 */
static FormalOption const *find_formal_option(FormalCommand const *cmd,
                                              char const          *option_name) {
    if (cmd == NULL || cmd->options == NULL) {
        return NULL;
    }

    for (int i = 0;
         cmd->options[i].long_form != NULL || cmd->options[i].short_form != NULL; i++) {
        if (cmd->options[i].long_form != NULL
            && strcmp(cmd->options[i].long_form, option_name) == 0) {
            return &cmd->options[i];
        }
    }

    return NULL;
}

/**
 * @brief Validate that a RuntimeCommand makes semantic sense
 *
 * Checks:
 * - Required positional arguments are present
 * - Options are valid for this command
 * - Option arguments are present when required
 * - Auto-implies --failures for failure-specific options
 *
 * @param cmd the RuntimeCommand to validate
 * @param error_msg output parameter for error message (allocated on arena)
 * @return true if valid, false otherwise
 */
bool validate_command(RuntimeCommand const *cmd, char **error_msg) {
    if (cmd == NULL) {
        if (error_msg != NULL) {
            *error_msg = "NULL command";
        }
        return false;
    }

    // If there's a subcommand, validate it recursively
    if (cmd->subcommand != NULL) {
        return validate_command(cmd->subcommand, error_msg);
    }

    // Check for failure-specific options and auto-imply --failures
    // Note: We need to cast away const to modify the command
    // This is acceptable during validation phase before execution
    bool has_type        = has_option(cmd, "type");
    bool has_phase       = has_option(cmd, "phase");
    bool has_with_source = has_option(cmd, "with-source");
    bool has_failures    = has_option(cmd, "failures");

    if ((has_type || has_phase || has_with_source) && !has_failures) {
        // Auto-imply --failures
        FormalOption const *failures_opt = find_formal_option(cmd->command, "failures");
        if (failures_opt != NULL) {
            // Cast away const - this is safe during validation
            RuntimeCommand *mutable_cmd = (RuntimeCommand *)cmd;
            add_option(mutable_cmd, failures_opt, NULL);
        }
    }

    // Command-specific validation rules
    char const *cmd_name = cmd->command->name;

    // Validation for "result" subcommand (show result <id>)
    if (strcmp(cmd_name, "result") == 0) {
        // Requires exactly one positional argument (the ID)
        if (cmd->argc != 1) {
            if (error_msg != NULL) {
                char *msg = FL_MALLOC(128);
                snprintf(
                    msg, 128,
                    "Command 'show result' requires exactly one argument (result ID)");
                *error_msg = msg;
            }
            return false;
        }

        // --limit doesn't make sense for single item
        if (has_option(cmd, "limit")) {
            if (error_msg != NULL) {
                *error_msg = "Option '--limit' is not valid for 'show result' (single "
                             "item command)";
            }
            return false;
        }

        // --failures doesn't make sense for single item details
        if (has_option(cmd, "failures")) {
            if (error_msg != NULL) {
                *error_msg = "Option '--failures' is not valid for 'show result' (use "
                             "'show results --failures' instead)";
            }
            return false;
        }

        // Failure-specific options don't make sense for single item
        if (has_type || has_phase || has_with_source) {
            if (error_msg != NULL) {
                *error_msg = "Failure-specific options are only valid for 'show "
                             "results' command";
            }
            return false;
        }
    }

    // Validation for "suite" subcommand (show suite <name>)
    if (strcmp(cmd_name, "suite") == 0) {
        // Requires exactly one positional argument (the suite name)
        if (cmd->argc != 1) {
            if (error_msg != NULL) {
                char *msg = FL_MALLOC(128);
                snprintf(
                    msg, 128,
                    "Command 'show suite' requires exactly one argument (suite name)");
                *error_msg = msg;
            }
            return false;
        }

        // --limit doesn't make sense for single suite
        if (has_option(cmd, "limit")) {
            if (error_msg != NULL) {
                *error_msg = "Option '--limit' is not valid for 'show suite' (single "
                             "item command)";
            }
            return false;
        }

        // --failures doesn't make sense for suite details
        if (has_option(cmd, "failures")) {
            if (error_msg != NULL) {
                *error_msg = "Option '--failures' is not valid for 'show suite'";
            }
            return false;
        }

        // Failure-specific options don't make sense
        if (has_type || has_phase || has_with_source) {
            if (error_msg != NULL) {
                *error_msg = "Failure-specific options are only valid for 'show "
                             "results' command";
            }
            return false;
        }

        // --suite option doesn't make sense when showing a specific suite
        if (has_option(cmd, "suite")) {
            if (error_msg != NULL) {
                *error_msg = "Option '--suite' is not valid for 'show suite' (suite "
                             "name is positional argument)";
            }
            return false;
        }
    }

    // Validation for "suites" subcommand (show suites)
    if (strcmp(cmd_name, "suites") == 0) {
        // Failure-specific options don't make sense for listing suites
        if (has_failures || has_type || has_phase || has_with_source) {
            if (error_msg != NULL) {
                *error_msg = "Failure options are not valid for 'show suites' command";
            }
            return false;
        }
    }

    // Validation for "cases" subcommand (show cases)
    if (strcmp(cmd_name, "cases") == 0) {
        // Failure-specific options don't make sense for test case catalog
        if (has_failures || has_type || has_phase || has_with_source) {
            if (error_msg != NULL) {
                *error_msg = "Failure options are not valid for 'show cases' command";
            }
            return false;
        }
    }

    // Validation for "run" command
    if (strcmp(cmd_name, "run") == 0) {
        // Run needs at least one test suite path
        if (cmd->argc < 1) {
            if (error_msg != NULL) {
                *error_msg = "Command 'run' requires at least one test suite path";
            }
            return false;
        }

        // None of the show-related options make sense for run
        if (has_option(cmd, "limit") || has_failures || has_type || has_phase
            || has_with_source || has_option(cmd, "suite") || has_option(cmd, "format")
            || has_option(cmd, "sort")) {
            if (error_msg != NULL) {
                *error_msg = "Query/display options are not valid for 'run' command";
            }
            return false;
        }
    }

    // Validation for "show" command with "no-db" option
    if (strcmp(cmd_name, "show") == 0 && has_option(cmd, "no-db")) {
        if (error_msg != NULL) {
            *error_msg = "Database required for query operations. Remove --no-db or "
                         "specify --db <path>";
        }
        return false;
    }

    // Check that options requiring arguments have them
    if (cmd->options != NULL) {
        for (int i = 0; cmd->options[i].option != NULL; i++) {
            if (cmd->options[i].option->requires_arg && cmd->options[i].arg == NULL) {
                if (error_msg != NULL) {
                    char *msg = FL_MALLOC(128);
                    snprintf(msg, 128, "Option '%s' requires an argument",
                             cmd->options[i].option->long_form);
                    *error_msg = msg;
                }
                return false;
            }
        }
    }

    return true;
}
