/**
 * @file wb_cli.c
 * @author Douglas Cuthbertson
 * @brief The command engine declared in wb_cli.h: tree, flags, help, and the line
 * handling the REPL feeds it.
 * @version 0.1
 * @date 2026-08-22
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * The CLI implementation used for all commands.
 *
 * The CRT headers must precede fl_memory.h. Below a non-platform build,
 * fla_memory_service.h rewrites malloc/calloc/realloc/free into macros over the
 * injected service, and a CRT header that declares them afterwards expands the macro
 * into an invalid declaration.
 */
#include <errno.h>  // errno, ERANGE
#include <stdio.h>  // FILE, fgets, fprintf, fputs, fputc, snprintf
#include <stdlib.h> // strtoll
#include <string.h> // strchr, strcmp, strlen, strncmp, memcpy

#include <wb_cli.h>

#include <faultline/fl_exception_assert.h> // FL_ASSERT_DETAILS
#include <faultline/fl_macros.h>           // FL_UNUSED
#include <faultline/fl_memory.h>           // FL_REALLOC, FL_FREE
#include <faultline/fl_try.h>              // FL_THROW_DETAILS

FLExceptionReason wb_cli_out_of_memory = "the command line ran out of memory";

enum {
    // Big enough that an ordinary typed line never reallocates twice.
    WB_LINE_INITIAL_CAPACITY = 256,
    // Slack kept available for each fgets pass.
    WB_LINE_READ_CHUNK = 128,
    // Longest --flag name the parser will copy out of a --name=value argument.
    WB_FLAG_NAME_MAX = 64,
    // Numbers a person types at a command line are decimal.
    WB_NUMBER_BASE = 10,
    // Widest left column a help table renders.
    WB_HELP_LEFT_MAX = 64,
    // Most flags one help block will list.
    WB_HELP_MAX_ROWS = 32
};

// ---------------------------------------------------------------------------
// The input line
// ---------------------------------------------------------------------------

// Ensure lb is at least `needed` bytes including the terminator. If it's too small, then
// double its capacity.

/**
 * @brief reserve buffer space. If the capacity of @p lb is too small, then double it
 * until it is at least `needed` bytes.
 *
 * @param lb the buffer into which data will be read
 * @param needed the number of bytes the buffer must be able to store
 * @throw wb_cli_out_of_memory if space can't be allocated
 */
static void wb_line_reserve(WBLineBuffer *lb, size_t needed) {
    if (lb->capacity >= needed) {
        return;
    }

    size_t capacity
        = (lb->capacity == 0) ? (size_t)WB_LINE_INITIAL_CAPACITY : lb->capacity;
    while (capacity < needed) {
        capacity *= 2;
    }

    char *data = (char *)FL_REALLOC(lb->data, capacity);
    if (data == NULL) {
        FL_THROW_DETAILS(wb_cli_out_of_memory, "cannot grow the input line to %zu bytes",
                         capacity);
    }

    lb->data     = data;
    lb->capacity = capacity;
}

// The three possible states that result from calling fgets.
typedef enum WBLineChunk {
    WB_LINE_CHUNK_EOF,     // nothing was read; the stream is finished
    WB_LINE_CHUNK_PARTIAL, // text was appended, but this line has not ended yet
    WB_LINE_CHUNK_COMPLETE // text was appended and it ended with a newline
} WBLineChunk;

// Append one fgets pass to lb, first ensuring it's large enough
static WBLineChunk wb_line_append(WBLineBuffer *lb, FILE *in) {
    wb_line_reserve(lb, lb->length + (size_t)WB_LINE_READ_CHUNK);

    char *const tail = lb->data + lb->length;
    // fgets returns NULL for EOF or for a read error. In either case, we're done reading
    // input, so return EOF.
    // NOLINTNEXTLINE(clang-analyzer-unix.Errno)
    if (fgets(tail, (int)(lb->capacity - lb->length), in) == NULL) {
        return WB_LINE_CHUNK_EOF; // end of input, or a read error
    }

    lb->length += strlen(tail);
    return (lb->length > 0 && lb->data[lb->length - 1] == '\n') ? WB_LINE_CHUNK_COMPLETE
                                                                : WB_LINE_CHUNK_PARTIAL;
}

