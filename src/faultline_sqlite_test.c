/**
 * @file faultline_sqlite_test.c
 * @author Douglas Cuthbertson
 * @brief Test cases for opening a results database: the version gates and schema v1.
 * @version 0.1
 * @date 2025-09-06
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Each case owns a private database file named in setup and removed in cleanup. Cleanup
 * runs even when the body throws, so a failed assertion still closes the connection and
 * removes the file.
 */
#include <faultline_sqlite.h>

#include "faultline_sqlite_test.h"         // TestSchema
#include <faultline/fl_exception_assert.h> // FL_ASSERT_*, FL_FAIL
#include <faultline/fl_macros.h>           // FL_ARRAY_COUNT, FL_CONTAINER_OF
#include <faultline/fl_result_codes.h>     // FLResultCode
#include <faultline/fl_test.h>             // FL_TYPE_TEST_SETUP_CLEANUP
#include <faultline/fl_try.h>              // FL_TRY, FL_CATCH, FL_END_TRY
#include <faultline/fl_types.h>            // FLFailureType, FLTestPhase
#include <sqlite/sqlite3.h>

#include <stdbool.h> // bool
#include <stdio.h>   // snprintf, remove, fopen_s

#ifdef _WIN32
#include <io.h>       // _chmod
#include <sys/stat.h> // _S_IREAD, _S_IWRITE
#define FL_RO_MODE _S_IREAD
#define FL_RW_MODE (_S_IREAD | _S_IWRITE)
#define fl_chmod   _chmod
#else
#include <sys/stat.h> // chmod, S_IRUSR, S_IWUSR
#define FL_RO_MODE S_IRUSR
#define FL_RW_MODE (S_IRUSR | S_IWUSR)
#define fl_chmod   chmod
#endif

enum {
    // test_db_insert_result's phase argument for a result that records no phase
    TEST_DB_NO_PHASE = -1
};

///////////////
//  Fixture  //
///////////////

static int schema_test_counter = 0;

// Remove a case's database file and the sidecar files a journal or WAL connection leaves.
static void test_db_remove(TestSchema const *t) {
    char sidecar[sizeof t->path + sizeof "-journal"];
    remove(t->path);
    snprintf(sidecar, sizeof sidecar, "%s-journal", t->path);
    remove(sidecar);
    snprintf(sidecar, sizeof sidecar, "%s-wal", t->path);
    remove(sidecar);
    snprintf(sidecar, sizeof sidecar, "%s-shm", t->path);
    remove(sidecar);
}

// A fresh path per case. The counter also advances across fault-injection re-runs of a
// case, so a re-run never opens the previous pass's file.
static FL_SETUP_FN(setup_schema_db) {
    TestSchema *t = FL_CONTAINER_OF(tc, TestSchema, tc);
    t->db         = NULL;
    snprintf(t->path, sizeof t->path, "faultline_schema_test_%d.sqlite",
             ++schema_test_counter);
    test_db_remove(t);
}

static FL_CLEANUP_FN(cleanup_schema_db) {
    TestSchema *t = FL_CONTAINER_OF(tc, TestSchema, tc);
    if (t->db != NULL) {
        sqlite3_close_v2(t->db);
        t->db = NULL;
    }
    test_db_remove(t);
}

///////////////
//  Helpers  //
///////////////

// Open a connection directly, bypassing the code under test, to arrange or inspect a
// file.
static sqlite3 *test_db_open_raw(char const *path) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close_v2(db);
        FL_FAIL("sqlite3_open_v2('%s') returned %s", path, sqlite3_errstr(rc));
    }
    return db;
}

// Run SQL that must succeed.
static void test_db_exec(sqlite3 *db, char const *sql) {
    char *errmsg = NULL;
    int   rc     = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        char details[320];
        snprintf(details, sizeof details, "%s in \"%.160s\"",
                 errmsg ? errmsg : sqlite3_errstr(rc), sql);
        sqlite3_free(errmsg); // free before the throw
        FL_FAIL("%s", details);
    }
}

