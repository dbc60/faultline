/**
 * @file wb_cli_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the CLI command engine.
 * @version 0.1
 * @date 2026-08-22
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Unlike the sqlite and DAL suites, this one includes exactly one file under test and
 * needs no database and no sqlite3.obj: wb_cli.c knows nothing about projects
 * or modules, which is the point of keeping the engine and the tree apart. Everything
 * the domain adds lives in worldbuilder_cli_commands.c and is exercised by driving the
 * built executable.
 *
 * The fixture gives each case its own scratch streams, so a case can run a command and
 * then read back exactly what was printed. Commands here are throwaway structs declared
 * per case, which is also the clearest demonstration that a command is only ever a
 * struct literal plus a function.
 */
#include <stdio.h>  // fopen_s, fseek, fread, remove, snprintf
#include <stdlib.h> // NULL
#include <string.h> // strcmp, strstr, memset

#include <faultline/fl_exception_assert.h> // FL_ASSERT_*, FL_FAIL
#include <faultline/fl_test.h>             // FL_TYPE_TEST_SETUP_CLEANUP, FL_SUITE_*
#include <faultline/fl_try.h>              // FL_TRY, FL_CATCH, FL_END_TRY

#include "wb_cli.c" // code under test

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

enum {
    WB_TEST_CAPTURE_MAX = 4096,
    WB_TEST_PATH_MAX    = 128
};

static int wb_cli_test_counter = 0;

// A scratch stream in the working directory. tmpfile() is the obvious choice and the
// wrong one here: MSVC deprecates it under /WX, and its implementation opens the file in
// the root of the current drive, which an unelevated test run cannot do. The DAL suite
// already keeps its scratch files beside the runner and removes them in cleanup, so this
// follows it.
static FILE *open_scratch(char *path, size_t size) {
    FILE *file = NULL;
    snprintf(path, size, "wb_cli_test_%d.tmp", ++wb_cli_test_counter);
    remove(path);
    if (fopen_s(&file, path, "w+b") != 0) {
        file = NULL;
    }
    FL_ASSERT_DETAILS(file != NULL, "cannot open the scratch file '%s'", path);
    return file;
}

static void close_scratch(FILE **file, char *path) {
    if (*file != NULL) {
        fclose(*file);
        *file = NULL;
    }
    if (path[0] != 0) {
        remove(path);
        path[0] = 0;
    }
}

typedef struct WbCliTest {
    FLTestCase tc;
    WBCliIO    io;
    char       capture[WB_TEST_CAPTURE_MAX];

    // Backing files for io.out, io.err, and the stream the line-reading cases read from.
    char  out_path[WB_TEST_PATH_MAX];
    char  err_path[WB_TEST_PATH_MAX];
    char  in_path[WB_TEST_PATH_MAX];
    FILE *in;

    // The line-reading cases use this rather than a local, so that cleanup releases it
    // even when the case throws. Under fault injection wb_line_read() is re-run with
    // each of its allocations forced to fail in turn, and a failure part way through
    // leaves a buffer that is still allocated and still the caller's to free -- which is
    // what the REPL itself does, in wb_cli_session_free() after FL_END_TRY.
    WBLineBuffer line_buf;

    // Scratch the cases share, so a case body reads as the thing it is testing.
    char  line[512];
    char *argv[WB_CLI_MAX_ARGS];

    // Set by the recording run function below, so a case can prove what was passed to
    // it.
    int   seen_argc;
    char *seen_argv[8];
    bool  ran;
    void *seen_user;
} WbCliTest;

static FL_SETUP_FN(setup_cli) {
    WbCliTest *t = FL_CONTAINER_OF(tc, WbCliTest, tc);
    memset(t->capture, 0, sizeof t->capture);
    memset(t->line, 0, sizeof t->line);
    t->in_path[0] = 0;
    t->in         = NULL;
    t->line_buf   = (WBLineBuffer){0};
    t->io.out     = open_scratch(t->out_path, sizeof t->out_path);
    t->io.err     = open_scratch(t->err_path, sizeof t->err_path);
    t->ran        = false;
    t->seen_argc  = 0;
    t->seen_user  = NULL;
}

