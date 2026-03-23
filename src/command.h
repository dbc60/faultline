#ifndef COMMAND_H_
#define COMMAND_H_
/**
 * @file command.h
 * @author Douglas Cuthbertson
 * @brief Provide structure for command-line commands, subcommands, and options.
 * Commands, subcommands and options are single words, but options are expected to be
 * prefixed with two hyphens and may have arguments. Options may also have a short form
 * consisting of one hyphen followed by a single letter. If the option has arguments,
 * then the short form is followed by an equals sign and the arguments.
 *
 * @version 0.1
 * @date 2026-01-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_exception_types.h>

#include <stdbool.h>

extern FLExceptionReason command_unknown;
extern FLExceptionReason command_error;

typedef struct RuntimeCommand RuntimeCommand;
typedef struct FormalCommand  FormalCommand;
typedef struct FormalOption   FormalOption;

/*
 * Git command handlers match this function signature:
 *   int (*fn)(int, const char **, const char *, struct repository *);
 *
 * For example:
 *   int cmd_add(int argc,
 *  	         const char **argv,
 *  	         const char *prefix,
 *  	         struct repository *repo)
 *
 * This command calls a generic "parse_options()" funcion:
 * 	argc = parse_options(argc, argv, prefix, builtin_add_options,
 *                       builtin_add_usage, PARSE_OPT_KEEP_ARGV0);
 *
 * builtin_add_options is large:
 *
 * static struct option builtin_add_options[] = {
 *     OPT__DRY_RUN(&show_only, N_("dry run")),
 *     OPT__VERBOSE(&verbose, N_("be verbose")),
 *     OPT_GROUP(""),
 *     OPT_BOOL('i', "interactive", &add_interactive, N_("interactive picking")),
 *     OPT_BOOL('p', "patch", &patch_interactive, N_("select hunks interactively")),
 *     OPT_DIFF_UNIFIED(&add_p_opt.context),
 *     OPT_DIFF_INTERHUNK_CONTEXT(&add_p_opt.interhunkcontext),
 *     OPT_BOOL('e', "edit", &edit_interactive, N_("edit current diff and apply")),
 *     OPT__FORCE(&ignored_too, N_("allow adding otherwise ignored files"), 0),
 *     OPT_BOOL('u', "update", &take_worktree_changes, N_("update tracked files")),
 *     OPT_BOOL(0, "renormalize", &add_renormalize, N_("renormalize EOL of tracked files
 * (implies -u)")), OPT_BOOL('N', "intent-to-add", &intent_to_add, N_("record only the
 * fact that the path will be added later")), OPT_BOOL('A', "all", &addremove_explicit,
 * N_("add changes from all tracked and untracked files")), OPT_CALLBACK_F(0,
 * "ignore-removal", &addremove_explicit, NULL (takes no arguments) , N_("ignore paths
 * removed in the working tree (same as --no-all)"), PARSE_OPT_NOARG, ignore_removal_cb),
 *     OPT_BOOL( 0 , "refresh", &refresh_only, N_("don't add, only refresh the index")),
 *     OPT_BOOL( 0 , "ignore-errors", &ignore_add_errors, N_("just skip files which
 * cannot be added because of errors")), OPT_BOOL( 0 , "ignore-missing", &ignore_missing,
 * N_("check if - even missing - files are ignored in dry run")), OPT_BOOL(0, "sparse",
 * &include_sparse, N_("allow updating entries outside of the sparse-checkout cone")),
 *     OPT_STRING(0, "chmod", &chmod_arg, "(+|-)x",
 *            N_("override the executable bit of the listed files")),
 *     OPT_HIDDEN_BOOL(0, "warn-embedded-repo", &warn_on_embedded_repo,
 *             N_("warn when adding an embedded repository")),
 *     OPT_PATHSPEC_FROM_FILE(&pathspec_from_file),
 *     OPT_PATHSPEC_FILE_NUL(&pathspec_file_nul),
 *     OPT_END(),
 * };
 *
 */
// Execute the runtime command that was built from the command-line arguments
#define COMMAND_HANDLER(name) void name(const RuntimeCommand *cmd)
typedef COMMAND_HANDLER(CommandHandler);

// Formal command/subcommand definition
struct FormalCommand {
    char const          *name;
    char const          *help;
    CommandHandler      *handler;
    FormalOption const  *options;     // NULL-terminated array
    FormalCommand const *subcommands; // NULL-terminated array
    char const          *arguments;   // positional argument description, or NULL
};

typedef struct RuntimeOption RuntimeOption;
#define OPTION_HANDLER(name) void name(RuntimeOption *opt)
typedef OPTION_HANDLER(OptionHandler);

// Formal command-line option definition
struct FormalOption {
    char const    *short_form; // "-f" (NULL if none)
    char const    *long_form;  // "--force"
    char const    *help;
    bool           requires_arg;
    OptionHandler *handler;
};

// Actual option from the command-line
struct RuntimeOption {
    FormalOption const *option; // link back to the formal definition
    char const         *arg;    // a string for the arg, NULL if none
};

// Actual command/subcommand from the command-line
struct RuntimeCommand {
    FormalCommand const  *command; // link back to formal definition
    struct RuntimeOption *options; // NULL-terminated array
    struct RuntimeCommand
          *subcommand; // a subcommand read from the command line, NULL if none
    char **args;       // positional arguments
    int    argc;       // argument count
};

// Command parsing
RuntimeCommand *parse_command(FormalCommand const *formals, int argc, char **argv);

// Command validation
bool validate_command(RuntimeCommand const *cmd, char **error_msg);

// Option helper functions
bool        has_option(RuntimeCommand const *cmd, char const *option_name);
char const *get_string_option(RuntimeCommand const *cmd, char const *option_name,
                              char const *default_value);
int         get_int_option(RuntimeCommand const *cmd, char const *option_name,
                           int default_value);

#endif // COMMAND_H_
