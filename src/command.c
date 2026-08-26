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
#include <faultline/fl_log.h>    // for LOG_VERBOSE
#include <faultline/fl_macros.h> // for FL_ANALYSIS_SUPPRESS
#include <faultline/fl_try.h>    // for FL_THROW
#if defined(__clang__) || defined(__GNUC__)
#include <sec_api/string_s.h> // for strncpy_s
#endif
#include <stdbool.h> // for bool, false, true
#include <stdlib.h>  // for NULL, free, malloc, atoi, size_t
#include <faultline/fl_memory.h> // IWYU pragma: keep - routes malloc/free through service in DLL build
#include <string.h>                       // for strcmp, strchr
#include <faultline/fl_exception_types.h> // for FLExceptionReason

FLExceptionReason command_unknown       = "unknown command";
FLExceptionReason command_error         = "command error";
FLExceptionReason command_out_of_memory = "command parser out of memory";

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
        FL_THROW_DETAILS(command_unknown, "%s", command);
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
 * @throw command_out_of_memory if an allocation fails
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
                FL_THROW(command_out_of_memory);
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
            FL_THROW(command_out_of_memory);
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
        FL_THROW(command_out_of_memory);
    }
    runtime_opt->option = formal;
    runtime_opt->arg    = option_arg;

    return runtime_opt;
}