// Run SQL that may fail, and return its result code.
static int test_db_exec_rc(sqlite3 *db, char const *sql) {
    return sqlite3_exec(db, sql, NULL, NULL, NULL);
}

// First column of the first row as an int, or fallback when the query yields no row.
static int test_db_query_int(sqlite3 *db, char const *sql, int fallback) {
    sqlite3_stmt *stmt  = NULL;
    int           value = fallback;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK
        && sqlite3_step(stmt) == SQLITE_ROW) {
        value = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return value;
}

// First column of the first row as text, or "" when the query yields no row.
static void test_db_query_text(sqlite3 *db, char const *sql, char *out, size_t size) {
    sqlite3_stmt *stmt = NULL;
    out[0]             = '\0';
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK
        && sqlite3_step(stmt) == SQLITE_ROW) {
        char const *text = (char const *)sqlite3_column_text(stmt, 0);
        if (text != NULL) {
            snprintf(out, size, "%s", text);
        }
    }
    sqlite3_finalize(stmt);
}

static bool test_db_object_exists(sqlite3 *db, char const *type, char const *name) {
    char sql[160];
    snprintf(sql, sizeof sql,
             "SELECT count(*) FROM sqlite_master WHERE type = '%s' AND name = '%s';", type,
             name);
    return test_db_query_int(db, sql, 0) > 0;
}

// Count the objects of one type, leaving out SQLite's own (sqlite_sequence and the
// sqlite_autoindex_ entries behind UNIQUE constraints).
static int test_db_object_count(sqlite3 *db, char const *type) {
    char sql[128];
    snprintf(sql, sizeof sql,
             "SELECT count(*) FROM sqlite_master WHERE type = '%s'"
             " AND name NOT LIKE 'sqlite_%%';",
             type);
    return test_db_query_int(db, sql, -1);
}

// Assert that exactly the named objects of one type exist.
static void test_db_expect_objects(sqlite3 *db, char const *type,
                                   char const *const *names, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!test_db_object_exists(db, type, names[i])) {
            FL_FAIL("expected %s '%s'", type, names[i]);
        }
    }

    FL_ASSERT_EQ((int)count, test_db_object_count(db, type));
}

// Stamp user_version on a file without applying any schema.
static void test_db_stamp_version(char const *path, int version) {
    char sql[40];
    snprintf(sql, sizeof sql, "PRAGMA user_version = %d;", version);

    sqlite3 *db = test_db_open_raw(path);
    test_db_exec(db, sql);
    sqlite3_close_v2(db);
}

static int test_db_version(char const *path) {
    sqlite3 *db      = test_db_open_raw(path);
    int      version = test_db_query_int(db, "PRAGMA user_version;", -1);
    sqlite3_close_v2(db);
    return version;
}

// Assert that a refused file kept its version, gained no schema v1 table, and was not
// switched to WAL.
static void test_db_expect_untouched(char const *path, int version) {
    FL_ASSERT_EQ(version, test_db_version(path));

    sqlite3   *db         = test_db_open_raw(path);
    bool const has_suites = test_db_object_exists(db, "table", "suites");
    char       mode[16];
    test_db_query_text(db, "PRAGMA journal_mode;", mode, sizeof mode);
    sqlite3_close_v2(db);

    FL_ASSERT_FALSE(has_suites);
    FL_ASSERT_STR_EQ("delete", mode);
}

// Insert one result into a run of its own, so UNIQUE(run_id, case_id) never decides the
// outcome. Expects suite 1 and case 1 to exist. Returns the result code of the insert.
static int test_db_insert_result(sqlite3 *db, int result_code, int failure_type,
                                 int failure_phase) {
    char phase[16];
    if (failure_phase == TEST_DB_NO_PHASE) {
        snprintf(phase, sizeof phase, "%s", "NULL");
    } else {
        snprintf(phase, sizeof phase, "%d", failure_phase);
    }

    char sql[640];
    snprintf(sql, sizeof sql,
             "INSERT INTO runs (suite_id, started_at, completed_at, case_count)"
             " VALUES (1, '2026-01-01T00:00:00Z', '2026-01-01T00:00:00Z', 1);"
             "INSERT INTO results (run_id, case_id, case_index, result_code, failure_type,"
             " failure_phase, elapsed_seconds, discovery_seconds, injection_seconds,"
             " fault_sites, discovery_failures, injection_failures)"
             " VALUES (last_insert_rowid(), 1, 0, %d, %d, %s, 0, 0, 0, 0, 0, 0);",
             result_code, failure_type, phase);

    return test_db_exec_rc(db, sql);
}