bool wb_line_read(WBLineBuffer *lb, FILE *in) {
    bool more  = false;
    lb->length = 0;
    wb_line_reserve(lb, (size_t)WB_LINE_INITIAL_CAPACITY);
    lb->data[0] = '\0';

    // Each pass is capped by the room fgets was given, so PARTIAL means the line is
    // longer than that and the next pass continues it.
    WBLineChunk chunk;
    do {
        chunk = wb_line_append(lb, in);
    } while (chunk == WB_LINE_CHUNK_PARTIAL);

    if (chunk != WB_LINE_CHUNK_EOF || lb->length != 0) {
        // A final line with no newline of its own is still a line.
        while (lb->length > 0
               && (lb->data[lb->length - 1] == '\n'
                   || lb->data[lb->length - 1] == '\r')) {
            lb->length--;
            lb->data[lb->length] = '\0';
        }
        more = true;
    }

    return more;
}

void wb_line_free(WBLineBuffer *lb) {
    if (lb->data != NULL) {
        FL_FREE(lb->data);
    }
    lb->data     = NULL;
    lb->capacity = 0;
    lb->length   = 0;
}

// ---------------------------------------------------------------------------
// Splitting a line
// ---------------------------------------------------------------------------

static bool wb_is_space(char c) {
    return c == ' ' || c == '\t';
}

static char *wb_skip_space(char *p) {
    while (wb_is_space(*p)) {
        p++;
    }
    return p;
}

int wb_tokenize(char *line, char **argv, int max_args) {
    int argc = 0;

    // Skip whitespace in both the initializer and the increment
    for (char *read = wb_skip_space(line); *read != '\0'; read = wb_skip_space(read)) {
        if (argc >= max_args) {
            return WB_TOKENIZE_TOO_MANY_ARGS;
        }

        // `write` trails `read` whenever a quote or escape has been dropped. Compacting
        // in place is safe precisely because the result is never longer than the input.
        char *write   = read;
        argv[argc++]  = write;
        bool in_quote = false;

        // A token ends at the end of the line, or at whitespace that a quote is not
        // protecting -- which is the whole condition, so the body only has to copy.
        while (*read != '\0' && (in_quote || !wb_is_space(*read))) {
            char const c = *read;
            if (in_quote && c == '\\' && (read[1] == '"' || read[1] == '\\')) {
                read++;             // skip the backslash
                *write++ = *read++; // take the character it protected
            } else if (c == '"') {
                // One toggle covers both: outside a quote it opens, inside one it
                // closes.
                in_quote = !in_quote;
                read++;
            } else {
                *write++ = *read++;
            }
        }

        if (in_quote) {
            return WB_TOKENIZE_UNTERMINATED_QUOTE;
        }

        // Terminate before stepping over the separator: when nothing was compacted,
        // `write` and `read` are the same address and the NUL replaces the separator.
        bool const at_end = (*read == '\0');
        *write            = '\0';
        if (!at_end) {
            read++;
        }
    }

    return argc;
}

// ---------------------------------------------------------------------------
// Parsing a number
// ---------------------------------------------------------------------------

bool wb_parse_i64(char const *text, i64 *out) {
    char *end = NULL;

    // strtoll will return LLONG_MAX for that number typed out and for errors, so clear
    // errno first and read it after to determine if there's an error.
    errno            = 0;
    long long parsed = strtoll(text, &end, WB_NUMBER_BASE);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return false;
    }

    *out = (i64)parsed;
    return true;
}

// ---------------------------------------------------------------------------
// The tree
// ---------------------------------------------------------------------------

void wb_command_add(WBCommand *parent, WBCommand *child) {
    child->parent  = parent;
    child->sibling = NULL;

    // insert the first child
    if (parent->child == NULL) {
        parent->child = child;
        return;
    }

    // walk the first child's sibling chain and make the new child the next sibling.
    WBCommand *last = parent->child;
    while (last->sibling != NULL) {
        last = last->sibling;
    }
    last->sibling = child;
}