void free_command(RuntimeCommand *cmd) {
    if (cmd == NULL) {
        return;
    }

    free_command(cmd->subcommand);
    if (cmd->options != NULL) {
        free(cmd->options);
    }
    if (cmd->args != NULL) {
        free(cmd->args);
    }
    free(cmd);
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
 * Exception safety: when any throw propagates out of this function, the partially built
 * command and all working storage have been released.
 *
 * @param formals the table of built-in commands
 * @param argc the number of arguments in argv from the command line.
 * @param argv a vector of strings from the command line.
 * @return RuntimeCommand* a freshly allocated RuntimeCommand; release it with
 * free_command
 * @throw command_unknown if the command or subcommand is not recognized.
 * @throw command_error if the command, subcommand, or their options can't be parsed.
 * @throw command_out_of_memory if an allocation fails.
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
        FL_THROW(command_out_of_memory);
    }

    runtime_cmd->command    = cmd;
    runtime_cmd->options    = NULL;
    runtime_cmd->subcommand = NULL;
    runtime_cmd->args       = NULL;
    runtime_cmd->argc       = 0;

    // Working storage not yet owned by runtime_cmd. Everything in the FL_TRY block below
    // can throw (each allocation, parse_option, and the subcommand recursion), so these
    // are volatile-qualified: they are written between setjmp and longjmp and read by
    // the catch block, which releases them and the partially built runtime_cmd before
    // rethrowing. Each pointer is cleared as its ownership transfers so the catch block
    // never frees an object twice.
    RuntimeOption *volatile option_array = NULL;
    char **volatile positional_array     = NULL;
    char **volatile sub_argv             = NULL;

    // Permute options and operands (GNU-style) for commands that take no subcommands, so
    // options may appear before or after positional arguments (e.g. `run a.dll --db x`).
    // Commands with subcommands keep POSIX ordering: the first non-option token is the
    // subcommand and ends option scanning.
    bool permute          = (cmd->subcommands == NULL);
    int  positional_count = 0;
    int  option_count     = 0;
    int  option_capacity  = 8;

    FL_TRY {
        // Parse options - collect them in a dynamic array first
        option_array = malloc(sizeof(RuntimeOption) * option_capacity);
        if (option_array == NULL) {
            FL_THROW(command_out_of_memory);
        }

        if (permute) {
            // argc is a safe upper bound on the number of operands.
            positional_array = malloc(sizeof(char *) * argc);
            if (positional_array == NULL) {
                FL_THROW(command_out_of_memory);
            }
        }

        int i = 2; // Start after program name and command name
        while (i < argc) {
            // "--" ends option parsing; everything after it is an operand.
            if (strcmp(argv[i], "--") == 0) {
                i++; // Skip the "--"
                if (permute) {
                    while (i < argc) {
                        positional_array[positional_count++] = argv[i];
                        i++;
                    }
                }
                break;
            }

            // Try to parse as option
            int            consumed;
            RuntimeOption *opt = parse_option(cmd, &argv[i], argc - i, &consumed);

            if (opt == NULL) {
                // For a command with subcommands, the first non-option token begins the
                // subcommand, so stop scanning. Without subcommands, collect it as an
                // operand and keep scanning so later options are recognized.
                if (!permute) {
                    break;
                }
                positional_array[positional_count++] = argv[i];
                i++;
                continue;
            }

            // Add to option array, growing if necessary
            if (option_count >= option_capacity) {
                option_capacity *= 2;
                RuntimeOption *new_array
                    = realloc(option_array, sizeof(RuntimeOption) * option_capacity);
                if (new_array == NULL) {
                    free(opt); // loop-local; the catch block cannot reach it
                    FL_THROW(command_out_of_memory);
                }
                option_array = new_array;
            }

            option_array[option_count++] = *opt;
            free(opt);
            i += consumed;
        }

        // NULL-terminate the options array
        RuntimeOption *final_options
            = realloc(option_array, sizeof(RuntimeOption) * (option_count + 1));
        if (final_options == NULL) {
            FL_THROW(command_out_of_memory);
        }
        option_array         = NULL;
        runtime_cmd->options = final_options; // owned by runtime_cmd from here

        runtime_cmd->options[option_count].option = NULL; // NULL terminator
        runtime_cmd->options[option_count].arg    = NULL;

        // Check if there's a subcommand
        bool parsed_subcommand = false;
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
                sub_argv = malloc(sizeof(char *) * (argc - i + 1));
                if (sub_argv == NULL) {
                    FL_THROW(command_out_of_memory);
                }

                LOG_VERBOSE("COMMAND",
                            "subcommand: program name=%s, i=%d, subcommand name=%s",
                            argv[0], i, argv[i]);
                sub_argv[0] = argv[0]; // program name
                sub_argv[1] = argv[i]; // subcommand name
                for (int j = i + 1; j < argc; j++) {
                    sub_argv[j - i + 1] = argv[j];
                }

                runtime_cmd->subcommand
                    = parse_command(cmd->subcommands, argc - i + 1, sub_argv);
                free(sub_argv); // sub_argv elements point into original argv; safe
                                // to free now
                sub_argv          = NULL;
                parsed_subcommand = true;
            }
        }

        if (!parsed_subcommand) {
            // Remaining arguments are positional
            if (permute) {
                // Operands gathered during the permuting scan above.
                if (positional_count > 0) {
                    runtime_cmd->args = positional_array;
                    runtime_cmd->argc = positional_count;
                    positional_array  = NULL; // owned by runtime_cmd from here
                } else {
                    free(positional_array);
                    positional_array = NULL;
                }
            } else {
                int remaining = argc - i;
                if (remaining > 0) {
                    runtime_cmd->args = malloc(sizeof(char *) * remaining);
                    if (runtime_cmd->args == NULL) {
                        FL_THROW(command_out_of_memory);
                    }

                    runtime_cmd->argc = remaining;
                    for (int j = 0; j < remaining; j++) {
                        runtime_cmd->args[j] = argv[i + j];
                    }
                }
            }
        }
    }
    FL_CATCH_ALL_RETHROW {
        if (sub_argv != NULL) {
            free(sub_argv);
        }
        if (positional_array != NULL) {
            free(positional_array);
        }
        if (option_array != NULL) {
            free(option_array);
        }
        free_command(runtime_cmd);
    }
    FL_END_TRY;

    return runtime_cmd;
}

/**
 * @brief Parse a command line that may carry global options before the command.
 *
 * Scans any leading options that match `globals` (e.g. `--db x` in `faultline
 * --db x show hotspots`) to locate the command, then reorders the line to
 * "program <command> <leading options...> <rest...>" and hands it to
 * parse_command. The leading options are thus parsed as the command's own
 * options, so the command must accept them (the real commands do, via their
 * shared option set). A leading token that is not a recognized global option
 * ends the scan and is taken as the command name; passing NULL for `globals`
 * makes this behave exactly like parse_command.
 *
 * @param formals the table of built-in commands
 * @param globals NULL-terminated array of options accepted before the command
 * @param argc argument count
 * @param argv argument vector
 * @return RuntimeCommand* the parsed command
 * @throw command_error if a leading option is unknown, missing its argument, or
 *        not followed by a command
 * @throw command_unknown if the command name is not recognized
 * @throw command_out_of_memory if an allocation fails
 */
