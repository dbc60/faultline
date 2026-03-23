/**
 * @file command_test.c
 * @author Douglas Cuthbertson
 * @brief Unit tests for command-line parsing
 * @version 0.1
 * @date 2026-01-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service
#include "fla_log_service.c"       // g_fla_log_service
#include "fla_memory_service.c"    // g_fla_memory_service, fla_set_memory_service

#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_* macros
#include <faultline/fl_macros.h>                   // FL_CONTAINER_OF, FL_UNUSED
#include <faultline/fl_test.h>                     // FL_TYPE_TEST_SETUP_CLEANUP, FL_GET_TEST_SUITE
#include <faultline/fl_try.h>                      // FL_TRY, FL_CATCH, FL_END_TRY

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
// must be included after fla_memory_service.c so the macros brought in by
// fla_memory_service.h are defined before malloc, calloc, and free are declared in
// stdlib.h that command.c includes.
#include "command.c"

// ============================================================================
// Test Command Definitions
// ============================================================================

typedef struct TestCommand {
    FLTestCase     tc;
    FormalCommand *commands;
} TestCommand;

// Simple handler that just records it was called
static bool        handler_called    = false;
static char const *last_command_name = NULL;

COMMAND_HANDLER(test_cmd_handler) {
    handler_called    = true;
    last_command_name = cmd->command->name;
}

// Test options
static FormalOption test_options[] = {
    {"f", "force", "Force operation", false, NULL},
    {"l", "limit", "Limit count", true, NULL},
    {"v", "verbose", "Verbose output", false, NULL},
    {NULL, NULL, NULL, false, NULL}, // NULL terminator
};

// Test subcommands
static FormalCommand test_subcommands[] = {
    {"sub1", "First subcommand", test_cmd_handler, NULL, NULL, NULL},
    {"sub2", "Second subcommand", test_cmd_handler, test_options, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL, NULL}, // NULL terminator
};

// Test commands
static FormalCommand test_commands[] = {
    {"simple", "Simple command", test_cmd_handler, NULL, NULL, NULL},
    {"options", "Command with options", test_cmd_handler, test_options, NULL, NULL},
    {"nested", "Command with subcommands", test_cmd_handler, NULL, test_subcommands, NULL},
    {NULL, NULL, NULL, NULL, NULL, NULL}, // NULL terminator
};

// ============================================================================
// Test Setup/Cleanup
// ============================================================================

static void setup_command_tests(FLTestCase *tc) {
    TestCommand *t    = FL_CONTAINER_OF(tc, TestCommand, tc);
    t->commands       = test_commands;
    handler_called    = false;
    last_command_name = NULL;
}

static void cleanup_command_tests(FLTestCase *tc) {
    FL_UNUSED(tc);
    // Nothing to clean up - arena-allocated
}

// ============================================================================
// Tests for find_command (internal, but we can test via parse_command)
// ============================================================================
FL_TYPE_TEST_SETUP_CLEANUP("find command valid", TestCommand, test_find_command_valid,
                           setup_command_tests, cleanup_command_tests) {
    char *argv[] = {"program", "simple"};
    int   argc   = 2;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_NOT_NULL(cmd->command);
    FL_ASSERT_STR_EQ(cmd->command->name, "simple");
}

FL_TYPE_TEST_SETUP_CLEANUP("find invalid command", TestCommand,
                           test_find_command_invalid, setup_command_tests, NULL) {
    char *argv[] = {"program", "nonexistent"};
    int   argc   = 2;

    // Should throw command_unknown exception
    FL_TRY {
        (void)parse_command(t->commands, argc, argv);
    }
    FL_CATCH(command_unknown) {
        // Expected exception
    }
    FL_END_TRY;
}

// ============================================================================
// Tests for parse_command
// ============================================================================
FL_TYPE_TEST_SETUP_CLEANUP("parse simple command", TestCommand,
                           test_parse_simple_command, setup_command_tests, NULL) {
    char *argv[] = {"program", "simple"};
    int   argc   = 2;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_STR_EQ(cmd->command->name, "simple");
    FL_ASSERT_NULL(cmd->subcommand);
    FL_ASSERT_NULL(cmd->options[0].option); // No options
    FL_ASSERT_EQ_INT(cmd->argc, 0);         // No positional args
}

FL_TYPE_TEST_SETUP_CLEANUP("parse command with long option", TestCommand,
                           test_parse_command_with_long_option, setup_command_tests,
                           NULL) {
    char *argv[] = {"program", "options", "--force"};
    int   argc   = 3;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_STR_EQ(cmd->command->name, "options");
    FL_ASSERT_TRUE(has_option(cmd, "force"));
    FL_ASSERT_FALSE(has_option(cmd, "verbose"));
}

FL_TYPE_TEST_SETUP_CLEANUP("parse command with short option", TestCommand,
                           test_parse_command_with_short_option, setup_command_tests,
                           NULL) {
    char *argv[] = {"program", "options", "-f"};
    int   argc   = 3;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_TRUE(has_option(cmd, "force"));
}

FL_TYPE_TEST_SETUP_CLEANUP("parse option with argument space", TestCommand,
                           test_parse_option_with_argument_space, setup_command_tests,
                           NULL) {
    char *argv[] = {"program", "options", "--limit", "10"};
    int   argc   = 4;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_TRUE(has_option(cmd, "limit"));

    char const *limit = get_string_option(cmd, "limit", NULL);
    FL_ASSERT_NOT_NULL(limit);
    FL_ASSERT_STR_EQ(limit, "10");
}

FL_TYPE_TEST_SETUP_CLEANUP("parse option with argument equals", TestCommand,
                           test_parse_option_with_argument_equals, setup_command_tests,
                           NULL) {
    char *argv[] = {"program", "options", "--limit=25"};
    int   argc   = 3;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);

    char const *limit = get_string_option(cmd, "limit", NULL);
    FL_ASSERT_NOT_NULL(limit);
    FL_ASSERT_STR_EQ(limit, "25");
}

FL_TYPE_TEST_SETUP_CLEANUP("parse multiple options", TestCommand,
                           test_parse_multiple_options, setup_command_tests, NULL) {
    char *argv[] = {"program", "options", "-f", "-v", "--limit", "5"};
    int   argc   = 6;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_TRUE(has_option(cmd, "force"));
    FL_ASSERT_TRUE(has_option(cmd, "verbose"));
    FL_ASSERT_TRUE(has_option(cmd, "limit"));

    int limit = get_int_option(cmd, "limit", 0);
    FL_ASSERT_EQ_INT(limit, 5);
}

FL_TYPE_TEST_SETUP_CLEANUP("parse positional arguments", TestCommand,
                           test_parse_positional_arguments, setup_command_tests, NULL) {
    char *argv[] = {"program", "simple", "arg1", "arg2", "arg3"};
    int   argc   = 5;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_EQ_INT(cmd->argc, 3);
    FL_ASSERT_STR_EQ(cmd->args[0], "arg1");
    FL_ASSERT_STR_EQ(cmd->args[1], "arg2");
    FL_ASSERT_STR_EQ(cmd->args[2], "arg3");
}

FL_TYPE_TEST_SETUP_CLEANUP("parse options and arguments", TestCommand,
                           test_parse_options_and_arguments, setup_command_tests, NULL) {
    char *argv[]
        = {"program", "options", "-f", "--limit", "10", "file1.txt", "file2.txt"};
    int argc = 7;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_TRUE(has_option(cmd, "force"));
    FL_ASSERT_EQ_INT(get_int_option(cmd, "limit", 0), 10);
    FL_ASSERT_EQ_INT(cmd->argc, 2);
    FL_ASSERT_STR_EQ(cmd->args[0], "file1.txt");
    FL_ASSERT_STR_EQ(cmd->args[1], "file2.txt");
}

FL_TYPE_TEST_SETUP_CLEANUP("parse double-dash separator", TestCommand,
                           test_parse_double_dash_separator, setup_command_tests, NULL) {
    char *argv[] = {"program", "simple", "--", "--not-an-option", "-f"};
    int   argc   = 5;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_FALSE(has_option(cmd,
                               "force")); // -f is after --, so it's a positional arg
    FL_ASSERT_EQ_INT(cmd->argc, 2);
    FL_ASSERT_STR_EQ(cmd->args[0], "--not-an-option");
    FL_ASSERT_STR_EQ(cmd->args[1], "-f");
}

FL_TYPE_TEST_SETUP_CLEANUP("parse subcommand", TestCommand, test_parse_subcommand,
                           setup_command_tests, NULL) {
    char *argv[] = {"program", "nested", "sub1"};
    int   argc   = 3;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_STR_EQ(cmd->command->name, "nested");
    FL_ASSERT_NOT_NULL(cmd->subcommand);
    FL_ASSERT_STR_EQ(cmd->subcommand->command->name, "sub1");
}

FL_TYPE_TEST_SETUP_CLEANUP("parse subcommand with options", TestCommand,
                           test_parse_subcommand_with_options, setup_command_tests,
                           NULL) {
    char *argv[] = {"program", "nested", "sub2", "--force", "--limit", "42"};
    int   argc   = 6;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_NOT_NULL(cmd);
    FL_ASSERT_NOT_NULL(cmd->subcommand);
    FL_ASSERT_STR_EQ(cmd->subcommand->command->name, "sub2");
    FL_ASSERT_TRUE(has_option(cmd->subcommand, "force"));
    FL_ASSERT_EQ_INT(get_int_option(cmd->subcommand, "limit", 0), 42);
}

// ============================================================================
// Tests for option helper functions
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("has option", TestCommand, test_has_option,
                           setup_command_tests, NULL) {
    char *argv[] = {"program", "options", "-f", "-v"};
    int   argc   = 4;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    FL_ASSERT_TRUE(has_option(cmd, "force"));
    FL_ASSERT_TRUE(has_option(cmd, "verbose"));
    FL_ASSERT_FALSE(has_option(cmd, "limit"));
    FL_ASSERT_FALSE(has_option(cmd, "nonexistent"));
}

FL_TYPE_TEST_SETUP_CLEANUP("get string option", TestCommand, test_get_string_option,
                           setup_command_tests, NULL) {
    char *argv[] = {"program", "options", "--limit", "hello"};
    int   argc   = 4;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    char const *value = get_string_option(cmd, "limit", "default");
    FL_ASSERT_STR_EQ(value, "hello");

    char const *missing = get_string_option(cmd, "nonexistent", "default");
    FL_ASSERT_STR_EQ(missing, "default");
}

FL_TYPE_TEST_SETUP_CLEANUP("get int option", TestCommand, test_get_int_option,
                           setup_command_tests, NULL) {
    char *argv[] = {"program", "options", "--limit", "99"};
    int   argc   = 4;

    RuntimeCommand *cmd = parse_command(t->commands, argc, argv);

    int value = get_int_option(cmd, "limit", 0);
    FL_ASSERT_EQ_INT(value, 99);

    int missing = get_int_option(cmd, "nonexistent", 42);
    FL_ASSERT_EQ_INT(missing, 42);
}

// ============================================================================
// Test Suite Definition
// ============================================================================

FL_SUITE_BEGIN(command_tests)
FL_SUITE_ADD_EMBEDDED(test_find_command_valid)
FL_SUITE_ADD_EMBEDDED(test_find_command_invalid)
FL_SUITE_ADD_EMBEDDED(test_parse_simple_command)
FL_SUITE_ADD_EMBEDDED(test_parse_command_with_long_option)
FL_SUITE_ADD_EMBEDDED(test_parse_command_with_short_option)
FL_SUITE_ADD_EMBEDDED(test_parse_option_with_argument_space)
FL_SUITE_ADD_EMBEDDED(test_parse_option_with_argument_equals)
FL_SUITE_ADD_EMBEDDED(test_parse_multiple_options)
FL_SUITE_ADD_EMBEDDED(test_parse_positional_arguments)
FL_SUITE_ADD_EMBEDDED(test_parse_options_and_arguments)
FL_SUITE_ADD_EMBEDDED(test_parse_double_dash_separator)
FL_SUITE_ADD_EMBEDDED(test_parse_subcommand)
FL_SUITE_ADD_EMBEDDED(test_parse_subcommand_with_options)
FL_SUITE_ADD_EMBEDDED(test_has_option)
FL_SUITE_ADD_EMBEDDED(test_get_string_option)
FL_SUITE_ADD_EMBEDDED(test_get_int_option)
FL_SUITE_END;

FL_GET_TEST_SUITE("Command Parsing", command_tests);
