#ifndef WB_CLI_H_
#define WB_CLI_H_
/**
 * @file wb_cli.h
 * @author Douglas Cuthbertson
 * @brief A small command engine modeled on Go's spf13/cobra.
 * @version 0.1
 * @date 2026-08-22
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * A CLI is a tree of commands, each struct WBCommand holds its name, help text, flags,
 * and the function that runs it. The wb_command_add() function links two commands in a
 * parent/child relationship.
 *
 * Two properties are worth knowing before writing a command:
 *
 * - No memory is allocated to define a command. They are file-scope structs, so the
 *   whole tree is static storage and the parent, child, and sibling links are intrusive.
 *   The only heap in the CLI layer is the REPL's input line (WBLineBuffer), which grows
 *   through FL_REALLOC and is reused for the life of the session.
 *
 * - Parsed values are read-only. A WB_FLAG_STRING value and every argv entry point into
 *   the line buffer that produced them are valid only until the run function returns.
 *
 * - Commands receive a `void *user`, so the engine can be exercised without a database.
 */
#include <faultline/fl_abbreviated_types.h> // i32, i64
#include <faultline/fl_exception_types.h>   // FLExceptionReason

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdio.h>   // FILE

#if defined(__cplusplus)
extern "C" {
#endif

// Thrown when the CLI cannot allocate. Defined in wb_cli.c.
extern FLExceptionReason wb_cli_out_of_memory;

/**
 * @brief Return values from a command that can be used as a process's exit code.
 */
enum {
    WB_CLI_OK    = 0, // the command did what it was asked
    WB_CLI_ERROR = 1, // the command ran and failed
    WB_CLI_USAGE = 2  // the command line was malformed; usage has been printed
};

/**
 * @brief Files for standard and error output. This enables a test to write to a
 * temporary file, read back that output, and verify it.
 */
typedef struct WBCliIO {
    FILE *out; // ordinary command output and help text
    FILE *err; // diagnostics: unknown command, bad flag, a caught exception
} WBCliIO;

/**
 * @brief define Boolean, text, and numeric flags.
 */
typedef enum WBFlagType {
    WB_FLAG_BOOL,   // --- verbose, --verbose=false
    WB_FLAG_STRING, // --- description "text", -d text, --description=text
    WB_FLAG_I64     // --- project 3, -p 3, --project=3
} WBFlagType;

/**
 * @brief A flag's payload. One named union type, so that resetting a flag before a parse
 * is a single assignment (e.g., value = def).
 */
typedef union WBFlagValue {
    bool        b;
    char const *s;
    i64         i;
} WBFlagValue;

/**
 * @brief WBFlag represents a single command option.
 *
 * `value` is scratch owned by the parser: wb_command_execute() copies `def` over it and
 * clears `changed` before every parse, so a static flag table is safe to reuse across
 * the many commands one REPL session runs.
 */
typedef struct WBFlag {
    char const *name;      // long name without dashes: "description"
    char        shorthand; // 'd', or 0 for no short form
    WBFlagType  type;      // bool, string, integer
    char const *desc;      // a one-line description of the flag"
    bool        changed;   // true when this parse saw the flag
    WBFlagValue value;
    WBFlagValue def;
} WBFlag;

typedef struct WBCommand WBCommand;

/**
 * @brief A function to run a command conforms to this type. It return one of the status
 * values (WB_CLI_OK, WB_CLI_ERROR, or WB_CLI_USAGE).
 *
 * @param cmd the command tree
 * @param io command output and error output
 * @param argc the number of positional arguments
 * @param argv the positional arguments only; flags have been consumed.
 * @param user passed through wb_command_execute() untouched.
 */
#define WB_RUN_FN(name) \
    int name(WBCommand *cmd, WBCliIO const *io, int argc, char **argv, void *user)
typedef WB_RUN_FN(wb_run_fn);

/**
 * @brief A function that validates optional arguments beyond min_args/max_args conforms
 * to this type. It return false after writing an error message to io->err; the CLI
 * engine then returns WB_CLI_USAGE.
 */
#define WB_ARGS_FN(name) \
    bool name(WBCommand const *cmd, WBCliIO const *io, int argc, char **argv)
typedef WB_ARGS_FN(wb_args_fn);

// max_args sentinel: any number of positional arguments is acceptable.
#define WB_ARGS_ANY (-1)

/**
 * @brief One node of the command tree.
 *
 * A zero-initialized command takes no arguments and min_args and max_args both default
 * to 0.
 * @param usage is the one-line usage message.
 *    Recommended syntax is as follows:
 *      [ ] identifies an optional argument. Arguments that are not enclosed in brackets
 *          are required.
 *      ... indicates that you can specify multiple values for the previous argument.
 *      |   indicates mutually exclusive information. You can use the argument to the
 *          left of the separator or the argument to the right of the separator. You
 *          cannot use both arguments in a single use of the command.
 *      { } delimits a set of mutually exclusive arguments when one of the arguments is
 *          required. If the arguments are optional, they are enclosed in brackets ([ ]).
 *    Example: add [-F file | -D dir]... [-f format] profile
 */
struct WBCommand {
    // "create <name>": everything up to the first space is the command's name; the
    // rest is shown in the usage line.
    char const *usage;      // a one-line usage message.
    char const *short_desc; // a short description shown in the 'help' output.
    char const *long_desc;  // paragraph shown at the top of this command's help
    char const *example;    // examples of how to use the command; can be NULL.

    WBFlag *flags;
    size_t  flag_count;

    // Inherited by every descendant, so "module list -p 2" and "module enable -p 2 x"
    // both work from one declaration on the "module" group.
    WBFlag *persistent_flags;
    size_t  persistent_flag_count;

    i32         min_args;
    i32         max_args; // WB_ARGS_ANY for unbounded
    wb_args_fn *args;     // optional, runs after the min/max check passes

    // NULL makes this a grouping command: running it prints its help.
    wb_run_fn *run;

    // Intrusive tree. wb_command_add() maintains all three; never set them by hand.
    WBCommand *parent;  // not NULL if this is a subcommand
    WBCommand *child;   // first child
    WBCommand *sibling; // next child of `parent`

    bool hidden; // omitted from "Available Commands:" but still runnable
};

// ---  Building the tree ---------------------------------------------------------------

/**
 * @brief insert @p child into the @p parent's command tree.
 *
 * The form of the tree is first-child/next-sibling (aka left-child/right-sibling). It
 * lets a tree with any number of children be represented using only two pointers.
 */
void wb_command_add(WBCommand *parent, WBCommand *child);

/**
 * @brief The command's name: `use` up to its first space.
 *
 * Returned as a pointer into `use` plus a length, so nothing is copied. Print it with
 * "%.*s" and (int)length.
 */
char const *wb_command_name(WBCommand const *cmd, size_t *out_length);

/**
 * @brief True when @p token equals this command's name.
 */
bool wb_command_name_is(WBCommand const *cmd, char const *token);

// ---  Running -------------------------------------------------------------------------

/**
 * @brief walk @p argv from @p root while each word names a child.
 *
 * Stops at the first word that is not a child name or that begins with '-'. Never
 * returns NULL: a command line naming nothing deeper resolves to @p root itself.
 *
 * @param out_consumed set to the number of argv entries the path used.
 */
WBCommand *wb_command_find(WBCommand *root, int argc, char **argv, int *out_consumed);

/**
 * @brief Resolve, parse flags, validate arguments, and run.
 *
 * Every flag reachable from the resolved command, its own and the persistent flags of
 * each ancestor, is reset to its default before parsing, which is what makes a static
 * flag table safe in a REPL that runs the same command repeatedly.
 *
 * `--help` or `-h` anywhere on the line prints help instead of running.
 *
 * @return the run function's status, or WB_CLI_USAGE if the line did not parse. Does
 *         not catch exceptions: a throwing command unwinds to the caller's FL_TRY.
 */
int wb_command_execute(WBCommand *root, WBCliIO const *io, int argc, char **argv,
                       void *user);

// ---  Reading parsed flags ------------------------------------------------------------
// Each searches @p cmd's own flags, then walks the parent chain for persistent flags.
// Asking for a flag the command never declared is a programming error and throws.

bool        wb_flag_bool(WBCommand const *cmd, char const *name);
char const *wb_flag_string(WBCommand const *cmd, char const *name);
i64         wb_flag_i64(WBCommand const *cmd, char const *name);

/**
 * @brief True when this parse actually saw the flag, rather than falling back to its
 * default. Lets "unset" and "set to the default value" differ.
 */
bool wb_flag_changed(WBCommand const *cmd, char const *name);

// ---  Help ----------------------------------------------------------------------------

/**
 * @brief Display help for @p cmd: description, usage, examples, available
 * commands, flags, inherited flags.
 *
 * If the long description is NULL, then the short description is displayed if it is set.
 *
 * @param cmd: the command whose help message is displayed
 * @param io: the output channel for feedback and error messages
 */
void wb_command_help(WBCommand const *cmd, WBCliIO const *io);

/**
 * @brief The one-line "Usage:" form, written to io->err after a usage error.
 */
void wb_command_usage(WBCommand const *cmd, WBCliIO const *io);

/**
 * @brief Write the full path to this command, "worldbuilder module enable".
 */
void wb_command_write_path(WBCommand const *cmd, FILE *out);

// ---  Reading a line ------------------------------------------------------------------

/**
 * @brief A growable input line. One of these serves a whole session: it keeps whatever
 * capacity the longest line so far needed, so the steady state costs no allocation.
 */
typedef struct WBLineBuffer {
    char  *data;
    size_t capacity;
    size_t length; // characters in `data`, excluding the terminator
} WBLineBuffer;

/**
 * @brief Read one line from @p in into @p lb, without its trailing newline.
 *
 * @return false at end of input with nothing read; true otherwise, including for a
 *         final line that has no newline of its own.
 * @throw wb_cli_out_of_memory if the buffer cannot grow.
 */
bool wb_line_read(WBLineBuffer *lb, FILE *in);

/**
 * @brief Release the buffer. Safe on a zero-initialized WBLineBuffer.
 */
void wb_line_free(WBLineBuffer *lb);

// ---  Splitting a line into arguments -------------------------------------------------

// Enough for any hand-typed command; overflow is reported, never silently truncated.
#define WB_CLI_MAX_ARGS 64

enum {
    WB_TOKENIZE_TOO_MANY_ARGS      = -1,
    WB_TOKENIZE_UNTERMINATED_QUOTE = -2
};

/**
 * @brief Split @p line into arguments in place.
 *
 * Words separate on whitespace. A double-quoted run is one argument, and a backslash
 * inside one escapes the next character. Quotes are removed by shifting the remainder
 * left, which is safe because the result is never longer than the input.
 *
 * @param argv     filled with pointers into @p line; each is NUL-terminated in place.
 * @param max_args capacity of @p argv.
 * @return the argument count, or a negative WB_TOKENIZE_* code.
 */
int wb_tokenize(char *line, char **argv, int max_args);

// ---  Parsing a number ----------------------------------------------------------------

/**
 * @brief Parse a whole decimal integer, the way a WB_FLAG_I64 is parsed.
 *
 * All of @p text has to be the number: a trailing character is a failure rather than a
 * value. Return false when @p text represents a number outside the range i64 can hold
 * rather than clamping to LLONG_MAX or LLONG_MIN.
 *
 * @return false if @p text is not a whole number, leaving @p out untouched.
 */
bool wb_parse_i64(char const *text, i64 *out);

#if defined(__cplusplus)
}
#endif

#endif // WB_CLI_H_