static FL_CLEANUP_FN(cleanup_cli) {
    WbCliTest *t = FL_CONTAINER_OF(tc, WbCliTest, tc);
    close_scratch(&t->io.out, t->out_path);
    close_scratch(&t->io.err, t->err_path);
    close_scratch(&t->in, t->in_path);
    wb_line_free(&t->line_buf);
}

// The input stream for a case that reads lines; cleanup closes and removes it.
static FILE *scratch_in(WbCliTest *t) {
    t->in = open_scratch(t->in_path, sizeof t->in_path);
    return t->in;
}

// Seek a scratch stream back to the start. Use fseek rather than rewind, because fseek
// returns an error code satisfying static code analysis, it can be immediately detected.
static void rewind_scratch(FILE *stream) {
    FL_ASSERT_EQ(fseek(stream, 0, SEEK_SET), 0);
}

// Read everything written to one of the fixture's streams back into t->capture.
static char const *captured(WbCliTest *t, FILE *stream) {
    fflush(stream);
    rewind_scratch(stream);
    // A short read is end of stream here: the stream is a scratch file this fixture
    // wrote and flushed a moment ago, so there's no need to check errno.
    // NOLINTNEXTLINE(clang-analyzer-unix.Errno)
    size_t const read = fread(t->capture, 1, sizeof t->capture - 1, stream);
    t->capture[read]  = '\0';
    return t->capture;
}

// Copy a literal into the fixture's writable line: wb_tokenize edits in place, so a
// string literal would be undefined behavior.
static char *line_of(WbCliTest *t, char const *text) {
    size_t const length = strlen(text);
    FL_ASSERT_TRUE(length < sizeof t->line);
    memcpy(t->line, text, length + 1);
    return t->line;
}

// Tokenize `text` into the fixture's argv and return the count.
static int split(WbCliTest *t, char const *text) {
    return wb_tokenize(line_of(t, text), t->argv, WB_CLI_MAX_ARGS);
}

// A run function that records rather than doing anything.
static WB_RUN_FN(record_run) {
    WbCliTest *t = (WbCliTest *)user;
    FL_UNUSED(cmd);
    FL_UNUSED(io);

    t->ran       = true;
    t->seen_user = user;
    t->seen_argc = argc;
    for (int i = 0; i < argc && i < 8; i++) {
        t->seen_argv[i] = argv[i];
    }
    return WB_CLI_OK;
}

// ---------------------------------------------------------------------------
// Tokenizing
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("Tokenize splits on whitespace", WbCliTest, tokenize_splits,
                           setup_cli, cleanup_cli) {
    int const argc = split(t, "module   enable\tmagic");
    FL_ASSERT_EQ(argc, 3);
    FL_ASSERT_STR_EQ(t->argv[0], "module");
    FL_ASSERT_STR_EQ(t->argv[1], "enable");
    FL_ASSERT_STR_EQ(t->argv[2], "magic");
}

FL_TYPE_TEST_SETUP_CLEANUP("Tokenize keeps a quoted run together", WbCliTest,
                           tokenize_quotes, setup_cli, cleanup_cli) {
    int const argc = split(t, "project create \"A drowned city\"");
    FL_ASSERT_EQ(argc, 3);
    FL_ASSERT_STR_EQ(t->argv[2], "A drowned city");
}

FL_TYPE_TEST_SETUP_CLEANUP("Tokenize honours escapes inside quotes", WbCliTest,
                           tokenize_escapes, setup_cli, cleanup_cli) {
    int const argc = split(t, "create \"say \\\"hello\\\" now\"");
    FL_ASSERT_EQ(argc, 2);
    FL_ASSERT_STR_EQ(t->argv[1], "say \"hello\" now");
}

FL_TYPE_TEST_SETUP_CLEANUP("Tokenize joins a quote to its neighbours", WbCliTest,
                           tokenize_adjacent_quote, setup_cli, cleanup_cli) {
    // Shell habit: --name="two words" is one argument, quotes and all.
    int const argc = split(t, "--name=\"two words\"");
    FL_ASSERT_EQ(argc, 1);
    FL_ASSERT_STR_EQ(t->argv[0], "--name=two words");
}

