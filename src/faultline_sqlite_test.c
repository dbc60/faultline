/**
 * @file faultline_sqlite_test.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-09-06
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline_sqlite.h>

#include <faultline/fl_test.h>
#include <faultline/fl_try.h>
#include <faultline/fl_exception_service_assert.h> // assert macros and fl_unexpected_failure declaration
#include <sqlite/sqlite3.h>

#include <stdio.h>

typedef struct {
    FLTestCase  tc;
    char const *test_db;
} TestSchema;

///////////////////////////
//  Test Infrastructure  //
///////////////////////////

// Helper function to verify table exists
static void verify_table_exists(sqlite3 *db, char const *table_name) {
    sqlite3_stmt *stmt;
    char          query[256];

    snprintf(query, sizeof(query),
             "SELECT name FROM sqlite_master WHERE type='table' AND name='%s'",
             table_name);

    FL_ASSERT_EQ_INT(SQLITE_OK, sqlite3_prepare_v2(db, query, -1, &stmt, NULL));
    FL_ASSERT_EQ_INT(SQLITE_ROW, sqlite3_step(stmt)); // Should find the table
    sqlite3_finalize(stmt);
}

// Helper function to verify view exists
static void verify_view_exists(sqlite3 *db, char const *view_name) {
    sqlite3_stmt *stmt;
    char          query[256];

    snprintf(query, sizeof(query),
             "SELECT name FROM sqlite_master WHERE type='view' AND name='%s'",
             view_name);

    FL_ASSERT_EQ_INT(SQLITE_OK, sqlite3_prepare_v2(db, query, -1, &stmt, NULL));
    FL_ASSERT_EQ_INT(SQLITE_ROW, sqlite3_step(stmt)); // Should find the view
    sqlite3_finalize(stmt);
}

// Helper function to clean up test database
static void cleanup_test_db(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    remove(t->test_db);
}

////////////////////////////////
//  Core Functionality Tests  //
////////////////////////////////

//  1. Happy Path - Schema Creation
static void setup_schema_creation_db(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_schema_creation.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Schema Creation", TestSchema, schema_creates_database,
                           setup_schema_creation_db, cleanup_test_db) {
    // Should create database and schema without throwing
    faultline_sqlite_init_schema(t->test_db);

    // Verify database file was created
    FILE *f;
    fopen_s(&f, t->test_db, "r");
    FL_ASSERT_NOT_NULL(f);
    fclose(f);
}

//  2. Core Tables Verification
static void setup_core_tables(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_core_tables.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Verify Tables", TestSchema, schema_creates_core_tables,
                           setup_core_tables, cleanup_test_db) {
    sqlite3 *db;

    faultline_sqlite_init_schema(t->test_db);

    // Open database and verify core tables exist
    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READONLY, NULL));

    verify_table_exists(db, "test_suites");
    verify_table_exists(db, "raw_test_runs");
    verify_table_exists(db, "raw_test_summaries");
    verify_table_exists(db, "raw_faults");
    verify_table_exists(db, "schema_info");

    sqlite3_close_v2(db);
}

//  3. Schema Version Tracking
static void setup_version_tracking(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_version.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Version Tracking", TestSchema, schema_sets_version,
                           setup_version_tracking, cleanup_test_db) {
    sqlite3      *db;
    sqlite3_stmt *stmt;
    int           version;

    faultline_sqlite_init_schema(t->test_db);

    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READONLY, NULL));

    // Verify schema version was recorded
    FL_ASSERT_EQ_INT(SQLITE_OK, sqlite3_prepare_v2(db, "SELECT version FROM schema_info",
                                                   -1, &stmt, NULL));
    FL_ASSERT_EQ_INT(SQLITE_ROW, sqlite3_step(stmt));

    version = sqlite3_column_int(stmt, 0);
    FL_ASSERT_EQ_INT(FL_SCHEMA_VERSION, version);

    sqlite3_finalize(stmt);
    sqlite3_close_v2(db);
}

////////////////////////////////////
//  Foreign Key Constraint Tests  //
////////////////////////////////////

//  4. Foreign Key Relationships
static void setup_foreign_keys(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_foreign_keys.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Foreign Keys", TestSchema, schema_creates_foreign_keys,
                           setup_foreign_keys, cleanup_test_db) {
    sqlite3 *db;

    faultline_sqlite_init_schema(t->test_db);

    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READWRITE, NULL));

    // Enable foreign key checking
    sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);

    // Test foreign key constraint - should fail to insert run with invalid suite_id
    int rc = sqlite3_exec(
        db,
        "INSERT INTO raw_test_runs (suite_id, test_cases, tests_run, tests_passed, "
        "tests_passed_with_leaks, tests_failed, setups_failed, cleanups_failed, "
        "total_fault_sites, total_elapsed_time) "
        "VALUES (999, 1, 1, 1, 0, 0, 0, 0, 5, 1.0)",
        NULL, NULL, NULL);

    FL_ASSERT_NEQ_INT(SQLITE_OK, rc); // Should fail due to FK constraint

    sqlite3_close_v2(db);
}

///////////////////////////////////////
//  Analysis Tables and Views Tests  //
///////////////////////////////////////

//  5. Analysis Tables Creation
static void setup_analysis_tables(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_analysis_tables.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Analysis Tables", TestSchema, schema_creates_analysis_tables,
                           setup_analysis_tables, cleanup_test_db) {
    sqlite3 *db;

    faultline_sqlite_init_schema(t->test_db);

    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READONLY, NULL));

    verify_table_exists(db, "test_failures");
    verify_table_exists(db, "test_case_evolution");
    verify_table_exists(db, "fault_hotspots");
    verify_table_exists(db, "daily_suite_metrics");
    verify_table_exists(db, "test_case_baselines");
    verify_table_exists(db, "suite_evolution_summary");

    sqlite3_close_v2(db);
}

//  6. Views Creation
static void setup_views(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_views.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Create Views", TestSchema, schema_creates_views, setup_views,
                           cleanup_test_db) {
    sqlite3 *db;

    faultline_sqlite_init_schema(t->test_db);

    FL_ASSERT_EQ_INT(SQLITE_OK,
                     sqlite3_open_v2(t->test_db, &db, SQLITE_OPEN_READONLY, NULL));

    verify_view_exists(db, "latest_runs");
    verify_view_exists(db, "test_case_failure_summary");
    verify_view_exists(db, "performance_regressions");
    verify_view_exists(db, "suite_trends");

    sqlite3_close_v2(db);
}

////////////////////////////
//  Error Handling Tests  //
////////////////////////////

//  7. Invalid Path Handling
static void setup_invalid_path(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "/invalid/nonexistent/path/test.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Invalid Path", TestSchema, schema_invalid_path_throws,
                           setup_invalid_path, cleanup_test_db) {
    bool throw_catch = false;

    // Test with invalid/inaccessible path
    FL_TRY {
        faultline_sqlite_init_schema(t->test_db);
    }
    FL_CATCH(faultline_db_create_failed) {
        throw_catch = true; // success
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(throw_catch);
}

//  8. Permission Denied Handling
static void setup_permission_denied(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
// Create a read-only directory (platform-specific implementation)
#ifdef _WIN32
    t->test_db = "C:\\Windows\\System32\\test.db"; // Should be read-only
#else
    t->test_db = "/usr/test.db"; // Should be read-only
#endif
}

FL_TYPE_TEST_SETUP_CLEANUP("Permission Denied", TestSchema,
                           schema_permission_denied_throws, setup_permission_denied,
                           cleanup_test_db) {
    bool throw_catch = false;
    FL_TRY {
        faultline_sqlite_init_schema(t->test_db);
    }
    FL_CATCH(faultline_db_create_failed) {
        throw_catch = true; // success
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(throw_catch);
}

//  9. Existing Database Handling
static void setup_existing_database(FLTestCase *btc) {
    TestSchema *t = FL_CONTAINER_OF(btc, TestSchema, tc);
    t->test_db    = "test_existing.db";
}

FL_TYPE_TEST_SETUP_CLEANUP("Existing Database", TestSchema,
                           schema_existing_database_succeeds, setup_existing_database,
                           cleanup_test_db) {
    // Create database first time
    faultline_sqlite_init_schema(t->test_db);

    // Should succeed when called on existing database
    faultline_sqlite_init_schema(t->test_db);
}
