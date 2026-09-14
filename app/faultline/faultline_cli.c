/**
 * @file faultline_cli.c
 * @author Douglas Cuthbertson
 * @brief Run one command line, and open what every command shares.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <stdio.h>  // FILE, fflush, fprintf, fputc, fputs
#include <string.h> // strcmp

#include "faultline_cli.h" // FLCliSession, FLCliDbNeed

#include <faultline/fl_exception.h> // FL_REASON, FL_DETAILS
#include <faultline/fl_log.h>       // LOG_ERROR
#include <faultline/fl_try.h>       // FL_TRY, FL_CATCH_ALL, FL_END_TRY
#include <faultline_sqlite.h>       // faultline_init_database, faultline_close_database

static char const *faultline_cli_module = "Faultline CLI";

// Write a caught exception as "error: <reason>: <details>", or "error: <reason>" when
// there are no details.
static void faultline_cli_print_error(FILE *err, char const *reason,
                                      char const *details) {
    if (details != NULL && details[0] != '\0') {
        fprintf(err, "error: %s: %s\n", reason, details);
    } else {
        fprintf(err, "error: %s\n", reason);
    }
}

// --log-level as a log level. An absent or unrecognized name is info.
static FLLogLevel faultline_cli_log_level(char const *name) {
    FLLogLevel level = LOG_LEVEL_INFO;

    if (name == NULL) {
        return level;
    }

    if (strcmp(name, "error") == 0) {
        level = LOG_LEVEL_ERROR;
    } else if (strcmp(name, "warn") == 0) {
        level = LOG_LEVEL_WARN;
    } else if (strcmp(name, "debug") == 0) {
        level = LOG_LEVEL_DEBUG;
    }

    return level;
}

// The session's connection, opened from --db on the first call. A file that cannot be
// opened is reported and leaves the session without one.
static sqlite3 *faultline_cli_connect(WBCommand const *cmd, FLCliSession *session) {
    if (session->db != NULL) {
        return session->db;
    }

    char const *path = wb_flag_string(cmd, "db");
    FL_TRY {
        session->db = faultline_init_database(path);
    }
    FL_CATCH_ALL {
        faultline_cli_print_error(session->io.err, FL_REASON, FL_DETAILS);
        LOG_ERROR(faultline_cli_module, "cannot open the database \"%s\": %s", path,
                  FL_REASON);
    }
    FL_END_TRY;

    return session->db;
}

sqlite3 *faultline_cli_open(WBCommand const *cmd, FLCliSession *session,
                            FLCliDbNeed need) {
    if (session->configure_log != NULL) {
        char const *level = wb_flag_string(cmd, "log-level");
        session->configure_log(faultline_cli_log_level(level),
                               wb_flag_string(cmd, "log-file"));
    }

    // --no-db leaves this command without a database even when the session has one open.
    sqlite3 *db = NULL;
    if (!wb_flag_bool(cmd, "no-db")) {
        db = faultline_cli_connect(cmd, session);
    }

    if (db == NULL && need == FL_CLI_DB_REQUIRED) {
        fputc('"', session->io.err);
        wb_command_write_path(cmd, session->io.err);
        fputs("\" needs the results database\n", session->io.err);
    }

    return db;
}

int faultline_cli_run_once(FLCliSession *session, WBCommand *root, int argc,
                           char **argv) {
    int volatile status = WB_CLI_OK; // written inside FL_TRY, read after it

    FL_TRY {
        status = wb_command_execute(root, &session->io, argc, argv, session);
    }
    FL_CATCH_ALL {
        faultline_cli_print_error(session->io.err, FL_REASON, FL_DETAILS);
        LOG_ERROR(faultline_cli_module, "command failed: %s", FL_REASON);
        status = WB_CLI_ERROR;
    }
    FL_END_TRY;

    // Ordinary output first, so that when both streams go to one terminal, a command
    // that printed before it failed reads in the order it happened.
    fflush(session->io.out);
    fflush(session->io.err);
    return status;
}

void faultline_cli_session_free(FLCliSession *session) {
    if (session->db != NULL) {
        faultline_close_database(session->db);
        session->db = NULL;
    }
}