FL_TYPE_TEST_SETUP_CLEANUP("Tokenize accepts an empty quoted argument", WbCliTest,
                           tokenize_empty_quotes, setup_cli, cleanup_cli) {
    int const argc = split(t, "create \"\"");
    FL_ASSERT_EQ(argc, 2);
    FL_ASSERT_STR_EQ(t->argv[1], "");
}

FL_TYPE_TEST_SETUP_CLEANUP("Tokenize returns nothing for blank input", WbCliTest,
                           tokenize_blank, setup_cli, cleanup_cli) {
    FL_ASSERT_EQ(split(t, ""), 0);
    FL_ASSERT_EQ(split(t, "   \t  "), 0);
}

FL_TYPE_TEST_SETUP_CLEANUP("Tokenize rejects an unterminated quote", WbCliTest,
                           tokenize_unterminated, setup_cli, cleanup_cli) {
    FL_ASSERT_EQ(split(t, "create \"never closed"), WB_TOKENIZE_UNTERMINATED_QUOTE);
}

FL_TYPE_TEST_SETUP_CLEANUP("Tokenize reports overflow rather than truncating", WbCliTest,
                           tokenize_overflow, setup_cli, cleanup_cli) {
    // Four words for three slots: return an error, don't clip the count to three.
    char     *argv[3];
    int const argc = wb_tokenize(line_of(t, "one two three four"), argv, 3);
    FL_ASSERT_EQ(argc, WB_TOKENIZE_TOO_MANY_ARGS);
}

// ---------------------------------------------------------------------------
// Parsing a number
// ---------------------------------------------------------------------------

// Three call sites parse a typed integer -- an i64 flag, "project use", and
// "status move" -- and they agree only because they share this. The whole token has
// to be the number, and rejected text leaves the caller's variable alone, so a command
// that ignored the result would act on its own default rather than on garbage.
FL_TEST("A number is the whole token or it is not a number", parse_i64_whole_token) {
    i64 value = 0;

    FL_ASSERT_TRUE(wb_parse_i64("42", &value));
    FL_ASSERT_EQ((int)value, 42);
    FL_ASSERT_TRUE(wb_parse_i64("-7", &value));
    FL_ASSERT_EQ((int)value, -7);

    // Each of these leaves the last good value in place.
    FL_ASSERT_FALSE(wb_parse_i64("", &value));
    FL_ASSERT_FALSE(wb_parse_i64("7 ", &value));
    FL_ASSERT_FALSE(wb_parse_i64("12x", &value));
    FL_ASSERT_FALSE(wb_parse_i64("seven", &value));
    FL_ASSERT_EQ((int)value, -7);

    // Too big for an i64: strtoll would hand back LLONG_MAX and say nothing.
    FL_ASSERT_FALSE(wb_parse_i64("99999999999999999999", &value));
    FL_ASSERT_FALSE(wb_parse_i64("-99999999999999999999", &value));
    FL_ASSERT_EQ((int)value, -7);
}

