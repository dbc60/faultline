#ifndef FAULTLINE_CLI_H_
#define FAULTLINE_CLI_H_
/**
 * @file faultline_cli.h
 * @author Douglas Cuthbertson
 * @brief The state a faultline command runs with, and the call that runs one command
 * line.
 * @version 0.1
 * @date 2026-09-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * The command engine passes each run function a `void *user`. In faultline that pointer
 * is an FLCliSession.
 */
#include <wb_cli.h> // WBCliIO, WBCommand

#include <faultline/arena.h>                    // Arena
#include <faultline/fl_context.h>               // FLContext
#include <faultline/flp_fault_memory_context.h> // FLFaultMemoryContext
#include <platform_api.h>                       // FLPlatformAPI, fl_configure_log_fn
#include <sqlite/sqlite3.h>                     // sqlite3

#if defined(__cplusplus)
extern "C" {
#endif

// The database file used when --db is not given.
#define FL_DEFAULT_DB_PATH "./faultline.sqlite"

/**
 * @brief Everything a command reaches beyond its own arguments and flags.
 *
 * The entry point fills every field except `db`, which faultline_cli_open() sets.
 */
typedef struct FLCliSession {
    // The results database. NULL until a command opens it.
    sqlite3 *db;

    // The test context the run command drives, and the arena behind it and JUnit output.
    FLContext *fctx;
    Arena     *arena;

    // How the run command reaches suite modules: the monolithic driver sets mem_ctx and
    // the split core sets platform. The other is NULL.
    FLFaultMemoryContext *mem_ctx;
    FLPlatformAPI const  *platform;

    // Applies --log-level and --log-file. NULL leaves logging as it is.
    fl_configure_log_fn *configure_log;

    WBCliIO io;
} FLCliSession;

/**
 * @brief Whether a command can run without the results database.
 */
typedef enum FLCliDbNeed {
    FL_CLI_DB_OPTIONAL, // the command runs without one
    FL_CLI_DB_REQUIRED  // the command reads or writes results
} FLCliDbNeed;

/**
 * @brief Build and wire the command tree. Static storage: the same root every call.
 */
WBCommand *faultline_cli_root(void);

/**
 * @brief Run one command line and return its WB_CLI_* status.
 *
 * An exception a command raises is written to session->io.err, and the status is
 * WB_CLI_ERROR.
 *
 * @param argv the command line after the program name
 */
int faultline_cli_run_once(FLCliSession *session, WBCommand *root, int argc, char **argv);

/**
 * @brief Apply the flags every command inherits, and return the results database.
 *
 * --log-level and --log-file go to session->configure_log. The database named by --db is
 * opened on the first call and stays open for the session; a file that cannot be opened
 * is reported to session->io.err. --no-db returns NULL.
 *
 * @return NULL when the command has no database. With FL_CLI_DB_REQUIRED, a message
 * saying so has been written to session->io.err.
 */
sqlite3 *faultline_cli_open(WBCommand const *cmd, FLCliSession *session,
                            FLCliDbNeed need);

/**
 * @brief Close the database the session opened. Safe to call twice.
 */
void faultline_cli_session_free(FLCliSession *session);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_CLI_H_