// Insert one fault on result 1 at the next free position.
static int test_db_insert_fault(sqlite3 *db, int result_code) {
    char sql[160];
    snprintf(sql, sizeof sql,
             "INSERT INTO faults (result_id, position, fault_index, result_code)"
             " VALUES (1, (SELECT count(*) FROM faults), 0, %d);",
             result_code);

    return test_db_exec_rc(db, sql);
}

//////////////////////
//  Schema Objects  //
//////////////////////

// A file that does not exist yet is created and stamped with the current version.
FL_TYPE_TEST_SETUP_CLEANUP("Fresh Database Is Stamped", TestSchema,
                           schema_stamps_fresh_database, setup_schema_db,
                           cleanup_schema_db) {
    t->db = faultline_init_database(t->path);

    FL_ASSERT_NOT_NULL(t->db);
    FL_ASSERT_EQ(FL_SCHEMA_VERSION,
                     test_db_query_int(t->db, "PRAGMA user_version;", -1));
}

// Opening a current database again changes neither its version nor its objects.
FL_TYPE_TEST_SETUP_CLEANUP("Reopen Is Idempotent", TestSchema,
                           schema_reopen_is_idempotent, setup_schema_db,
                           cleanup_schema_db) {
    t->db             = faultline_init_database(t->path);
    int const objects = test_db_query_int(t->db, "SELECT count(*) FROM sqlite_master;", -1);
    faultline_close_database(t->db);
    t->db = NULL;

    t->db = faultline_init_database(t->path);

    FL_ASSERT_EQ(FL_SCHEMA_VERSION,
                     test_db_query_int(t->db, "PRAGMA user_version;", -1));
    FL_ASSERT_EQ(objects,
                     test_db_query_int(t->db, "SELECT count(*) FROM sqlite_master;", -1));
}

// Every table schema v1 defines, and no others.
FL_TYPE_TEST_SETUP_CLEANUP("Schema Creates Every Table", TestSchema,
                           schema_creates_every_table, setup_schema_db,
                           cleanup_schema_db) {
    static char const *const expected[] = {"suites", "cases", "runs", "results", "faults"};

    t->db = faultline_init_database(t->path);

    test_db_expect_objects(t->db, "table", expected, FL_ARRAY_COUNT(expected));
}

// Every index schema v1 defines, and no others. Dropping an index changes no query's
// rows, only its cost, so nothing but this case would report the loss.
FL_TYPE_TEST_SETUP_CLEANUP("Schema Creates Every Index", TestSchema,
                           schema_creates_every_index, setup_schema_db,
                           cleanup_schema_db) {
    static char const *const expected[] = {"idx_runs_suite", "idx_cases_baseline_run",
                                           "idx_results_case", "idx_results_failed"};

    t->db = faultline_init_database(t->path);

    test_db_expect_objects(t->db, "index", expected, FL_ARRAY_COUNT(expected));
}

// Every view schema v1 defines, and no others.
FL_TYPE_TEST_SETUP_CLEANUP("Schema Creates Every View", TestSchema,
                           schema_creates_every_view, setup_schema_db,
                           cleanup_schema_db) {
    static char const *const expected[] = {"run_totals"};

    t->db = faultline_init_database(t->path);

    test_db_expect_objects(t->db, "view", expected, FL_ARRAY_COUNT(expected));
}