// ---------------------------------------------------------------------------
// The line buffer
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("A line longer than the initial buffer still arrives whole",
                           WbCliTest, line_grows, setup_cli, cleanup_cli) {
    // 1000 characters forces at least two doublings past WB_LINE_INITIAL_CAPACITY.
    FILE *in = scratch_in(t);
    for (int i = 0; i < 1000; i++) {
        fputc('x', in);
    }
    fputc('\n', in);
    rewind_scratch(in);

    WBLineBuffer *lb = &t->line_buf;
    FL_ASSERT_TRUE(wb_line_read(lb, in));
    FL_ASSERT_EQ(lb->length, (size_t)1000);
    FL_ASSERT_TRUE(lb->capacity > (size_t)1000);
    FL_ASSERT_EQ(lb->data[999], 'x');
    FL_ASSERT_EQ(lb->data[1000], 0);

    wb_line_free(lb);
    FL_ASSERT_TRUE(lb->data == NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Reading strips the newline and stops at end of input",
                           WbCliTest, line_reads_and_ends, setup_cli, cleanup_cli) {
    FILE *in = scratch_in(t);
    fputs("first\r\nsecond", in); // no trailing newline on the last line
    rewind_scratch(in);

    WBLineBuffer *lb = &t->line_buf;
    FL_ASSERT_TRUE(wb_line_read(lb, in));
    FL_ASSERT_STR_EQ(lb->data, "first");
    FL_ASSERT_TRUE(wb_line_read(lb, in));
    FL_ASSERT_STR_EQ(lb->data, "second");
    FL_ASSERT_FALSE(wb_line_read(lb, in)); // nothing left

    wb_line_free(lb);
}

// ---------------------------------------------------------------------------
// Finding a command
// ---------------------------------------------------------------------------

// A three-level tree the path tests share. Wired once, because wb_command_add appends.
typedef struct WbTestTree {
    WBCommand root;
    WBCommand group;
    WBCommand leaf;
} WbTestTree;

static void build_tree(WbTestTree *tree) {
    memset(tree, 0, sizeof *tree);
    tree->root.usage       = "wb";
    tree->root.short_desc  = "the root";
    tree->group.usage      = "module";
    tree->group.short_desc = "a group";
    tree->leaf.usage       = "enable <code>";
    tree->leaf.short_desc  = "a leaf";
    tree->leaf.min_args    = 1;
    tree->leaf.max_args    = 1;
    tree->leaf.run         = record_run;

    wb_command_add(&tree->root, &tree->group);
    wb_command_add(&tree->group, &tree->leaf);
}

FL_TYPE_TEST_SETUP_CLEANUP("A command's name is its use up to the first space", WbCliTest,
                           name_is_first_word, setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);
    FL_UNUSED(t);

    size_t      length = 0;
    char const *name   = wb_command_name(&tree.leaf, &length);
    FL_ASSERT_EQ(length, (size_t)6);
    FL_ASSERT_EQ(strncmp(name, "enable", 6), 0);
    FL_ASSERT_TRUE(wb_command_name_is(&tree.leaf, "enable"));
    // A prefix is not the name: "ena" must not resolve to "enable".
    FL_ASSERT_FALSE(wb_command_name_is(&tree.leaf, "ena"));
    FL_ASSERT_FALSE(wb_command_name_is(&tree.leaf, "enabled"));
}

FL_TYPE_TEST_SETUP_CLEANUP("Find walks as deep as the words go", WbCliTest,
                           find_walks_path, setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);

    int const argc = split(t, "module enable magic");

    int        consumed = 0;
    WBCommand *found    = wb_command_find(&tree.root, argc, t->argv, &consumed);
    FL_ASSERT_TRUE(found == &tree.leaf);
    FL_ASSERT_EQ(consumed, 2); // "magic" is an argument, not part of the path
}

FL_TYPE_TEST_SETUP_CLEANUP("Find stops at an unknown word", WbCliTest, find_stops_unknown,
                           setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);

    int const  argc     = split(t, "modul enable");
    int        consumed = 0;
    WBCommand *found    = wb_command_find(&tree.root, argc, t->argv, &consumed);
    FL_ASSERT_TRUE(found == &tree.root);
    FL_ASSERT_EQ(consumed, 0);
}

FL_TYPE_TEST_SETUP_CLEANUP("Find stops at the first flag", WbCliTest, find_stops_at_flag,
                           setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);

    int const  argc     = split(t, "module -p 2 enable");
    int        consumed = 0;
    WBCommand *found    = wb_command_find(&tree.root, argc, t->argv, &consumed);
    FL_ASSERT_TRUE(found == &tree.group);
    FL_ASSERT_EQ(consumed, 1);
}

// ---------------------------------------------------------------------------
// Flag parsing
// ---------------------------------------------------------------------------

// A leaf carrying one flag of each type, plus a persistent flag on its parent.
typedef struct WbFlagTree {
    WBCommand root;
    WBCommand leaf;
    WBFlag    leaf_flags[3];
    WBFlag    root_persistent[1];
} WbFlagTree;