char const *wb_command_name(WBCommand const *cmd, size_t *out_length) {
    char const *usage = (cmd->usage != NULL) ? cmd->usage : "";
    char const *space = strchr(usage, ' ');
    if (out_length != NULL) {
        *out_length = (space != NULL) ? (size_t)(space - usage) : strlen(usage);
    }
    return usage;
}

bool wb_command_name_is(WBCommand const *cmd, char const *token) {
    size_t      length = 0;
    char const *name   = wb_command_name(cmd, &length);
    return strncmp(name, token, length) == 0 && token[length] == '\0';
}

WBCommand *wb_command_find(WBCommand *root, int argc, char **argv, int *out_consumed) {
    WBCommand *cmd = root;
    int        i   = 0;

    while (i < argc) {
        if (argv[i][0] == '-') {
            break; // a flag ends the command path
        }

        WBCommand *match = NULL;
        for (WBCommand *c = cmd->child; c != NULL; c = c->sibling) {
            if (wb_command_name_is(c, argv[i])) {
                match = c;
                break;
            }
        }

        if (match == NULL) {
            break;
        }

        cmd = match;
        i++;
    }

    if (out_consumed != NULL) {
        *out_consumed = i;
    }

    return cmd;
}

// ---------------------------------------------------------------------------
// Flags
// ---------------------------------------------------------------------------

// Match on either the long name or the shorthand; a zero shorthand never matches, so a
// lookup by name cannot collide with a flag that has no short form.
static WBFlag *wb_flag_in_table(WBFlag *flags, size_t count, char const *name,
                                char shorthand) {
    for (size_t i = 0; i < count; i++) {
        WBFlag *flag = &flags[i];
        if (name != NULL && flag->name != NULL && strcmp(flag->name, name) == 0) {
            return flag;
        }
        if (shorthand != 0 && flag->shorthand == shorthand) {
            return flag;
        }
    }
    return NULL;
}

// Look up flags; first the command's, then the persistent flags of the command and each
// ancestor command/subcommand in turn.
static WBFlag *wb_flag_lookup(WBCommand const *cmd, char const *name, char shorthand) {
    if (cmd == NULL) {
        return NULL;
    }

    WBFlag *flag = wb_flag_in_table(cmd->flags, cmd->flag_count, name, shorthand);
    if (flag != NULL) {
        return flag;
    }
    for (WBCommand const *c = cmd; c != NULL; c = c->parent) {
        flag = wb_flag_in_table(c->persistent_flags, c->persistent_flag_count, name,
                                shorthand);
        if (flag != NULL) {
            return flag;
        }
    }
    return NULL;
}

static void wb_flags_reset_table(WBFlag *flags, size_t count) {
    for (size_t i = 0; i < count; i++) {
        flags[i].value   = flags[i].def;
        flags[i].changed = false;
    }
}

// Clear all flags before each parse.
static void wb_flags_reset(WBCommand *cmd) {
    wb_flags_reset_table(cmd->flags, cmd->flag_count);
    for (WBCommand *c = cmd; c != NULL; c = c->parent) {
        wb_flags_reset_table(c->persistent_flags, c->persistent_flag_count);
    }
}

// Apply `text` to `flag`. A bool with no explicit value is being set, as --verbose is.
static bool wb_flag_assign(WBFlag *flag, char const *text, WBCliIO const *io) {
    switch (flag->type) {
    case WB_FLAG_BOOL:
        if (text == NULL || strcmp(text, "true") == 0 || strcmp(text, "1") == 0) {
            flag->value.b = true;
        } else if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0) {
            flag->value.b = false;
        } else {
            fprintf(io->err, "invalid boolean value \"%s\" for --%s\n", text, flag->name);
            return false;
        }
        break;

    case WB_FLAG_STRING:
        flag->value.s = text;
        break;

    case WB_FLAG_I64:
        if (!wb_parse_i64(text, &flag->value.i)) {
            fprintf(io->err, "invalid integer value \"%s\" for --%s\n", text, flag->name);
            return false;
        }
        break;

    default:
        fprintf(io->err, "flag --%s has an unknown type\n", flag->name);
        return false;
    }

    flag->changed = true;
    return true;
}