// The connection enforces foreign keys: a run naming no suite is rejected.
FL_TYPE_TEST_SETUP_CLEANUP("Foreign Keys Are Enforced", TestSchema,
                           schema_enforces_foreign_keys, setup_schema_db,
                           cleanup_schema_db) {
    t->db = faultline_init_database(t->path);

    FL_ASSERT_EQ(1, test_db_query_int(t->db, "PRAGMA foreign_keys;", -1));

    int const rc = test_db_exec_rc(t->db,
                                   "INSERT INTO runs (suite_id, started_at, completed_at,"
                                   " case_count) VALUES (999, 'x', 'x', 0);");
    FL_ASSERT_EQ(SQLITE_CONSTRAINT, rc);
    FL_ASSERT_EQ(SQLITE_CONSTRAINT_FOREIGNKEY, sqlite3_extended_errcode(t->db));
}

// The schema's CHECK ranges are written as numbers. These inserts tie the numbers to the
// enums they encode, so a new enum value fails here until the schema accepts it.
FL_TYPE_TEST_SETUP_CLEANUP("Checks Match Enums", TestSchema, schema_checks_match_enums,
                           setup_schema_db, cleanup_schema_db) {
    t->db = faultline_init_database(t->path);
    test_db_exec(t->db, "INSERT INTO suites (name) VALUES ('S');"
                        "INSERT INTO cases (suite_id, name) VALUES (1, 'c');");

    // The last value of each enum is accepted.
    FL_ASSERT_EQ(SQLITE_OK,
                     test_db_insert_result(t->db, FL_BAD_STATE, FL_INVALID_FREE_FAILURE,
                                           FL_INJECTION_PHASE));

    // One past it is not.
    FL_ASSERT_EQ(SQLITE_CONSTRAINT,
                     test_db_insert_result(t->db, FL_BAD_STATE + 1, FL_TEST_FAILURE,
                                           FL_DISCOVERY_PHASE));
    FL_ASSERT_EQ(SQLITE_CONSTRAINT,
                     test_db_insert_result(t->db, FL_FAIL, FL_INVALID_FREE_FAILURE + 1,
                                           FL_DISCOVERY_PHASE));
    FL_ASSERT_EQ(SQLITE_CONSTRAINT,
                     test_db_insert_result(t->db, FL_FAIL, FL_TEST_FAILURE,
                                           FL_INJECTION_PHASE + 1));

    // A result records a phase exactly when it records a failure.
    FL_ASSERT_EQ(SQLITE_CONSTRAINT,
                     test_db_insert_result(t->db, FL_PASS, FL_FAILURE_NONE,
                                           FL_DISCOVERY_PHASE));
    FL_ASSERT_EQ(SQLITE_CONSTRAINT,
                     test_db_insert_result(t->db, FL_FAIL, FL_TEST_FAILURE,
                                           TEST_DB_NO_PHASE));
    FL_ASSERT_EQ(SQLITE_OK, test_db_insert_result(t->db, FL_PASS, FL_FAILURE_NONE,
                                                      TEST_DB_NO_PHASE));

    // A fault's result code has the same range.
    FL_ASSERT_EQ(SQLITE_OK, test_db_insert_fault(t->db, FL_BAD_STATE));
    FL_ASSERT_EQ(SQLITE_CONSTRAINT, test_db_insert_fault(t->db, FL_BAD_STATE + 1));
}

// Opening switches the file to WAL.
FL_TYPE_TEST_SETUP_CLEANUP("Database Runs In WAL Mode", TestSchema, schema_runs_in_wal,
                           setup_schema_db, cleanup_schema_db) {
    t->db = faultline_init_database(t->path);

    char mode[16];
    test_db_query_text(t->db, "PRAGMA journal_mode;", mode, sizeof mode);
    FL_ASSERT_STR_EQ("wal", mode);
}

/////////////////////
//  Version Gates  //
/////////////////////