RuntimeCommand *parse_command_with_globals(FormalCommand const *formals,
                                           FormalOption const *globals, int argc,
                                           char **argv) {
    if (argc < 2) {
        FL_THROW(command_error);
    }

    // Scan leading options against `globals` to find where the command begins.
    // parse_option throws command_error on an unknown option or a missing
    // required argument, which is the desired behavior for a bad leading option.
    FormalCommand const global_cmd = {"", "", NULL, globals, NULL, NULL};
    int                 cmd_index  = 1;
    while (cmd_index < argc && strcmp(argv[cmd_index], "--") != 0) {
        int            consumed;
        RuntimeOption *opt
            = parse_option(&global_cmd, &argv[cmd_index], argc - cmd_index, &consumed);
        if (opt == NULL) {
            break; // not a global option: the command name starts here
        }
        free(opt); // only the boundary matters; the option is re-parsed below
        cmd_index += consumed;
    }

    // No leading options: behave exactly like parse_command (and avoid a copy).
    if (cmd_index == 1) {
        return parse_command(formals, argc, argv);
    }
    // Leading options but no command after them.
    if (cmd_index >= argc) {
        FL_THROW(command_error);
    }

    // Reorder to "program <command> <leading options...> <rest...>" so the
    // leading options are parsed as the command's own options.
    char **reordered = malloc(sizeof(char *) * argc);
    if (reordered == NULL) {
        FL_THROW(command_out_of_memory);
    }
    int n          = 0;
    reordered[n++] = argv[0];         // program name
    reordered[n++] = argv[cmd_index]; // command
    // 6386: the three writes below store 2 + (cmd_index - 1) + (argc - cmd_index
    // - 1) == argc entries. cmd_index is in [2, argc - 1] here: 1 returns above
    // and argc or more throws above.
    for (int j = 1; j < cmd_index; j++) {
        FL_ANALYSIS_SUPPRESS(6386)
        reordered[n++] = argv[j]; // leading options and their arguments
    }
    for (int j = cmd_index + 1; j < argc; j++) {
        reordered[n++] = argv[j]; // everything after the command
    }

    // reordered is only scaffolding for the nested parse; release it whether the
    // parse returns or throws (FL_END_TRY rethrows a pending exception after the
    // FL_FINALLY block runs).
    RuntimeCommand *parsed = NULL;
    FL_TRY {
        parsed = parse_command(formals, n, reordered);
    }
    FL_FINALLY {
        free(reordered);
    }
    FL_END_TRY;

    return parsed;
}

/**
 * @brief check if a RuntimeCommand has a specific option
 *
 * @param cmd the RuntimeCommand to check
 * @param option_name the long-form name of the option (without --)
 * @return true if the option is present, false otherwise
 */
bool has_option(RuntimeCommand const *cmd, char const *option_name) {
    if (cmd == NULL) {
        return false;
    }

    if (cmd->options != NULL) {
        for (int i = 0; cmd->options[i].option != NULL; i++) {
            if (cmd->options[i].option->long_form != NULL
                && strcmp(cmd->options[i].option->long_form, option_name) == 0) {
                return true;
            }
        }
    }

    // Global options (e.g. --db) may be given before or after a subcommand, so
    // search the subcommand chain. The current level is checked first, so an
    // option set at this level wins over the same option set on a subcommand.
    return has_option(cmd->subcommand, option_name);
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
    if (cmd == NULL) {
        return default_value;
    }

    if (cmd->options != NULL) {
        for (int i = 0; cmd->options[i].option != NULL; i++) {
            if (cmd->options[i].option->long_form != NULL
                && strcmp(cmd->options[i].option->long_form, option_name) == 0) {
                return cmd->options[i].arg;
            }
        }
    }

    // See has_option: global options are resolved across the subcommand chain so
    // their placement on the command line does not matter.
    return get_string_option(cmd->subcommand, option_name, default_value);
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
