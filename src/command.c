/**
 * @file command.c
 * @author Douglas Cuthbertson
 * @brief Generic command-line parsing infrastructure
 * @version 0.1
 * @date 2026-01-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "command.h"
#include <faultline/fl_log.h> // for LOG_VERBOSE
#include <faultline/fl_try.h> // for FL_THROW
#if defined(__clang__) || defined(__GNUC__)
#include <sec_api/string_s.h> // for strncpy_s
#endif
#include <stdbool.h> // for bool, false, true
#include <stdlib.h>  // for NULL, free, malloc, atoi, size_t
#include <faultline/fl_memory.h> // IWYU pragma: keep - routes malloc/free through service in DLL build
#include <string.h>                       // for strcmp, strchr
#include <faultline/fl_exception_types.h> // for FLExceptionReason

FLExceptionReason command_unknown = "unknown command";
FLExceptionReason command_error   = "command error";

/**
 * @brief search the array of formals for the named command
 *
 * @param formals an array for FormalCommands
 * @param command the name of the command to search for
 * @return FormalCommand const * the address of a command from the formals array
 * @throw command_unknown if command is not found
 */
static FormalCommand const *find_command(FormalCommand const *formals,
                                         char const          *command) {
    int                  i   = 0;
    FormalCommand const *cmd = NULL;
    // Search for matching command name
    while (formals[i].name != NULL && cmd == NULL) {
        if (strcmp(formals[i].name, command) == 0) {
            cmd = &formals[i];
        }
        i++;
    }

    if (cmd == NULL) {
        // Command not found
        FL_THROW_DETAILS(command_unknown, command);
    }

    return cmd;
}

/**
 * @brief search the array of formal options for the named option
 *
 * @param options NULL-terminated array of FormalOptions
 * @param option_name the name of the option to search for (without leading dashes)
 * @return FormalOption const * the matching option, or NULL if not found
 */
static FormalOption const *find_option(FormalOption const *options,
                                       char const         *option_name) {
    FormalOption const *opt = NULL;
    if (options != NULL) {
        for (int i = 0; (options[i].long_form != NULL || options[i].short_form != NULL)
                        && opt == NULL;
             i++) {
            // Check long form (e.g., "limit")
            if (options[i].long_form != NULL
                && strcmp(options[i].long_form, option_name) == 0) {
                opt = &options[i];
            }
            // Check short form (e.g., "l")
            if (options[i].short_form != NULL
                && strcmp(options[i].short_form, option_name) == 0) {
                opt = &options[i];
            }
        }
    }

    return opt;
}

/**
 * @brief check if a string starts with '-'
 *
 * @param str the string to check
 * @return true if str starts with '-', false otherwise
 */
static bool is_option(char const *str) {
    return str != NULL && str[0] == '-';
}

/**
 * @brief Parse a single option from argv
 *
 * Handles both long form (--option, --option=value) and short form (-o, -o=value)
 *
 * @param cmd the command whose options are being parsed
 * @param argv current position in argument vector
 * @param argc_remaining number of arguments remaining
 * @param consumed_out set to number of argv elements consumed (1 or 2)
 * @return RuntimeOption* newly allocated RuntimeOption, or NULL if not an option
 * @throw command_error if option is invalid or missing required argument
 */