static WBFlag const *wb_flag_require(WBCommand const *cmd, char const *name,
                                     WBFlagType type) {
    WBFlag const *flag = wb_flag_lookup(cmd, name, 0);
    FL_ASSERT_DETAILS(flag != NULL, "no flag named \"--%s\" is declared here", name);
    FL_ASSERT_DETAILS(flag->type == type, "flag \"--%s\" was declared with another type",
                      name);
    return flag;
}

bool wb_flag_bool(WBCommand const *cmd, char const *name) {
    return wb_flag_require(cmd, name, WB_FLAG_BOOL)->value.b;
}

char const *wb_flag_string(WBCommand const *cmd, char const *name) {
    return wb_flag_require(cmd, name, WB_FLAG_STRING)->value.s;
}

i64 wb_flag_i64(WBCommand const *cmd, char const *name) {
    return wb_flag_require(cmd, name, WB_FLAG_I64)->value.i;
}

bool wb_flag_changed(WBCommand const *cmd, char const *name) {
    WBFlag const *flag = wb_flag_lookup(cmd, name, 0);
    FL_ASSERT_DETAILS(flag != NULL, "no flag named \"--%s\" is declared here", name);
    return flag->changed;
}

// ---------------------------------------------------------------------------
// Help
// ---------------------------------------------------------------------------

/**
 * @brief Display the command name. If it is a subcommand, the display its ancestors
 * recursively from the oldest one first.
 *
 * @param cmd
 * @param out
 */
void wb_command_write_path(WBCommand const *cmd, FILE *out) {
    if (cmd == NULL) {
        return;
    }

    // display ancestor command names
    if (cmd->parent != NULL) {
        wb_command_write_path(cmd->parent, out);
        fputc(' ', out);
    }

    // display this command's name
    size_t      length = 0;
    char const *name   = wb_command_name(cmd, &length);
    fprintf(out, "%.*s", (int)length, name);
}

/**
 * @brief display the command's usage line.
 *
 * For example: "worldbuilder module enable <code> [flags]"
 *
 * @param cmd the command
 * @param out where to write its usage text.
 */
static void wb_write_use_line(WBCommand const *cmd, FILE *out) {
    size_t      length = 0;
    char const *name   = wb_command_name(cmd, &length);

    wb_command_write_path(cmd, out);
    char const *rest = name + length;
    if (*rest != '\0') {
        fputs(rest, out); // includes its own leading space
    }
    fputs(" [flags]", out);
}

void wb_command_usage(WBCommand const *cmd, WBCliIO const *io) {
    fputs("Usage:\n  ", io->err);
    wb_write_use_line(cmd, io->err);
    fputc('\n', io->err);
}

typedef struct WBHelpRow {
    char        left[WB_HELP_LEFT_MAX];
    char const *usage;
} WBHelpRow;

// "-d, --description string" / "    --verbose".
static void wb_help_row_flag(WBFlag const *flag, WBHelpRow *row) {
    char const *type_name = "";
    if (flag->type == WB_FLAG_STRING) {
        type_name = " string";
    } else if (flag->type == WB_FLAG_I64) {
        type_name = " int";
    }

    if (flag->shorthand != 0) {
        snprintf(row->left, sizeof row->left, "-%c, --%s%s", flag->shorthand, flag->name,
                 type_name);
    } else {
        snprintf(row->left, sizeof row->left, "    --%s%s", flag->name, type_name);
    }
    row->usage = (flag->desc != NULL) ? flag->desc : "";
}

static size_t wb_help_add_flags(WBFlag const *flags, size_t count, WBHelpRow *rows,
                                size_t used, size_t capacity) {
    for (size_t i = 0; i < count && used < capacity; i++) {
        wb_help_row_flag(&flags[i], &rows[used]);
        used++;
    }
    return used;
}