static void build_flag_tree(WbFlagTree *tree) {
    memset(tree, 0, sizeof *tree);

    tree->leaf_flags[0].name      = "description";
    tree->leaf_flags[0].shorthand = 'd';
    tree->leaf_flags[0].type      = WB_FLAG_STRING;
    tree->leaf_flags[0].desc      = "a description";
    tree->leaf_flags[0].def.s     = NULL;

    tree->leaf_flags[1].name      = "count";
    tree->leaf_flags[1].shorthand = 'c';
    tree->leaf_flags[1].type      = WB_FLAG_I64;
    tree->leaf_flags[1].desc      = "how many";
    tree->leaf_flags[1].def.i     = 7;

    tree->leaf_flags[2].name      = "force";
    tree->leaf_flags[2].shorthand = 'f';
    tree->leaf_flags[2].type      = WB_FLAG_BOOL;
    tree->leaf_flags[2].desc      = "do it anyway";
    tree->leaf_flags[2].def.b     = false;

    tree->root_persistent[0].name      = "project";
    tree->root_persistent[0].shorthand = 'p';
    tree->root_persistent[0].type      = WB_FLAG_I64;
    tree->root_persistent[0].desc      = "project id";
    tree->root_persistent[0].def.i     = 0;

    tree->root.usage                 = "wb";
    tree->root.short_desc            = "the root";
    tree->root.persistent_flags      = tree->root_persistent;
    tree->root.persistent_flag_count = 1;

    tree->leaf.usage      = "create <name>";
    tree->leaf.short_desc = "make one";
    tree->leaf.flags      = tree->leaf_flags;
    tree->leaf.flag_count = 3;
    tree->leaf.min_args   = 0;
    tree->leaf.max_args   = WB_ARGS_ANY;
    tree->leaf.run        = record_run;

    wb_command_add(&tree->root, &tree->leaf);
}

FL_TYPE_TEST_SETUP_CLEANUP("Every accepted flag form parses", WbCliTest, flag_forms,
                           setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    int const argc   = split(t, "create --description=inline -c 42 -f rest");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_OK);
    FL_ASSERT_TRUE(t->ran);
    FL_ASSERT_STR_EQ(wb_flag_string(&tree.leaf, "description"), "inline");
    FL_ASSERT_EQ(wb_flag_i64(&tree.leaf, "count"), (i64)42);
    FL_ASSERT_TRUE(wb_flag_bool(&tree.leaf, "force"));
    // Only the non-flag word survives as an argument.
    FL_ASSERT_EQ(t->seen_argc, 1);
    FL_ASSERT_STR_EQ(t->seen_argv[0], "rest");
}

FL_TYPE_TEST_SETUP_CLEANUP("A separated value and an explicit bool parse", WbCliTest,
                           flag_separated_values, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    int const argc   = split(t, "create --description \"two words\" --force=false");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_OK);
    FL_ASSERT_STR_EQ(wb_flag_string(&tree.leaf, "description"), "two words");
    FL_ASSERT_FALSE(wb_flag_bool(&tree.leaf, "force"));
}

FL_TYPE_TEST_SETUP_CLEANUP("An unset flag keeps its default and reports it", WbCliTest,
                           flag_defaults, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    int const argc   = split(t, "create");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_OK);
    FL_ASSERT_EQ(wb_flag_i64(&tree.leaf, "count"), (i64)7);
    FL_ASSERT_FALSE(wb_flag_changed(&tree.leaf, "count"));
}

FL_TYPE_TEST_SETUP_CLEANUP("A persistent flag reaches a child", WbCliTest,
                           flag_persistent_inherited, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    // --project is declared on the root, never on the leaf.
    int const argc   = split(t, "create -p 3");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_OK);
    FL_ASSERT_EQ(wb_flag_i64(&tree.leaf, "project"), (i64)3);
    FL_ASSERT_TRUE(wb_flag_changed(&tree.leaf, "project"));
}