static RuntimeOption *parse_option(FormalCommand const *cmd, char **argv,
                                   int argc_remaining, int *consumed_out) {
    char const *arg            = argv[0];
    char const *option_name    = NULL;
    char       *allocated_name = NULL; // heap-allocated copy of option name, if any
    char const *option_arg     = NULL;

    *consumed_out = 1;

    // Not an option
    if (!is_option(arg)) {
        return NULL;
    }

    // Handle "--" (stop option parsing)
    if (strcmp(arg, "--") == 0) {
        return NULL;
    }

    // Parse long form: --option or --option=value
    if (arg[0] == '-' && arg[1] == '-') {
        option_name = arg + 2; // skip "--"

        // Check for "="
        char *equals = strchr(option_name, '=');
        if (equals != NULL) {
            // Option has embedded argument: --option=value
            size_t name_len  = equals - option_name;
            char  *name_copy = malloc(name_len + 1);
            if (name_copy == NULL) {
                return NULL;
            }
            strncpy_s(name_copy, name_len + 1, option_name, name_len);
            name_copy[name_len] = '\0';
            allocated_name      = name_copy;
            option_name         = name_copy;
            option_arg          = equals + 1;
        }
    } else if (arg[0] == '-' && arg[1] != '\0') {
        // Parse short form: -o or -o=value
        char *short_name = malloc(2);
        if (short_name == NULL) {
            return NULL;
        }
        short_name[0]  = arg[1];
        short_name[1]  = '\0';
        allocated_name = short_name;
        option_name    = short_name;

        // Check for "="
        if (arg[2] == '=') {
            option_arg = arg + 3;
        }
    } else {
        FL_THROW(command_error);
    }

    // Find the formal option definition
    FormalOption const *formal = find_option(cmd->options, option_name);
    if (allocated_name != NULL) {
        free(allocated_name); // free heap copy if one was made
    }
    if (formal == NULL) {
        FL_THROW(command_error);
    }

    // If option requires argument and we don't have one embedded, take next argv
    if (formal->requires_arg && option_arg == NULL) {
        if (argc_remaining < 2) {
            FL_THROW(command_error);
        }
        option_arg    = argv[1];
        *consumed_out = 2;
    }

    // Allocate and populate RuntimeOption
    RuntimeOption *runtime_opt = malloc(sizeof(RuntimeOption));
    if (runtime_opt == NULL) {
        return NULL;
    }
    runtime_opt->option = formal;
    runtime_opt->arg    = option_arg;

    return runtime_opt;
}

/**
 * @brief build a RuntimeCommand from the table of FormalCommands and the command-line
 * arguments.
 *
 * 1. Find matching command in formals array
 * 2. Allocate RuntimeCommand, link to FormalCommand
 * 3. Parse options (loop while argv[i] starts with '-')
 * 4. Check for subcommand, recurse if found
 * 5. Store remaining argc/argv as positional args
 * 6. Return constructed RuntimeCommand
 *
 * @param formals the table of built-in commands
 * @param argc the number of arguments in argv from the command line.
 * @param argv a vector of strings from the command line.
 * @return RuntimeCommand* a freshly allocated RuntimeCommand
 * @throw command_unknown if the command or subcommand is not recognized.
 * @throw command_error if the command, subcommand, or their options can't be parsed.
 */