static void wb_help_print_rows(WBHelpRow const *rows, size_t count, FILE *out) {
    size_t width = 0;
    for (size_t i = 0; i < count; i++) {
        size_t const length = strlen(rows[i].left);
        if (length > width) {
            width = length;
        }
    }
    for (size_t i = 0; i < count; i++) {
        fprintf(out, "  %-*s   %s\n", (int)width, rows[i].left, rows[i].usage);
    }
}

void wb_command_help(WBCommand const *cmd, WBCliIO const *io) {
    FILE       *out  = io->out;
    char const *desc = (cmd->long_desc != NULL) ? cmd->long_desc : cmd->short_desc;

    // description
    if (desc != NULL) {
        fprintf(out, "%s\n\n", desc);
    }

    fputs("Usage:\n  ", out);
    wb_write_use_line(cmd, out);
    fputc('\n', out);
    if (cmd->child != NULL) {
        fputs("  ", out);
        wb_command_write_path(cmd, out);
        fputs(" [command]\n", out);
    }

    if (cmd->example != NULL) {
        fprintf(out, "\nExamples:\n%s\n", cmd->example);
    }

    if (cmd->child != NULL) {
        size_t width = 0;
        for (WBCommand const *c = cmd->child; c != NULL; c = c->sibling) {
            size_t length = 0;
            (void)wb_command_name(c, &length);
            if (!c->hidden && length > width) {
                width = length;
            }
        }
        if (width > 0) {
            fputs("\nAvailable Commands:\n", out);
            for (WBCommand const *c = cmd->child; c != NULL; c = c->sibling) {
                if (c->hidden) {
                    continue;
                }
                size_t      length = 0;
                char const *name   = wb_command_name(c, &length);
                fprintf(out, "  %-*.*s   %s\n", (int)width, (int)length, name,
                        (c->short_desc != NULL) ? c->short_desc : "");
            }
        }
    }

    // "Flags:" is this command's own set, plus the persistent flags it declares itself,
    // plus the --help flag.
    {
        WBHelpRow rows[WB_HELP_MAX_ROWS];
        size_t    used = 0;
        used           = wb_help_add_flags(cmd->flags, cmd->flag_count, rows, used,
                                           WB_HELP_MAX_ROWS);
        used = wb_help_add_flags(cmd->persistent_flags, cmd->persistent_flag_count, rows,
                                 used, WB_HELP_MAX_ROWS);
        if (used < WB_HELP_MAX_ROWS) {
            snprintf(rows[used].left, sizeof rows[used].left, "-h, --help");
            rows[used].usage = "help for this command";
            used++;
        }
        fputs("\nFlags:\n", out);
        wb_help_print_rows(rows, used, out);
    }

    // "Global Flags:" are the flags contributed by ancestor commands/subcommands.
    {
        WBHelpRow rows[WB_HELP_MAX_ROWS];
        size_t    used = 0;
        for (WBCommand const *c = cmd->parent; c != NULL; c = c->parent) {
            used = wb_help_add_flags(c->persistent_flags, c->persistent_flag_count, rows,
                                     used, WB_HELP_MAX_ROWS);
        }
        if (used > 0) {
            fputs("\nGlobal Flags:\n", out);
            wb_help_print_rows(rows, used, out);
        }
    }

    if (cmd->child != NULL) {
        fputs("\nUse \"", out);
        wb_command_write_path(cmd, out);
        fputs(" [command] --help\" for more information about a command.\n", out);
    }
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

int wb_command_execute(WBCommand *root, WBCliIO const *io, int argc, char **argv,
                       void *user) {
    int        consumed = 0;
    WBCommand *cmd      = wb_command_find(root, argc, argv, &consumed);

    wb_flags_reset(cmd);

    char *positional[WB_CLI_MAX_ARGS];
    int   positional_count = 0;
    bool  want_help        = false;
    bool  literal          = false; // everything after "--" is positional

    for (int i = consumed; i < argc; i++) {
        char *arg = argv[i];

        // "-" on its own is the conventional name for standard input, not a flag.
        if (literal || arg[0] != '-' || arg[1] == '\0') {
            if (positional_count >= WB_CLI_MAX_ARGS) {
                fputs("too many arguments\n", io->err);
                wb_command_usage(cmd, io);
                return WB_CLI_USAGE;
            }
            positional[positional_count++] = arg;
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            literal = true;
            continue;
        }

        char        name_buf[WB_FLAG_NAME_MAX] = {0};
        char const *name                       = NULL;
        char        shorthand                  = 0;
        char const *inline_value               = NULL;

        if (arg[1] == '-') {
            char const *body = arg + 2;
            char const *eq   = strchr(body, '=');
            if (eq != NULL) {
                size_t const length = (size_t)(eq - body);
                if (length == 0 || length >= sizeof name_buf) {
                    fprintf(io->err, "malformed flag \"%s\"\n", arg);
                    wb_command_usage(cmd, io);
                    return WB_CLI_USAGE;
                }
                memcpy(name_buf, body, length);
                name_buf[length] = '\0';
                name             = name_buf;
                inline_value     = eq + 1;
            } else {
                name = body;
            }
        } else {
            shorthand = arg[1];
            if (arg[2] == '=') {
                inline_value = arg + 3;
            } else if (arg[2] != '\0') {
                // Clustered shorthands (-abc) are not supported; say so rather than
                // silently reading it as a long name.
                fprintf(io->err, "combined short flags are not supported: \"%s\"\n", arg);
                wb_command_usage(cmd, io);
                return WB_CLI_USAGE;
            }
        }

        // --help is applied to every command
        if ((name != NULL && strcmp(name, "help") == 0) || shorthand == 'h') {
            want_help = true;
            continue;
        }

        WBFlag *flag = wb_flag_lookup(cmd, name, shorthand);
        if (flag == NULL) {
            fprintf(io->err, "unknown flag \"%s\" for \"", arg);
            wb_command_write_path(cmd, io->err);
            fputs("\"\n", io->err);
            wb_command_usage(cmd, io);
            return WB_CLI_USAGE;
        }

        char const *text = inline_value;
        if (flag->type != WB_FLAG_BOOL && text == NULL) {
            if (i + 1 >= argc) {
                fprintf(io->err, "flag \"--%s\" needs a value\n", flag->name);
                wb_command_usage(cmd, io);
                return WB_CLI_USAGE;
            }
            text = argv[++i];
        }
        if (!wb_flag_assign(flag, text, io)) {
            wb_command_usage(cmd, io);
            return WB_CLI_USAGE;
        }
    }

    if (want_help) {
        wb_command_help(cmd, io);
        return WB_CLI_OK;
    }

    if (cmd->run == NULL) {
        // A grouping command with a leftover word was a typo, not a request for help.
        if (positional_count > 0) {
            fprintf(io->err, "unknown command \"%s\" for \"", positional[0]);
            wb_command_write_path(cmd, io->err);
            fputs("\"\n", io->err);
            wb_command_usage(cmd, io);
            return WB_CLI_USAGE;
        }
        wb_command_help(cmd, io);
        return WB_CLI_OK;
    }

    if (positional_count < cmd->min_args) {
        fprintf(io->err, "\"");
        wb_command_write_path(cmd, io->err);
        fprintf(io->err, "\" needs at least %d argument%s, got %d\n", cmd->min_args,
                (cmd->min_args == 1) ? "" : "s", positional_count);
        wb_command_usage(cmd, io);
        return WB_CLI_USAGE;
    }
    if (cmd->max_args != WB_ARGS_ANY && positional_count > cmd->max_args) {
        fprintf(io->err, "\"");
        wb_command_write_path(cmd, io->err);
        fprintf(io->err, "\" accepts at most %d argument%s, got %d\n", cmd->max_args,
                (cmd->max_args == 1) ? "" : "s", positional_count);
        wb_command_usage(cmd, io);
        return WB_CLI_USAGE;
    }
    if (cmd->args != NULL && !cmd->args(cmd, io, positional_count, positional)) {
        wb_command_usage(cmd, io);
        return WB_CLI_USAGE;
    }

    return cmd->run(cmd, io, positional_count, positional, user);
}