FL_TYPE_TEST_SETUP_CLEANUP("A second run does not inherit the first run's flags",
                           WbCliTest, flag_reset_between_runs, setup_cli, cleanup_cli) {
    // The command structs are static, so reset between commands and check that flags are
    // cleared.
    WbFlagTree tree;
    build_flag_tree(&tree);

    int argc = split(t, "create -d first -c 99 -f");
    FL_ASSERT_EQ(wb_command_execute(&tree.root, &t->io, argc, t->argv, t), WB_CLI_OK);
    FL_ASSERT_STR_EQ(wb_flag_string(&tree.leaf, "description"), "first");

    argc = split(t, "create");
    FL_ASSERT_EQ(wb_command_execute(&tree.root, &t->io, argc, t->argv, t), WB_CLI_OK);

    FL_ASSERT_TRUE(wb_flag_string(&tree.leaf, "description") == NULL);
    FL_ASSERT_EQ(wb_flag_i64(&tree.leaf, "count"), (i64)7);
    FL_ASSERT_FALSE(wb_flag_bool(&tree.leaf, "force"));
    FL_ASSERT_FALSE(wb_flag_changed(&tree.leaf, "description"));
}

FL_TYPE_TEST_SETUP_CLEANUP("Everything after -- is an argument", WbCliTest,
                           flag_double_dash, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    int const argc   = split(t, "create -- --description notaflag");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_OK);
    FL_ASSERT_EQ(t->seen_argc, 2);
    FL_ASSERT_STR_EQ(t->seen_argv[0], "--description");
    FL_ASSERT_FALSE(wb_flag_changed(&tree.leaf, "description"));
}

FL_TYPE_TEST_SETUP_CLEANUP("An unknown flag is rejected", WbCliTest,
                           flag_unknown_rejected, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    int const argc   = split(t, "create --nosuchflag");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_USAGE);
    FL_ASSERT_FALSE(t->ran);
    FL_ASSERT_TRUE(strstr(captured(t, t->io.err), "unknown flag") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("A flag missing its value is rejected", WbCliTest,
                           flag_missing_value, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    int const argc   = split(t, "create --description");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_USAGE);
    FL_ASSERT_FALSE(t->ran);
    FL_ASSERT_TRUE(strstr(captured(t, t->io.err), "needs a value") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("A non-numeric integer flag is rejected", WbCliTest,
                           flag_bad_integer, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    int const argc   = split(t, "create --count seven");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_USAGE);
    FL_ASSERT_FALSE(t->ran);
    FL_ASSERT_TRUE(strstr(captured(t, t->io.err), "invalid integer") != NULL);
}

// strtoll saturates at LLONG_MAX and reports the overflow through errno alone, so a
// number nobody could have typed on purpose used to arrive looking like a plausible
// value. A flag carries what was typed or it fails.
FL_TYPE_TEST_SETUP_CLEANUP("A flag value too large for i64 is rejected", WbCliTest,
                           flag_integer_overflow, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    int const argc   = split(t, "create --count 99999999999999999999");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_USAGE);
    FL_ASSERT_FALSE(t->ran);
    FL_ASSERT_TRUE(strstr(captured(t, t->io.err), "invalid integer") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Asking for a flag that was never declared throws", WbCliTest,
                           flag_undeclared_throws, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);
    FL_UNUSED(t);

    bool caught = false;
    FL_TRY {
        (void)wb_flag_string(&tree.leaf, "nosuchflag");
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// Argument validation
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("Too few arguments is a usage error", WbCliTest, args_too_few,
                           setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree); // leaf wants exactly one

    int const argc   = split(t, "module enable");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_USAGE);
    FL_ASSERT_FALSE(t->ran);
    FL_ASSERT_TRUE(strstr(captured(t, t->io.err), "at least 1 argument") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Too many arguments is a usage error", WbCliTest,
                           args_too_many, setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);

    int const argc   = split(t, "module enable magic extra");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_USAGE);
    FL_ASSERT_FALSE(t->ran);
    FL_ASSERT_TRUE(strstr(captured(t, t->io.err), "at most 1 argument") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("A zero-initialized command accepts no arguments", WbCliTest,
                           args_default_is_none, setup_cli, cleanup_cli) {
    WBCommand root = {.usage = "wb", .short_desc = "root"};
    WBCommand leaf = {.usage = "list", .short_desc = "list", .run = record_run};
    wb_command_add(&root, &leaf);

    int const argc   = split(t, "list unexpected");
    int const status = wb_command_execute(&root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_USAGE);
    FL_ASSERT_FALSE(t->ran);
}

FL_TYPE_TEST_SETUP_CLEANUP("The run function receives the user pointer", WbCliTest,
                           run_receives_user, setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);

    int const argc = split(t, "module enable magic");
    FL_ASSERT_EQ(wb_command_execute(&tree.root, &t->io, argc, t->argv, t), WB_CLI_OK);
    FL_ASSERT_TRUE(t->ran);
    FL_ASSERT_EQ_PTR(t->seen_user, (void *)t);
    FL_ASSERT_STR_EQ(t->seen_argv[0], "magic");
}