RuntimeCommand *parse_command(FormalCommand const *formals, int argc, char **argv) {
    if (argc < 2) {
        FL_THROW(command_error);
    }

    // Find the command
    FormalCommand const *cmd = find_command(formals, argv[1]);

    // Allocate RuntimeCommand
    RuntimeCommand *runtime_cmd = malloc(sizeof(RuntimeCommand));
    if (runtime_cmd == NULL) {
        return NULL;
    }

    runtime_cmd->command    = cmd;
    runtime_cmd->subcommand = NULL;
    runtime_cmd->args       = NULL;
    runtime_cmd->argc       = 0;

    // Parse options - collect them in a dynamic array first
    int            option_count    = 0;
    int            option_capacity = 8;
    RuntimeOption *option_array    = malloc(sizeof(RuntimeOption) * option_capacity);
    if (option_array == NULL) {
        free(runtime_cmd);
        return NULL;
    }

    int i = 2; // Start after program name and command name
    while (i < argc) {
        // Stop if we hit "--"
        if (strcmp(argv[i], "--") == 0) {
            i++; // Skip the "--"
            break;
        }

        // Try to parse as option
        int            consumed;
        RuntimeOption *opt = parse_option(cmd, &argv[i], argc - i, &consumed);

        if (opt == NULL) {
            // Not an option, stop parsing options
            break;
        }

        // Add to option array, growing if necessary
        if (option_count >= option_capacity) {
            option_capacity *= 2;
            RuntimeOption *new_array = malloc(sizeof(RuntimeOption) * option_capacity);
            if (new_array == NULL) {
                free(opt);
                free(option_array);
                free(runtime_cmd);
                return NULL;
            }

            for (int j = 0; j < option_count; j++) {
                new_array[j] = option_array[j];
            }

            free(option_array);
            option_array = new_array;
        }

        option_array[option_count++] = *opt;
        free(opt);
        i += consumed;
    }

    // NULL-terminate the options array
    RuntimeOption *final_options = malloc(sizeof(RuntimeOption) * (option_count + 1));
    if (final_options == NULL) {
        free(option_array);
        free(runtime_cmd);
        return NULL;
    }

    for (int j = 0; j < option_count; j++) {
        final_options[j] = option_array[j];
    }

    final_options[option_count].option = NULL; // NULL terminator
    final_options[option_count].arg    = NULL;
    runtime_cmd->options               = final_options;
    free(option_array); // option_array has been copied into final_options; no longer
                        // needed

    // Check if there's a subcommand
    if (i < argc && cmd->subcommands != NULL) {
        // Try to find a subcommand
        FormalCommand const *subcmd = NULL;
        for (int j = 0; cmd->subcommands[j].name != NULL; j++) {
            if (strcmp(cmd->subcommands[j].name, argv[i]) == 0) {
                subcmd = &cmd->subcommands[j];
                break;
            }
        }

        if (subcmd != NULL) {
            // Parse subcommand recursively
            // Create new argv starting with "program subcommand ..."
            char **sub_argv = malloc(sizeof(char *) * (argc - i + 1));
            if (sub_argv == NULL) {
                free(runtime_cmd->options);
                free(runtime_cmd);
                return NULL;
            }

            LOG_VERBOSE("COMMAND",
                        "subcommand: program name=%s, i=%d, subcommand name=%s", argv[0],
                        i, argv[i]);
            sub_argv[0] = argv[0]; // program name
            sub_argv[1] = argv[i]; // subcommand name
            for (int j = i + 1; j < argc; j++) {
                sub_argv[j - i + 1] = argv[j];
            }

            runtime_cmd->subcommand
                = parse_command(cmd->subcommands, argc - i + 1, sub_argv);
            free(sub_argv); // sub_argv elements point into original argv; safe to free
                            // now
            if (runtime_cmd->subcommand == NULL) {
                free(runtime_cmd->options);
                free(runtime_cmd);
                return NULL;
            }
            return runtime_cmd;
        }
    }

    // Remaining arguments are positional
    int positional_count = argc - i;
    if (positional_count > 0) {
        runtime_cmd->args = malloc(sizeof(char *) * positional_count);
        if (runtime_cmd->args == NULL) {
            free(runtime_cmd->options);
            free(runtime_cmd);
            return NULL;
        }

        runtime_cmd->argc = positional_count;
        for (int j = 0; j < positional_count; j++) {
            runtime_cmd->args[j] = argv[i + j];
        }
    }

    return runtime_cmd;
}

/**
 * @brief check if a RuntimeCommand has a specific option
 *
 * @param cmd the RuntimeCommand to check
 * @param option_name the long-form name of the option (without --)
 * @return true if the option is present, false otherwise
 */
bool has_option(RuntimeCommand const *cmd, char const *option_name) {
    if (cmd == NULL || cmd->options == NULL) {
        return false;
    }

    for (int i = 0; cmd->options[i].option != NULL; i++) {
        if (cmd->options[i].option->long_form != NULL
            && strcmp(cmd->options[i].option->long_form, option_name) == 0) {
            return true;
        }
    }

    return false;
}

/**
 * @brief get the string argument of an option
 *
 * @param cmd the RuntimeCommand to search
 * @param option_name the long-form name of the option (without --)
 * @param default_value value to return if option not found
 * @return char const * the option's argument, or default_value if not present
 */
char const *get_string_option(RuntimeCommand const *cmd, char const *option_name,
                              char const *default_value) {
    if (cmd == NULL || cmd->options == NULL) {
        return default_value;
    }

    for (int i = 0; cmd->options[i].option != NULL; i++) {
        if (cmd->options[i].option->long_form != NULL
            && strcmp(cmd->options[i].option->long_form, option_name) == 0) {
            return cmd->options[i].arg;
        }
    }

    return default_value;
}

/**
 * @brief get the integer argument of an option
 *
 * @param cmd the RuntimeCommand to search
 * @param option_name the long-form name of the option (without --)
 * @param default_value value to return if option not found
 * @return int the option's argument parsed as integer, or default_value if not present
 */
int get_int_option(RuntimeCommand const *cmd, char const *option_name,
                   int default_value) {
    char const *str = get_string_option(cmd, option_name, NULL);
    if (str == NULL) {
        return default_value;
    }

    return atoi(str);
}