// A database written by a newer build is refused and left as it was.
FL_TYPE_TEST_SETUP_CLEANUP("Future Version Is Refused", TestSchema,
                           schema_refuses_future_version, setup_schema_db,
                           cleanup_schema_db) {
    int const future = FL_SCHEMA_VERSION + 1;
    test_db_stamp_version(t->path, future);

    bool caught = false;
    FL_TRY {
        t->db = faultline_init_database(t->path);
    }
    FL_CATCH(faultline_db_version_too_new) {
        caught = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(caught);
    test_db_expect_untouched(t->path, future);
}

// A file holding tables but no version predates schema v1. It is refused, and its tables
// stay where they are.
FL_TYPE_TEST_SETUP_CLEANUP("Legacy Schema Is Refused", TestSchema,
                           schema_refuses_legacy_schema, setup_schema_db,
                           cleanup_schema_db) {
    sqlite3 *seed = test_db_open_raw(t->path);
    test_db_exec(seed, "CREATE TABLE raw_test_runs (id INTEGER PRIMARY KEY);");
    sqlite3_close_v2(seed);

    bool caught = false;
    FL_TRY {
        t->db = faultline_init_database(t->path);
    }
    FL_CATCH(faultline_db_legacy_schema) {
        caught = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(caught);
    test_db_expect_untouched(t->path, 0);

    sqlite3   *inspect = test_db_open_raw(t->path);
    bool const kept    = test_db_object_exists(inspect, "table", "raw_test_runs");
    sqlite3_close_v2(inspect);
    FL_ASSERT_TRUE(kept);
}

// A view already named results makes the schema step fail. Nothing the step created
// before the failure remains, and the version is not stamped.
FL_TYPE_TEST_SETUP_CLEANUP("Failed Schema Rolls Back", TestSchema,
                           schema_failure_rolls_back, setup_schema_db, cleanup_schema_db) {
    sqlite3 *seed = test_db_open_raw(t->path);
    test_db_exec(seed, "CREATE VIEW results AS SELECT 1 AS x;");
    sqlite3_close_v2(seed);

    bool caught = false;
    FL_TRY {
        t->db = faultline_init_database(t->path);
    }
    FL_CATCH(faultline_db_create_failed) {
        caught = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(caught);
    FL_ASSERT_EQ(0, test_db_version(t->path));

    sqlite3  *inspect = test_db_open_raw(t->path);
    int const tables  = test_db_object_count(inspect, "table");
    sqlite3_close_v2(inspect);
    FL_ASSERT_EQ(0, tables);
}

//////////////////////
//  Error Handling  //
//////////////////////

static FL_SETUP_FN(setup_invalid_path) {
    TestSchema *t = FL_CONTAINER_OF(tc, TestSchema, tc);
    t->db         = NULL;
    snprintf(t->path, sizeof t->path, "%s", "/invalid/nonexistent/path/test.db");
}

FL_TYPE_TEST_SETUP_CLEANUP("Invalid Path", TestSchema, schema_invalid_path_throws,
                           setup_invalid_path, cleanup_schema_db) {
    bool caught = false;
    FL_TRY {
        t->db = faultline_init_database(t->path);
    }
    FL_CATCH(faultline_db_create_failed) {
        caught = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(caught);
}

// Create an empty file and mark it read-only. SQLite opens a write-protected file
// read-only, so the version gates pass and the schema transaction is the first write,
// which fails. The read-only attribute holds even for an elevated process, so this does
// not depend on a system directory being write-protected.
static FL_SETUP_FN(setup_read_only) {
    setup_schema_db(tc);
    TestSchema *t = FL_CONTAINER_OF(tc, TestSchema, tc);

    FILE *f = NULL;
    fopen_s(&f, t->path, "wb");
    if (f != NULL) {
        (void)fclose(f);
    }
    (void)fl_chmod(t->path, FL_RO_MODE);
}

// remove() cannot delete a read-only file on Windows, so restore write access first.
static FL_CLEANUP_FN(cleanup_read_only) {
    TestSchema *t = FL_CONTAINER_OF(tc, TestSchema, tc);
    (void)fl_chmod(t->path, FL_RW_MODE);
    cleanup_schema_db(tc);
}

FL_TYPE_TEST_SETUP_CLEANUP("Read-Only File", TestSchema, schema_read_only_throws,
                           setup_read_only, cleanup_read_only) {
    bool caught = false;
    FL_TRY {
        t->db = faultline_init_database(t->path);
    }
    FL_CATCH(faultline_db_create_failed) {
        caught = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(caught);
}