// ---------------------------------------------------------------------------
// Grouping commands and help
// ---------------------------------------------------------------------------

FL_TYPE_TEST_SETUP_CLEANUP("A group with a stray word reports an unknown command",
                           WbCliTest, group_unknown_command, setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree); // group has no run function

    int const argc   = split(t, "module wobble");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_USAGE);
    char const *err = captured(t, t->io.err);
    FL_ASSERT_TRUE(strstr(err, "unknown command") != NULL);
    FL_ASSERT_TRUE(strstr(err, "wobble") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("A bare group prints its help", WbCliTest, group_prints_help,
                           setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);

    int const argc   = split(t, "module");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_OK);
    char const *out = captured(t, t->io.out);
    FL_ASSERT_TRUE(strstr(out, "Available Commands:") != NULL);
    FL_ASSERT_TRUE(strstr(out, "enable") != NULL);
    FL_ASSERT_TRUE(strstr(out, "a leaf") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("--help prints help instead of running", WbCliTest,
                           help_flag_short_circuits, setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);

    // The leaf needs an argument; --help must win before that check runs.
    int const argc   = split(t, "module enable --help");
    int const status = wb_command_execute(&tree.root, &t->io, argc, t->argv, t);

    FL_ASSERT_EQ(status, WB_CLI_OK);
    FL_ASSERT_FALSE(t->ran);
    FL_ASSERT_TRUE(strstr(captured(t, t->io.out), "wb module enable <code>") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("-h is the same as --help", WbCliTest, help_shorthand,
                           setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);

    int const argc = split(t, "module enable -h");
    FL_ASSERT_EQ(wb_command_execute(&tree.root, &t->io, argc, t->argv, t), WB_CLI_OK);
    FL_ASSERT_FALSE(t->ran);
    FL_ASSERT_TRUE(strstr(captured(t, t->io.out), "Usage:") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Help separates a command's flags from inherited ones",
                           WbCliTest, help_lists_flag_groups, setup_cli, cleanup_cli) {
    WbFlagTree tree;
    build_flag_tree(&tree);

    wb_command_help(&tree.leaf, &t->io);
    char const *out = captured(t, t->io.out);

    FL_ASSERT_TRUE(strstr(out, "-d, --description string") != NULL);
    FL_ASSERT_TRUE(strstr(out, "-c, --count int") != NULL);
    FL_ASSERT_TRUE(strstr(out, "-h, --help") != NULL);
    FL_ASSERT_TRUE(strstr(out, "Global Flags:") != NULL);
    FL_ASSERT_TRUE(strstr(out, "-p, --project int") != NULL);
}

FL_TYPE_TEST_SETUP_CLEANUP("Help omits a hidden command", WbCliTest, help_hides_hidden,
                           setup_cli, cleanup_cli) {
    WBCommand root   = {.usage = "wb", .short_desc = "root"};
    WBCommand shown  = {.usage = "shown", .short_desc = "visible", .run = record_run};
    WBCommand secret = {.usage      = "secret",
                        .short_desc = "hidden one",
                        .run        = record_run,
                        .hidden     = true};
    wb_command_add(&root, &shown);
    wb_command_add(&root, &secret);

    wb_command_help(&root, &t->io);
    char const *out = captured(t, t->io.out);
    FL_ASSERT_TRUE(strstr(out, "shown") != NULL);
    FL_ASSERT_TRUE(strstr(out, "secret") == NULL);

    int const argc = split(t, "secret");
    FL_ASSERT_EQ(wb_command_execute(&root, &t->io, argc, t->argv, t), WB_CLI_OK);
    FL_ASSERT_TRUE(t->ran);
}

FL_TYPE_TEST_SETUP_CLEANUP("A command's path names every ancestor", WbCliTest,
                           path_includes_ancestors, setup_cli, cleanup_cli) {
    WbTestTree tree;
    build_tree(&tree);

    wb_command_write_path(&tree.leaf, t->io.out);
    FL_ASSERT_STR_EQ(captured(t, t->io.out), "wb module enable");
}

// ---------------------------------------------------------------------------
// Suite registration
// ---------------------------------------------------------------------------

FL_SUITE_BEGIN(WorldbuilderCli)
FL_SUITE_ADD_EMBEDDED(tokenize_splits)
FL_SUITE_ADD_EMBEDDED(tokenize_quotes)
FL_SUITE_ADD_EMBEDDED(tokenize_escapes)
FL_SUITE_ADD_EMBEDDED(tokenize_adjacent_quote)
FL_SUITE_ADD_EMBEDDED(tokenize_empty_quotes)
FL_SUITE_ADD_EMBEDDED(tokenize_blank)
FL_SUITE_ADD_EMBEDDED(tokenize_unterminated)
FL_SUITE_ADD_EMBEDDED(tokenize_overflow)
FL_SUITE_ADD_EMBEDDED(line_grows)
FL_SUITE_ADD_EMBEDDED(line_reads_and_ends)
FL_SUITE_ADD_EMBEDDED(name_is_first_word)
FL_SUITE_ADD_EMBEDDED(find_walks_path)
FL_SUITE_ADD_EMBEDDED(find_stops_unknown)
FL_SUITE_ADD_EMBEDDED(find_stops_at_flag)
FL_SUITE_ADD_EMBEDDED(flag_forms)
FL_SUITE_ADD_EMBEDDED(flag_separated_values)
FL_SUITE_ADD_EMBEDDED(flag_defaults)
FL_SUITE_ADD_EMBEDDED(flag_persistent_inherited)
FL_SUITE_ADD_EMBEDDED(flag_reset_between_runs)
FL_SUITE_ADD_EMBEDDED(flag_double_dash)
FL_SUITE_ADD_EMBEDDED(flag_unknown_rejected)
FL_SUITE_ADD_EMBEDDED(flag_missing_value)
FL_SUITE_ADD(parse_i64_whole_token)
FL_SUITE_ADD_EMBEDDED(flag_bad_integer)
FL_SUITE_ADD_EMBEDDED(flag_integer_overflow)
FL_SUITE_ADD_EMBEDDED(flag_undeclared_throws)
FL_SUITE_ADD_EMBEDDED(args_too_few)
FL_SUITE_ADD_EMBEDDED(args_too_many)
FL_SUITE_ADD_EMBEDDED(args_default_is_none)
FL_SUITE_ADD_EMBEDDED(run_receives_user)
FL_SUITE_ADD_EMBEDDED(group_unknown_command)
FL_SUITE_ADD_EMBEDDED(group_prints_help)
FL_SUITE_ADD_EMBEDDED(help_flag_short_circuits)
FL_SUITE_ADD_EMBEDDED(help_shorthand)
FL_SUITE_ADD_EMBEDDED(help_lists_flag_groups)
FL_SUITE_ADD_EMBEDDED(help_hides_hidden)
FL_SUITE_ADD_EMBEDDED(path_includes_ancestors)
FL_SUITE_END;

FL_GET_TEST_SUITE("CLI", WorldbuilderCli)
