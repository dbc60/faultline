/**
 * @file fnv_tests.c
 * @author Douglas Cuthbertson
 * @brief FNV test suite covering all six variants (32-1024 bit).
 * @version 0.2
 * @date 2026-02-20
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Unity build of all FNV variant source files.  Tests every variant with
 * null-parameter checks, state-error checks, basis derivation, one-shot
 * hashes, incremental hashes, and file hashes.
 *
 * The driver injects an FLExceptionService via fla_set_exception_service().
 * FL_ASSERT macros throw exceptions caught by but_driver.exe.
 */

// Unity build: exception services first, then code under test.
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_* macros
#include <faultline/fl_test.h> // FL_VOID_TEST_SETUP_CLEANUP, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h>  // FL_TRY, FL_CATCH, FL_THROW (resolves to FLA_* in DLL builds)

#include "fl_exception_service.c"  // exception reason constants
#include "fla_exception_service.c" // TLS exception service (app-side)
#include "../third_party/fnv/FNV32.c"                 // code under test
#include "../third_party/fnv/FNV64.c"                 // code under test
#include "../third_party/fnv/FNV128.c"                // code under test
#include "../third_party/fnv/FNV256.c"                // code under test
#include "../third_party/fnv/FNV512.c"                // code under test
#include "../third_party/fnv/FNV1024.c"               // code under test

#include <stdint.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Test data
// ---------------------------------------------------------------------------

#define FNV_N_TEST_STRINGS 4

static char const *g_fnv_test_strings[FNV_N_TEST_STRINGS]
    = {"", "a", "foobar", "Hello!\x01\xFF\xED"};

static char const    g_fnv_err_string[] = "foo";
static uint8_t const g_fnv_err_bytes[3] = {1, 2, 3};

// Temp-file name; populated by setup_test_fnv, cleared by cleanup_test_fnv.
static char g_fnv_temp_name[L_tmpnam_s];

// Scratch buffer large enough for the biggest hash variant (FNV-1024).
static uint8_t g_fnv_hash[FNV1024size];

// ---------------------------------------------------------------------------
// Expected hash values - FNV-32 (little-endian uint32_t)
// ---------------------------------------------------------------------------

// FNV-1a string hashes (string without NUL terminator)
static uint32_t const k_fnv32_s[FNV_N_TEST_STRINGS]
    = {0x811c9dc5, 0xe40c292c, 0xbf9cf968, 0xfd9d3881};

// FNV-1a block hashes (string + NUL terminator)
static uint32_t const k_fnv32_b[FNV_N_TEST_STRINGS]
    = {0x050c5d1f, 0x2b24d044, 0x0c1c9eb8, 0xbf7ff313};

// ---------------------------------------------------------------------------
// Expected hash values - FNV-64 (little-endian uint64_t)
// ---------------------------------------------------------------------------

static uint64_t const k_fnv64_s[FNV_N_TEST_STRINGS]
    = {0xcbf29ce484222325, 0xaf63dc4c8601ec8c, 0x85944171f73967e8, 0xbd51ea7094ee6fa1};

static uint64_t const k_fnv64_b[FNV_N_TEST_STRINGS]
    = {0xaf63bd4c8601b7df, 0x089be207b544f1e4, 0x34531ca7168b8f38, 0xa0a0fe4d1127ae93};

// ---------------------------------------------------------------------------
// Expected hash values - FNV-128 (big-endian byte arrays)
// ---------------------------------------------------------------------------

static uint8_t const k_fnv128_s[FNV_N_TEST_STRINGS][FNV128size]
    = {{0x6c, 0x62, 0x27, 0x2e, 0x07, 0xbb, 0x01, 0x42, 0x62, 0xb8, 0x21, 0x75, 0x62,
        0x95, 0xc5, 0x8d},
       {0xd2, 0x28, 0xcb, 0x69, 0x6f, 0x1a, 0x8c, 0xaf, 0x78, 0x91, 0x2b, 0x70, 0x4e,
        0x4a, 0x89, 0x64},
       {0x34, 0x3e, 0x16, 0x62, 0x79, 0x3c, 0x64, 0xbf, 0x6f, 0x0d, 0x35, 0x97, 0xba,
        0x44, 0x6f, 0x18},
       {0x74, 0x20, 0x2c, 0x60, 0x0b, 0x05, 0x1c, 0x16, 0x5b, 0x1a, 0xca, 0xfe, 0xd1,
        0x0d, 0x14, 0x19}};

static uint8_t const k_fnv128_b[FNV_N_TEST_STRINGS][FNV128size]
    = {{0xd2, 0x28, 0xcb, 0x69, 0x10, 0x1a, 0x8c, 0xaf, 0x78, 0x91, 0x2b, 0x70, 0x4e,
        0x4a, 0x14, 0x7f},
       {0x08, 0x80, 0x95, 0x45, 0x19, 0xab, 0x1b, 0xe9, 0x5a, 0xa0, 0x73, 0x30, 0x55,
        0xb7, 0x0e, 0x0c},
       {0xe0, 0x1f, 0xcf, 0x9a, 0x45, 0x4f, 0xf7, 0x8d, 0xa5, 0x40, 0xf1, 0xb2, 0x32,
        0x34, 0xb2, 0x88},
       {0xe2, 0x67, 0xa7, 0x41, 0xa8, 0x49, 0x8f, 0x82, 0x19, 0xf7, 0xc7, 0x8b, 0x3b,
        0x17, 0xba, 0xc3}};

// ---------------------------------------------------------------------------
// Expected hash values - FNV-256 (big-endian byte arrays)
// ---------------------------------------------------------------------------

static uint8_t const k_fnv256_s[FNV_N_TEST_STRINGS][FNV256size]
    = {{0xdd, 0x26, 0x8d, 0xbc, 0xaa, 0xc5, 0x50, 0x36, 0x2d, 0x98, 0xc3,
        0x84, 0xc4, 0xe5, 0x76, 0xcc, 0xc8, 0xb1, 0x53, 0x68, 0x47, 0xb6,
        0xbb, 0xb3, 0x10, 0x23, 0xb4, 0xc8, 0xca, 0xee, 0x05, 0x35},
       {0x63, 0x32, 0x3f, 0xb0, 0xf3, 0x53, 0x03, 0xec, 0x28, 0xdc, 0x75,
        0x1d, 0x0a, 0x33, 0xbd, 0xfa, 0x4d, 0xe6, 0xa9, 0x9b, 0x72, 0x66,
        0x49, 0x4f, 0x61, 0x83, 0xb2, 0x71, 0x68, 0x11, 0x63, 0x7c},
       {0xb0, 0x55, 0xea, 0x2f, 0x30, 0x6c, 0xad, 0xad, 0x4f, 0x0f, 0x81,
        0xc0, 0x2d, 0x38, 0x89, 0xdc, 0x32, 0x45, 0x3d, 0xad, 0x5a, 0xe3,
        0x5b, 0x75, 0x3b, 0xa1, 0xa9, 0x10, 0x84, 0xaf, 0x34, 0x28},
       {0x0c, 0x5a, 0x44, 0x40, 0x2c, 0x65, 0x38, 0xcf, 0x98, 0xef, 0x20,
        0xc4, 0x03, 0xa8, 0x0f, 0x65, 0x9b, 0x80, 0xc9, 0xa5, 0xb0, 0x1a,
        0x6a, 0x87, 0x34, 0x2e, 0x26, 0x72, 0x64, 0x45, 0x67, 0xb1}};

static uint8_t const k_fnv256_b[FNV_N_TEST_STRINGS][FNV256size]
    = {{0x63, 0x32, 0x3f, 0xb0, 0xf3, 0x53, 0x03, 0xec, 0x28, 0xdc, 0x56,
        0x1d, 0x0a, 0x33, 0xbd, 0xfa, 0x4d, 0xe6, 0xa9, 0x9b, 0x72, 0x66,
        0x49, 0x4f, 0x61, 0x83, 0xb2, 0x71, 0x68, 0x11, 0x38, 0x7f},
       {0xf4, 0xf7, 0xa1, 0xc2, 0xef, 0xd0, 0xe1, 0xe4, 0xbb, 0x19, 0xe3,
        0x45, 0x25, 0xc0, 0x72, 0x1a, 0x06, 0xdd, 0x32, 0x8f, 0xa3, 0xd7,
        0xa9, 0x14, 0x39, 0xa0, 0x73, 0x43, 0x50, 0x1c, 0xf4, 0xf4},
       {0x6a, 0x7f, 0x34, 0xab, 0xc8, 0x5d, 0xe7, 0xd9, 0x51, 0xb5, 0x15,
        0x7e, 0xb5, 0x67, 0x2c, 0x59, 0xb6, 0x04, 0x87, 0x65, 0x09, 0x47,
        0xd3, 0x91, 0xb1, 0x2d, 0x71, 0xe7, 0xfe, 0xf5, 0x53, 0x78},
       {0x3b, 0x97, 0x2c, 0x31, 0xbe, 0x84, 0x3a, 0x45, 0x59, 0x02, 0x20,
        0xd1, 0x12, 0x0d, 0x59, 0xe6, 0xa3, 0x97, 0xa0, 0xc3, 0x34, 0xa1,
        0xb9, 0x7d, 0x5b, 0xff, 0x50, 0xa1, 0x0c, 0x3e, 0xca, 0x73}};

// ---------------------------------------------------------------------------
// Expected hash values - FNV-512 (big-endian byte arrays)
// ---------------------------------------------------------------------------

static uint8_t const k_fnv512_s[FNV_N_TEST_STRINGS][FNV512size]
    = {{0xb8, 0x6d, 0xb0, 0xb1, 0x17, 0x1f, 0x44, 0x16, 0xdc, 0xa1, 0xe5, 0x0f, 0x30,
        0x99, 0x90, 0xac, 0xac, 0x87, 0xd0, 0x59, 0xc9, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0d, 0x21, 0xe9, 0x48, 0xf6, 0x8a, 0x34, 0xc1, 0x92,
        0xf6, 0x2e, 0xa7, 0x9b, 0xc9, 0x42, 0xdb, 0xe7, 0xce, 0x18, 0x20, 0x36, 0x41,
        0x5f, 0x56, 0xe3, 0x4b, 0xac, 0x98, 0x2a, 0xac, 0x4a, 0xfe, 0x9f, 0xd9},
       {0xe4, 0x3a, 0x99, 0x2d, 0xc8, 0xfc, 0x5a, 0xd7, 0xde, 0x49, 0x3e, 0x3d, 0x69,
        0x6d, 0x6f, 0x85, 0xd6, 0x43, 0x26, 0xec, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x11, 0x98, 0x6f, 0x90, 0xc2, 0x53, 0x2c, 0xaf, 0x5b, 0xe7,
        0xd8, 0x82, 0x91, 0xba, 0xa8, 0x94, 0xa3, 0x95, 0x22, 0x53, 0x28, 0xb1, 0x96,
        0xbd, 0x6a, 0x8a, 0x64, 0x3f, 0xe1, 0x2c, 0xd8, 0x7b, 0x27, 0xff, 0x88},
       {0xb0, 0xec, 0x73, 0x8d, 0x9c, 0x6f, 0xd9, 0x69, 0xd0, 0x5f, 0x0b, 0x35, 0xf6,
        0xc0, 0xed, 0x53, 0xad, 0xca, 0xcc, 0xcd, 0x8e, 0x00, 0x00, 0x00, 0x4b, 0xf9,
        0x9f, 0x58, 0xee, 0x41, 0x96, 0xaf, 0xb9, 0x70, 0x0e, 0x20, 0x11, 0x08, 0x30,
        0xfe, 0xa5, 0x39, 0x6b, 0x76, 0x28, 0x0e, 0x47, 0xfd, 0x02, 0x2b, 0x6e, 0x81,
        0x33, 0x1c, 0xa1, 0xa9, 0xce, 0xd7, 0x29, 0xc3, 0x64, 0xbe, 0x77, 0x88},
       {0x4f, 0xdf, 0x00, 0xec, 0xb9, 0xbc, 0x04, 0xdd, 0x19, 0x38, 0x61, 0x8f, 0xe5,
        0xc4, 0xfb, 0xb8, 0x80, 0xa8, 0x2b, 0x15, 0xf5, 0xb6, 0xbd, 0x72, 0x1e, 0xc2,
        0xea, 0xfe, 0x03, 0xc4, 0x62, 0x48, 0xf7, 0xa6, 0xc2, 0x47, 0x89, 0x92, 0x80,
        0xd6, 0xd2, 0xf4, 0x2f, 0xf6, 0xb4, 0x7b, 0xf2, 0x20, 0x79, 0xdf, 0xd4, 0xbf,
        0xe8, 0x7b, 0xf0, 0xbb, 0x4e, 0x71, 0xea, 0xcb, 0x1e, 0x28, 0x77, 0x35}};

static uint8_t const k_fnv512_b[FNV_N_TEST_STRINGS][FNV512size]
    = {{0xe4, 0x3a, 0x99, 0x2d, 0xc8, 0xfc, 0x5a, 0xd7, 0xde, 0x49, 0x3e, 0x3d, 0x69,
        0x6d, 0x6f, 0x85, 0xd6, 0x43, 0x26, 0xec, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x11, 0x98, 0x6f, 0x90, 0xc2, 0x53, 0x2c, 0xaf, 0x5b, 0xe7,
        0xd8, 0x82, 0x91, 0xba, 0xa8, 0x94, 0xa3, 0x95, 0x22, 0x53, 0x28, 0xb1, 0x96,
        0xbd, 0x6a, 0x8a, 0x64, 0x3f, 0xe1, 0x2c, 0xd8, 0x7b, 0x28, 0x2b, 0xbf},
       {0x73, 0x17, 0xdf, 0xed, 0x6c, 0x70, 0xdf, 0xec, 0x6a, 0xdf, 0xce, 0xd2, 0xa5,
        0xe0, 0x4d, 0x7e, 0xec, 0x74, 0x4e, 0x3c, 0xe9, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x17, 0x93, 0x3d, 0x7a, 0xf4, 0x5d, 0x70, 0xde, 0xf4, 0x23, 0xa3,
        0x16, 0xf1, 0x41, 0x17, 0xdf, 0x27, 0x2c, 0xd0, 0xfd, 0x6b, 0x85, 0xf0, 0xf7,
        0xc9, 0xbf, 0x6c, 0x51, 0x96, 0xb3, 0x16, 0x0d, 0x02, 0x97, 0x5f, 0x38},
       {0x82, 0xf6, 0xe1, 0x04, 0x96, 0xde, 0x78, 0x34, 0xb0, 0x8b, 0x21, 0xef, 0x46,
        0x4c, 0xd2, 0x47, 0x9e, 0x1d, 0x25, 0xe0, 0xca, 0x00, 0x00, 0x65, 0xcb, 0x74,
        0x80, 0x27, 0x39, 0xe0, 0xe5, 0x71, 0x75, 0x22, 0xec, 0xf6, 0xd1, 0xf9, 0xa5,
        0x2f, 0x5f, 0xee, 0xfb, 0x4f, 0xab, 0x22, 0x73, 0xfd, 0xe8, 0x31, 0x0f, 0x1b,
        0x7b, 0x5c, 0x9a, 0x84, 0x22, 0x48, 0xf4, 0xcb, 0xfb, 0x32, 0x27, 0x38},
       {0xfa, 0x7e, 0xb9, 0x1e, 0xfb, 0x64, 0x64, 0x11, 0x8a, 0x73, 0x33, 0xbd, 0x96,
        0x3b, 0xb6, 0x1f, 0x2c, 0x6f, 0xe2, 0xe3, 0x6c, 0xd7, 0xd3, 0xe7, 0x37, 0x28,
        0xda, 0x57, 0x0c, 0x1f, 0xaf, 0xc3, 0xd0, 0x6e, 0x4d, 0xd9, 0x53, 0x4a, 0x9f,
        0xd4, 0xa5, 0x2c, 0x43, 0x8b, 0xd2, 0x11, 0x69, 0x83, 0x4a, 0xe6, 0x0d, 0x20,
        0x7e, 0x0f, 0x8a, 0xf6, 0x1a, 0xa1, 0x96, 0x25, 0x68, 0x37, 0xb8, 0x03}};

// ---------------------------------------------------------------------------
// Expected hash values - FNV-1024 (big-endian byte arrays)
// ---------------------------------------------------------------------------

static uint8_t const k_fnv1024_s[FNV_N_TEST_STRINGS][FNV1024size]
    = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5f, 0x7a, 0x76, 0x75,
        0x8e, 0xcc, 0x4d, 0x32, 0xe5, 0x6d, 0x5a, 0x59, 0x10, 0x28, 0xb7, 0x4b, 0x29,
        0xfc, 0x42, 0x23, 0xfd, 0xad, 0xa1, 0x6c, 0x3b, 0xf3, 0x4e, 0xda, 0x36, 0x74,
        0xda, 0x9a, 0x21, 0xd9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x04, 0xc6, 0xd7, 0xeb, 0x6e, 0x73, 0x80, 0x27, 0x34, 0x51, 0x0a,
        0x55, 0x5f, 0x25, 0x6c, 0xc0, 0x05, 0xae, 0x55, 0x6b, 0xde, 0x8c, 0xc9, 0xc6,
        0xa9, 0x3b, 0x21, 0xaf, 0xf4, 0xb1, 0x6c, 0x71, 0xee, 0x90, 0xb3},
       {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0xd7, 0xc1, 0x9f, 0xbc,
        0xe6, 0x53, 0xdf, 0x22, 0x1b, 0x9f, 0x71, 0x7d, 0x34, 0x90, 0xff, 0x95, 0xca,
        0x87, 0xfd, 0xae, 0xf3, 0x0d, 0x1b, 0x82, 0x33, 0x72, 0xf8, 0x5b, 0x24, 0xa3,
        0x72, 0xf5, 0x0e, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x07, 0x68, 0x5c, 0xd8, 0x1a, 0x49, 0x1d, 0xbc, 0xcc, 0x21, 0xad, 0x06,
        0x64, 0x8d, 0x09, 0xa5, 0xc8, 0xcf, 0x5a, 0x78, 0x48, 0x20, 0x54, 0xe9, 0x14,
        0x70, 0xb3, 0x3d, 0xde, 0x77, 0x25, 0x2c, 0xae, 0xf6, 0x95, 0xaa},
       {0x00, 0x00, 0x06, 0x31, 0x17, 0x5f, 0xa7, 0xae, 0x64, 0x3a, 0xd0, 0x87, 0x23,
        0xd3, 0x12, 0xc9, 0xfd, 0x02, 0x4a, 0xdb, 0x91, 0xf7, 0x7f, 0x6b, 0x19, 0x58,
        0x71, 0x97, 0xa2, 0x2b, 0xcd, 0xf2, 0x37, 0x27, 0x16, 0x6c, 0x45, 0x72, 0xd0,
        0xb9, 0x85, 0xd5, 0xae, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x70, 0xd1, 0x1e,
        0xf4, 0x18, 0xef, 0x08, 0xb8, 0xa4, 0x9e, 0x1e, 0x82, 0x5e, 0x54, 0x7e, 0xb3,
        0x99, 0x37, 0xf8, 0x19, 0x22, 0x2f, 0x3b, 0x7f, 0xc9, 0x2a, 0x0e, 0x47, 0x07,
        0x90, 0x08, 0x88, 0x84, 0x7a, 0x55, 0x4b, 0xac, 0xec, 0x98, 0xb0},
       {0xf6, 0xf7, 0x47, 0xaf, 0x25, 0xa9, 0xde, 0x26, 0xe8, 0xa4, 0x93, 0x43, 0x1e,
        0x31, 0xb4, 0xa1, 0xed, 0x2a, 0x92, 0x30, 0x4a, 0xf6, 0xca, 0x97, 0x6b, 0xc1,
        0xd9, 0x6f, 0xfc, 0xad, 0x35, 0x24, 0x4e, 0x8d, 0x38, 0x5d, 0x55, 0xf4, 0x2f,
        0xdc, 0xc8, 0xf2, 0x99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7, 0xca, 0x87, 0xce, 0x43, 0x22, 0x7b,
        0x98, 0xc1, 0x44, 0x60, 0x7e, 0x67, 0xcc, 0x50, 0xaf, 0x99, 0xbc, 0xc5, 0xd1,
        0x51, 0x4b, 0xb0, 0xd9, 0x23, 0xee, 0xde, 0xdd, 0x69, 0xe8, 0xe7, 0x47, 0x02,
        0x05, 0x08, 0x3a, 0x0c, 0x02, 0x27, 0xd0, 0xcc, 0x69, 0xde, 0x23}};

static uint8_t const k_fnv1024_b[FNV_N_TEST_STRINGS][FNV1024size]
    = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0xd7, 0xc1, 0x9f, 0xbc,
        0xe6, 0x53, 0xdf, 0x22, 0x1b, 0x9f, 0x71, 0x7d, 0x34, 0x90, 0xff, 0x95, 0xca,
        0x87, 0xfd, 0xae, 0xf3, 0x0d, 0x1b, 0x82, 0x33, 0x72, 0xf8, 0x5b, 0x24, 0xa3,
        0x72, 0xf5, 0x0e, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x07, 0x68, 0x5c, 0xd8, 0x1a, 0x49, 0x1d, 0xbc, 0xcc, 0x21, 0xad, 0x06,
        0x64, 0x8d, 0x09, 0xa5, 0xc8, 0xcf, 0x5a, 0x78, 0x48, 0x20, 0x54, 0xe9, 0x14,
        0x70, 0xb3, 0x3d, 0xde, 0x77, 0x25, 0x2c, 0xae, 0xf6, 0x65, 0x97},
       {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf4, 0x6e, 0xf4, 0x1c, 0xd2, 0x3a,
        0x4d, 0xcd, 0xd4, 0x06, 0x83, 0x49, 0x63, 0xb7, 0x8e, 0x82, 0x24, 0x1a, 0x6f,
        0x5c, 0xb0, 0x6f, 0x40, 0x3c, 0xbd, 0x5a, 0x7c, 0x89, 0x03, 0xce, 0xf6, 0xa5,
        0xf4, 0xfd, 0xd2, 0x95, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0b, 0x7c, 0xd7, 0xfb, 0x20, 0xc3, 0x63, 0x1d, 0xc8, 0x90, 0x39, 0x52, 0xe9,
        0xee, 0xb7, 0xf6, 0x18, 0x69, 0x8f, 0x4c, 0x87, 0xda, 0x23, 0xad, 0x74, 0xb2,
        0xc5, 0xf6, 0xf1, 0xfe, 0xc4, 0xa6, 0x4b, 0x54, 0x66, 0x18, 0xa2},
       {0x00, 0x09, 0xdc, 0x92, 0x10, 0x75, 0xfd, 0x8a, 0x5e, 0x3e, 0x1a, 0x37, 0x2c,
        0x72, 0xa5, 0x9b, 0xb1, 0x0c, 0xca, 0x1a, 0x94, 0xc8, 0xb2, 0x38, 0x7d, 0x63,
        0xa7, 0xef, 0xa7, 0xfc, 0xa7, 0xa7, 0x17, 0xa6, 0x4e, 0x6c, 0x2d, 0x62, 0xfb,
        0x61, 0x78, 0xf7, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x08, 0xf4, 0x4d, 0x00,
        0x8a, 0xaa, 0xb0, 0x86, 0x57, 0x49, 0x35, 0x50, 0x2c, 0x49, 0x08, 0x7c, 0x84,
        0x9b, 0xcb, 0xbe, 0xfa, 0x03, 0x3f, 0x45, 0x2a, 0xf6, 0x38, 0x24, 0x26, 0xba,
        0x5d, 0x3b, 0xb5, 0x71, 0xb6, 0x46, 0x5b, 0x2a, 0xe8, 0xc8, 0xf0},
       {0xc8, 0x01, 0xf8, 0xe0, 0x8a, 0xe9, 0x1b, 0x18, 0x0b, 0x98, 0xdd, 0x7d, 0x9f,
        0x65, 0xce, 0xb6, 0x87, 0xca, 0x86, 0x35, 0x8c, 0x69, 0x05, 0xf6, 0x0a, 0x7d,
        0x10, 0x14, 0xc1, 0x82, 0xb0, 0x4f, 0xd6, 0x08, 0xa2, 0xca, 0x4d, 0xd6, 0x0a,
        0x30, 0x0a, 0x15, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x45, 0x14, 0x9a, 0xde, 0x1c, 0x79, 0xab,
        0xe3, 0xb7, 0x09, 0xa4, 0x06, 0xf7, 0xd9, 0x20, 0x51, 0x69, 0xbe, 0xc5, 0x9b,
        0x12, 0x61, 0x40, 0xbc, 0xb9, 0x6f, 0x9d, 0x5d, 0x3e, 0x2e, 0xa9, 0x1e, 0x21,
        0xcd, 0xc2, 0x04, 0x9f, 0x57, 0xbe, 0xcd, 0x00, 0x2d, 0x7c, 0x47}};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * @brief Write @p len bytes of @p str (no NUL) to the test temp file in binary
 *        mode and return its name.  Throws on I/O failure.
 */
static char const *write_temp(char const *str, long int len) {
    FILE   *fp  = NULL;
    errno_t err = fopen_s(&fp, g_fnv_temp_name, "wb");
    FL_ASSERT_EQ_INT(0, (int)err);
    FL_ASSERT_NOT_NULL(fp);
    long int written = (long int)fwrite(str, 1, (size_t)len, fp);
    fclose(fp);
    FL_ASSERT_EQ_INT32(len, written);
    return g_fnv_temp_name;
}

// ---------------------------------------------------------------------------
// Shared setup / cleanup (all cases need a temp file)
// ---------------------------------------------------------------------------

typedef struct {
    FLTestCase tc;
} TestFnv;

static void setup_test_fnv(FLTestCase *tc) {
    FL_UNUSED(tc);
    errno_t err = tmpnam_s(g_fnv_temp_name, sizeof g_fnv_temp_name);
    FL_ASSERT_EQ_INT(0, (int)err);
}

static void cleanup_test_fnv(FLTestCase *tc) {
    FL_UNUSED(tc);
    remove(g_fnv_temp_name);
    memset(g_fnv_temp_name, 0, sizeof g_fnv_temp_name);
}

// ---------------------------------------------------------------------------
// FNV-32 test case
// ---------------------------------------------------------------------------

// clang-format off
FL_VOID_TEST_SETUP_CLEANUP("case32", TestFnv, test_fnv32, setup_test_fnv, cleanup_test_fnv) {
    // clang-format on
    FNV32context ctx;
    uint32_t     u32;
    uint32_t     basis_t = FNV32basis;

    // --- Null-parameter checks: stateless one-shot byte-vector functions ---
    FL_ASSERT_EQ_INT(fnvNull, FNV32string(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV32string(g_fnv_err_string, NULL));
    FL_ASSERT_EQ_INT(fnvNull,
                     FNV32stringBasis(NULL, g_fnv_hash, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV32stringBasis(g_fnv_err_string, NULL,
                                               (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV32stringBasis(g_fnv_err_string, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV32block(NULL, 1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV32block(g_fnv_err_bytes, -1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV32block(g_fnv_err_bytes, 1, NULL));
    FL_ASSERT_EQ_INT(fnvNull,
                     FNV32blockBasis(NULL, 1, g_fnv_hash, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV32blockBasis(g_fnv_err_bytes, -1, g_fnv_hash,
                                                  (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV32blockBasis(g_fnv_err_bytes, 1, NULL,
                                              (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV32blockBasis(g_fnv_err_bytes, 1, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV32file(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV32file("foo.txt", NULL));
    FL_ASSERT_EQ_INT(fnvNull,
                     FNV32fileBasis(NULL, g_fnv_hash, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull,
                     FNV32fileBasis("foo.txt", NULL, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV32fileBasis("foo.txt", g_fnv_hash, NULL));

    // --- Null-parameter checks: context-based init functions ---
    FL_ASSERT_EQ_INT(fnvSuccess, FNV32init(&ctx));
    FL_ASSERT_EQ_INT(fnvNull, FNV32init(NULL));
    FL_ASSERT_EQ_INT(fnvSuccess, FNV32initBasis(&ctx, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV32initBasis(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV32initBasis(&ctx, NULL));

    // --- Null-parameter checks: INT (integer-return) functions ---
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTstring(NULL, &u32));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTstring(g_fnv_err_string, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTstringBasis(NULL, &u32, FNV32basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTstringBasis(g_fnv_err_string, NULL, FNV32basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTblock(NULL, 1, &u32));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV32INTblock(g_fnv_err_bytes, -1, &u32));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTblock(g_fnv_err_bytes, 1, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTblockBasis(NULL, 1, &u32, FNV32basis));
    FL_ASSERT_EQ_INT(fnvBadParam,
                     FNV32INTblockBasis(g_fnv_err_bytes, -1, &u32, FNV32basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTblockBasis(g_fnv_err_bytes, 1, NULL, FNV32basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTinitBasis(NULL, FNV32basis));

    // --- State-error checks: put ctx into a known-bad (FNVclobber) state ---
    ctx.Computed = FNVclobber + FNV32state;
    FL_ASSERT_EQ_INT(fnvNull, FNV32blockin(NULL, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV32blockin(&ctx, NULL, 3));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV32blockin(&ctx, g_fnv_err_bytes, -1));
    FL_ASSERT_EQ_INT(fnvStateError, FNV32blockin(&ctx, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV32stringin(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV32stringin(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV32stringin(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV32filein(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV32filein(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV32filein(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV32result(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV32result(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV32result(&ctx, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTresult(NULL, &u32));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTresult(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV32INTresult(&ctx, &u32));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTfile(NULL, &u32));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTfile("foo.txt", NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTfileBasis(NULL, &u32, FNV32basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV32INTfileBasis("foo.txt", NULL, FNV32basis));

    // --- FNV-0 basis derivation (little-endian: XOR byte [0]) ---
    {
        char const basis_string[] = "chongo <Landon Curt Noll> /\\../";
        uint8_t    zero_basis[FNV32size];
        memset(zero_basis, 0, sizeof zero_basis);

        // Byte-vector variant
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV32stringBasis(basis_string, g_fnv_hash, zero_basis));
        g_fnv_hash[0] ^= '\\';
        FL_ASSERT_MEM_EQ(&k_fnv32_s[0], g_fnv_hash, FNV32size);

        // Integer variant
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV32INTstringBasis(basis_string, &u32, (uint32_t)0));
        u32 ^= (uint32_t)'\\';
        FL_ASSERT_TRUE(u32 == k_fnv32_s[0]);
    }

    // --- Hash correctness ---
    for (int i = 0; i < FNV_N_TEST_STRINGS; ++i) {
        long int len = (long int)strlen(g_fnv_test_strings[i]);

        // INT one-shot string (no NUL)
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32INTstring(g_fnv_test_strings[i], &u32));
        FL_ASSERT_TRUE(u32 == k_fnv32_s[i]);

        // INT one-shot block (string + NUL)
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV32INTblock(g_fnv_test_strings[i],
                                       (long int)(strlen(g_fnv_test_strings[i]) + 1),
                                       &u32));
        FL_ASSERT_TRUE(u32 == k_fnv32_b[i]);

        // Byte one-shot string (no NUL)
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32string(g_fnv_test_strings[i], g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv32_s[i], g_fnv_hash, FNV32size);

        // Byte one-shot block (string + NUL)
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV32block(g_fnv_test_strings[i],
                                    (long int)(strlen(g_fnv_test_strings[i]) + 1),
                                    g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv32_b[i], g_fnv_hash, FNV32size);

        // Incremental byte: split string at midpoint
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32init(&ctx));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32blockin(&ctx, g_fnv_test_strings[i], len / 2));
        if (i & 1) {
            FL_ASSERT_EQ_INT(fnvSuccess, FNV32result(&ctx, g_fnv_hash));
            FL_ASSERT_EQ_INT(fnvSuccess, FNV32initBasis(&ctx, g_fnv_hash));
        }
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV32stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32result(&ctx, g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv32_s[i], g_fnv_hash, FNV32size);

        // Incremental INT
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32init(&ctx));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32blockin(&ctx, g_fnv_test_strings[i], len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV32stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32INTresult(&ctx, &u32));
        FL_ASSERT_TRUE(u32 == k_fnv32_s[i]);

        // Incremental with custom basis (same value as the standard FNV32 basis)
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32initBasis(&ctx, (uint8_t const *)&basis_t));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32blockin(&ctx, g_fnv_test_strings[i], len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV32stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV32result(&ctx, g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv32_s[i], g_fnv_hash, FNV32size);

        // File hash (INT)
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV32INTfile(write_temp(g_fnv_test_strings[i], len), &u32));
        FL_ASSERT_TRUE(u32 == k_fnv32_s[i]);

        // File hash (byte)
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV32file(write_temp(g_fnv_test_strings[i], len), g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv32_s[i], g_fnv_hash, FNV32size);
    }
}

// ---------------------------------------------------------------------------
// FNV-64 test case
// ---------------------------------------------------------------------------

// clang-format off
FL_VOID_TEST_SETUP_CLEANUP("case64", TestFnv, test_fnv64, setup_test_fnv, cleanup_test_fnv) {
    // clang-format on
    FNV64context ctx;
    uint64_t     u64;
    uint64_t     basis_t = FNV64basis;

    // --- Null-parameter checks: stateless one-shot byte-vector functions ---
    FL_ASSERT_EQ_INT(fnvNull, FNV64string(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV64string(g_fnv_err_string, NULL));
    FL_ASSERT_EQ_INT(fnvNull,
                     FNV64stringBasis(NULL, g_fnv_hash, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV64stringBasis(g_fnv_err_string, NULL,
                                               (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV64stringBasis(g_fnv_err_string, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV64block(NULL, 1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV64block(g_fnv_err_bytes, -1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV64block(g_fnv_err_bytes, 1, NULL));
    FL_ASSERT_EQ_INT(fnvNull,
                     FNV64blockBasis(NULL, 1, g_fnv_hash, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV64blockBasis(g_fnv_err_bytes, -1, g_fnv_hash,
                                                  (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV64blockBasis(g_fnv_err_bytes, 1, NULL,
                                              (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV64blockBasis(g_fnv_err_bytes, 1, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV64file(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV64file("foo.txt", NULL));
    FL_ASSERT_EQ_INT(fnvNull,
                     FNV64fileBasis(NULL, g_fnv_hash, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull,
                     FNV64fileBasis("foo.txt", NULL, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV64fileBasis("foo.txt", g_fnv_hash, NULL));

    // --- Null-parameter checks: context-based init functions ---
    FL_ASSERT_EQ_INT(fnvSuccess, FNV64init(&ctx));
    FL_ASSERT_EQ_INT(fnvNull, FNV64init(NULL));
    FL_ASSERT_EQ_INT(fnvSuccess, FNV64initBasis(&ctx, (uint8_t const *)&basis_t));
    FL_ASSERT_EQ_INT(fnvNull, FNV64initBasis(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV64initBasis(&ctx, NULL));

    // --- Null-parameter checks: INT (integer-return) functions ---
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTstring(NULL, &u64));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTstring(g_fnv_err_string, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTstringBasis(NULL, &u64, FNV64basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTstringBasis(g_fnv_err_string, NULL, FNV64basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTblock(NULL, 1, &u64));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV64INTblock(g_fnv_err_bytes, -1, &u64));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTblock(g_fnv_err_bytes, 1, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTblockBasis(NULL, 1, &u64, FNV64basis));
    FL_ASSERT_EQ_INT(fnvBadParam,
                     FNV64INTblockBasis(g_fnv_err_bytes, -1, &u64, FNV64basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTblockBasis(g_fnv_err_bytes, 1, NULL, FNV64basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTinitBasis(NULL, FNV64basis));

    // --- State-error checks: put ctx into a known-bad (FNVclobber) state ---
    ctx.Computed = FNVclobber + FNV64state;
    FL_ASSERT_EQ_INT(fnvNull, FNV64blockin(NULL, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV64blockin(&ctx, NULL, 3));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV64blockin(&ctx, g_fnv_err_bytes, -1));
    FL_ASSERT_EQ_INT(fnvStateError, FNV64blockin(&ctx, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV64stringin(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV64stringin(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV64stringin(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV64filein(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV64filein(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV64filein(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV64result(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV64result(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV64result(&ctx, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTresult(NULL, &u64));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTresult(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV64INTresult(&ctx, &u64));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTfile(NULL, &u64));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTfile("foo.txt", NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTfileBasis(NULL, &u64, FNV64basis));
    FL_ASSERT_EQ_INT(fnvNull, FNV64INTfileBasis("foo.txt", NULL, FNV64basis));

    // --- FNV-0 basis derivation (little-endian: XOR byte [0]) ---
    {
        char const basis_string[] = "chongo <Landon Curt Noll> /\\../";
        uint8_t    zero_basis[FNV64size];
        memset(zero_basis, 0, sizeof zero_basis);

        // Byte-vector variant
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV64stringBasis(basis_string, g_fnv_hash, zero_basis));
        g_fnv_hash[0] ^= '\\';
        FL_ASSERT_MEM_EQ(&k_fnv64_s[0], g_fnv_hash, FNV64size);

        // Integer variant
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV64INTstringBasis(basis_string, &u64, (uint64_t)0));
        u64 ^= (uint64_t)'\\';
        FL_ASSERT_TRUE(u64 == k_fnv64_s[0]);
    }

    // --- Hash correctness ---
    for (int i = 0; i < FNV_N_TEST_STRINGS; ++i) {
        long int len = (long int)strlen(g_fnv_test_strings[i]);

        // INT one-shot string (no NUL)
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64INTstring(g_fnv_test_strings[i], &u64));
        FL_ASSERT_TRUE(u64 == k_fnv64_s[i]);

        // INT one-shot block (string + NUL)
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV64INTblock(g_fnv_test_strings[i],
                                       (long int)(strlen(g_fnv_test_strings[i]) + 1),
                                       &u64));
        FL_ASSERT_TRUE(u64 == k_fnv64_b[i]);

        // Byte one-shot string (no NUL)
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64string(g_fnv_test_strings[i], g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv64_s[i], g_fnv_hash, FNV64size);

        // Byte one-shot block (string + NUL)
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV64block(g_fnv_test_strings[i],
                                    (long int)(strlen(g_fnv_test_strings[i]) + 1),
                                    g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv64_b[i], g_fnv_hash, FNV64size);

        // Incremental byte: split string at midpoint
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64init(&ctx));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64blockin(&ctx, g_fnv_test_strings[i], len / 2));
        if (i & 1) {
            FL_ASSERT_EQ_INT(fnvSuccess, FNV64result(&ctx, g_fnv_hash));
            FL_ASSERT_EQ_INT(fnvSuccess, FNV64initBasis(&ctx, g_fnv_hash));
        }
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV64stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64result(&ctx, g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv64_s[i], g_fnv_hash, FNV64size);

        // Incremental INT
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64init(&ctx));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64blockin(&ctx, g_fnv_test_strings[i], len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV64stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64INTresult(&ctx, &u64));
        FL_ASSERT_TRUE(u64 == k_fnv64_s[i]);

        // Incremental with custom basis (same value as the standard FNV64 basis)
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64initBasis(&ctx, (uint8_t const *)&basis_t));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64blockin(&ctx, g_fnv_test_strings[i], len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV64stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV64result(&ctx, g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv64_s[i], g_fnv_hash, FNV64size);

        // File hash (INT)
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV64INTfile(write_temp(g_fnv_test_strings[i], len), &u64));
        FL_ASSERT_TRUE(u64 == k_fnv64_s[i]);

        // File hash (byte)
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV64file(write_temp(g_fnv_test_strings[i], len), g_fnv_hash));
        FL_ASSERT_MEM_EQ(&k_fnv64_s[i], g_fnv_hash, FNV64size);
    }
}

// ---------------------------------------------------------------------------
// FNV-128 test case (byte-array only, big-endian: XOR last byte for basis)
// ---------------------------------------------------------------------------

// clang-format off
FL_VOID_TEST_SETUP_CLEANUP("case128", TestFnv, test_fnv128, setup_test_fnv, cleanup_test_fnv) {
    // clang-format on
    FNV128context ctx;

    // --- Null-parameter checks ---
    FL_ASSERT_EQ_INT(fnvNull, FNV128string(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128string(g_fnv_err_string, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV128stringBasis(NULL, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128stringBasis(g_fnv_err_string, NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128stringBasis(g_fnv_err_string, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV128block(NULL, 1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV128block(g_fnv_err_bytes, -1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128block(g_fnv_err_bytes, 1, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV128blockBasis(NULL, 1, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam,
                     FNV128blockBasis(g_fnv_err_bytes, -1, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128blockBasis(g_fnv_err_bytes, 1, NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128blockBasis(g_fnv_err_bytes, 1, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV128file(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128file("foo.txt", NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV128fileBasis(NULL, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128fileBasis("foo.txt", NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128fileBasis("foo.txt", g_fnv_hash, NULL));

    // --- Context init checks ---
    FL_ASSERT_EQ_INT(fnvSuccess, FNV128init(&ctx));
    FL_ASSERT_EQ_INT(fnvNull, FNV128init(NULL));
    FL_ASSERT_EQ_INT(fnvSuccess, FNV128initBasis(&ctx, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128initBasis(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128initBasis(&ctx, NULL));

    // --- State-error checks ---
    ctx.Computed = FNVclobber + FNV128state;
    FL_ASSERT_EQ_INT(fnvNull, FNV128blockin(NULL, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV128blockin(&ctx, NULL, 3));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV128blockin(&ctx, g_fnv_err_bytes, -1));
    FL_ASSERT_EQ_INT(fnvStateError, FNV128blockin(&ctx, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV128stringin(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV128stringin(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV128stringin(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV128filein(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV128filein(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV128filein(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV128result(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV128result(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV128result(&ctx, g_fnv_hash));

    // --- Basis derivation (big-endian: XOR last byte) ---
    {
        char const basis_string[] = "chongo <Landon Curt Noll> /\\../";
        uint8_t    zero_basis[FNV128size];
        memset(zero_basis, 0, sizeof zero_basis);
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV128stringBasis(basis_string, g_fnv_hash, zero_basis));
        g_fnv_hash[FNV128size - 1] ^= '\\';
        FL_ASSERT_MEM_EQ(k_fnv128_s[0], g_fnv_hash, FNV128size);
    }

    // --- Hash correctness ---
    for (int i = 0; i < FNV_N_TEST_STRINGS; ++i) {
        long int len = (long int)strlen(g_fnv_test_strings[i]);

        FL_ASSERT_EQ_INT(fnvSuccess, FNV128string(g_fnv_test_strings[i], g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv128_s[i], g_fnv_hash, FNV128size);

        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV128block(g_fnv_test_strings[i],
                                     (long int)(strlen(g_fnv_test_strings[i]) + 1),
                                     g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv128_b[i], g_fnv_hash, FNV128size);

        // Incremental
        FL_ASSERT_EQ_INT(fnvSuccess, FNV128init(&ctx));
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV128blockin(&ctx, g_fnv_test_strings[i], len / 2));
        if (i & 1) {
            FL_ASSERT_EQ_INT(fnvSuccess, FNV128result(&ctx, g_fnv_hash));
            FL_ASSERT_EQ_INT(fnvSuccess, FNV128initBasis(&ctx, g_fnv_hash));
        }
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV128stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV128result(&ctx, g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv128_s[i], g_fnv_hash, FNV128size);

        // File hash
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV128file(write_temp(g_fnv_test_strings[i], len), g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv128_s[i], g_fnv_hash, FNV128size);
    }
}

// ---------------------------------------------------------------------------
// FNV-256 test case (byte-array only, big-endian)
// ---------------------------------------------------------------------------

// clang-format off
FL_VOID_TEST_SETUP_CLEANUP("case256", TestFnv, test_fnv256, setup_test_fnv, cleanup_test_fnv) {
    // clang-format on
    FNV256context ctx;

    // --- Null-parameter checks ---
    FL_ASSERT_EQ_INT(fnvNull, FNV256string(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256string(g_fnv_err_string, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV256stringBasis(NULL, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256stringBasis(g_fnv_err_string, NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256stringBasis(g_fnv_err_string, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV256block(NULL, 1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV256block(g_fnv_err_bytes, -1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256block(g_fnv_err_bytes, 1, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV256blockBasis(NULL, 1, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam,
                     FNV256blockBasis(g_fnv_err_bytes, -1, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256blockBasis(g_fnv_err_bytes, 1, NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256blockBasis(g_fnv_err_bytes, 1, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV256file(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256file("foo.txt", NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV256fileBasis(NULL, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256fileBasis("foo.txt", NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256fileBasis("foo.txt", g_fnv_hash, NULL));

    // --- Context init checks ---
    FL_ASSERT_EQ_INT(fnvSuccess, FNV256init(&ctx));
    FL_ASSERT_EQ_INT(fnvNull, FNV256init(NULL));
    FL_ASSERT_EQ_INT(fnvSuccess, FNV256initBasis(&ctx, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256initBasis(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256initBasis(&ctx, NULL));

    // --- State-error checks ---
    ctx.Computed = FNVclobber + FNV256state;
    FL_ASSERT_EQ_INT(fnvNull, FNV256blockin(NULL, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV256blockin(&ctx, NULL, 3));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV256blockin(&ctx, g_fnv_err_bytes, -1));
    FL_ASSERT_EQ_INT(fnvStateError, FNV256blockin(&ctx, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV256stringin(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV256stringin(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV256stringin(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV256filein(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV256filein(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV256filein(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV256result(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV256result(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV256result(&ctx, g_fnv_hash));

    // --- Basis derivation (big-endian: XOR last byte) ---
    {
        char const basis_string[] = "chongo <Landon Curt Noll> /\\../";
        uint8_t    zero_basis[FNV256size];
        memset(zero_basis, 0, sizeof zero_basis);
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV256stringBasis(basis_string, g_fnv_hash, zero_basis));
        g_fnv_hash[FNV256size - 1] ^= '\\';
        FL_ASSERT_MEM_EQ(k_fnv256_s[0], g_fnv_hash, FNV256size);
    }

    // --- Hash correctness ---
    for (int i = 0; i < FNV_N_TEST_STRINGS; ++i) {
        long int len = (long int)strlen(g_fnv_test_strings[i]);

        FL_ASSERT_EQ_INT(fnvSuccess, FNV256string(g_fnv_test_strings[i], g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv256_s[i], g_fnv_hash, FNV256size);

        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV256block(g_fnv_test_strings[i],
                                     (long int)(strlen(g_fnv_test_strings[i]) + 1),
                                     g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv256_b[i], g_fnv_hash, FNV256size);

        // Incremental
        FL_ASSERT_EQ_INT(fnvSuccess, FNV256init(&ctx));
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV256blockin(&ctx, g_fnv_test_strings[i], len / 2));
        if (i & 1) {
            FL_ASSERT_EQ_INT(fnvSuccess, FNV256result(&ctx, g_fnv_hash));
            FL_ASSERT_EQ_INT(fnvSuccess, FNV256initBasis(&ctx, g_fnv_hash));
        }
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV256stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV256result(&ctx, g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv256_s[i], g_fnv_hash, FNV256size);

        // File hash
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV256file(write_temp(g_fnv_test_strings[i], len), g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv256_s[i], g_fnv_hash, FNV256size);
    }
}

// ---------------------------------------------------------------------------
// FNV-512 test case (byte-array only, big-endian)
// ---------------------------------------------------------------------------

// clang-format off
FL_VOID_TEST_SETUP_CLEANUP("case512", TestFnv, test_fnv512, setup_test_fnv, cleanup_test_fnv) {
    // clang-format on
    FNV512context ctx;

    // --- Null-parameter checks ---
    FL_ASSERT_EQ_INT(fnvNull, FNV512string(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512string(g_fnv_err_string, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV512stringBasis(NULL, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512stringBasis(g_fnv_err_string, NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512stringBasis(g_fnv_err_string, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV512block(NULL, 1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV512block(g_fnv_err_bytes, -1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512block(g_fnv_err_bytes, 1, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV512blockBasis(NULL, 1, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam,
                     FNV512blockBasis(g_fnv_err_bytes, -1, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512blockBasis(g_fnv_err_bytes, 1, NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512blockBasis(g_fnv_err_bytes, 1, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV512file(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512file("foo.txt", NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV512fileBasis(NULL, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512fileBasis("foo.txt", NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512fileBasis("foo.txt", g_fnv_hash, NULL));

    // --- Context init checks ---
    FL_ASSERT_EQ_INT(fnvSuccess, FNV512init(&ctx));
    FL_ASSERT_EQ_INT(fnvNull, FNV512init(NULL));
    FL_ASSERT_EQ_INT(fnvSuccess, FNV512initBasis(&ctx, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512initBasis(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512initBasis(&ctx, NULL));

    // --- State-error checks ---
    ctx.Computed = FNVclobber + FNV512state;
    FL_ASSERT_EQ_INT(fnvNull, FNV512blockin(NULL, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV512blockin(&ctx, NULL, 3));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV512blockin(&ctx, g_fnv_err_bytes, -1));
    FL_ASSERT_EQ_INT(fnvStateError, FNV512blockin(&ctx, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV512stringin(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV512stringin(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV512stringin(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV512filein(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV512filein(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV512filein(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV512result(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV512result(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV512result(&ctx, g_fnv_hash));

    // --- Basis derivation (big-endian: XOR last byte) ---
    {
        char const basis_string[] = "chongo <Landon Curt Noll> /\\../";
        uint8_t    zero_basis[FNV512size];
        memset(zero_basis, 0, sizeof zero_basis);
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV512stringBasis(basis_string, g_fnv_hash, zero_basis));
        g_fnv_hash[FNV512size - 1] ^= '\\';
        FL_ASSERT_MEM_EQ(k_fnv512_s[0], g_fnv_hash, FNV512size);
    }

    // --- Hash correctness ---
    for (int i = 0; i < FNV_N_TEST_STRINGS; ++i) {
        long int len = (long int)strlen(g_fnv_test_strings[i]);

        FL_ASSERT_EQ_INT(fnvSuccess, FNV512string(g_fnv_test_strings[i], g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv512_s[i], g_fnv_hash, FNV512size);

        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV512block(g_fnv_test_strings[i],
                                     (long int)(strlen(g_fnv_test_strings[i]) + 1),
                                     g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv512_b[i], g_fnv_hash, FNV512size);

        // Incremental
        FL_ASSERT_EQ_INT(fnvSuccess, FNV512init(&ctx));
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV512blockin(&ctx, g_fnv_test_strings[i], len / 2));
        if (i & 1) {
            FL_ASSERT_EQ_INT(fnvSuccess, FNV512result(&ctx, g_fnv_hash));
            FL_ASSERT_EQ_INT(fnvSuccess, FNV512initBasis(&ctx, g_fnv_hash));
        }
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV512stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV512result(&ctx, g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv512_s[i], g_fnv_hash, FNV512size);

        // File hash
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV512file(write_temp(g_fnv_test_strings[i], len), g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv512_s[i], g_fnv_hash, FNV512size);
    }
}

// ---------------------------------------------------------------------------
// FNV-1024 test case (byte-array only, big-endian)
// ---------------------------------------------------------------------------

// clang-format off
FL_VOID_TEST_SETUP_CLEANUP("case1024", TestFnv, test_fnv1024, setup_test_fnv, cleanup_test_fnv) {
    // clang-format on
    FNV1024context ctx;

    // --- Null-parameter checks ---
    FL_ASSERT_EQ_INT(fnvNull, FNV1024string(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024string(g_fnv_err_string, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024stringBasis(NULL, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024stringBasis(g_fnv_err_string, NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024stringBasis(g_fnv_err_string, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024block(NULL, 1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV1024block(g_fnv_err_bytes, -1, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024block(g_fnv_err_bytes, 1, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024blockBasis(NULL, 1, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvBadParam,
                     FNV1024blockBasis(g_fnv_err_bytes, -1, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024blockBasis(g_fnv_err_bytes, 1, NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024blockBasis(g_fnv_err_bytes, 1, g_fnv_hash, NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024file(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024file("foo.txt", NULL));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024fileBasis(NULL, g_fnv_hash, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024fileBasis("foo.txt", NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024fileBasis("foo.txt", g_fnv_hash, NULL));

    // --- Context init checks ---
    FL_ASSERT_EQ_INT(fnvSuccess, FNV1024init(&ctx));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024init(NULL));
    FL_ASSERT_EQ_INT(fnvSuccess, FNV1024initBasis(&ctx, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024initBasis(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024initBasis(&ctx, NULL));

    // --- State-error checks ---
    ctx.Computed = FNVclobber + FNV1024state;
    FL_ASSERT_EQ_INT(fnvNull, FNV1024blockin(NULL, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024blockin(&ctx, NULL, 3));
    FL_ASSERT_EQ_INT(fnvBadParam, FNV1024blockin(&ctx, g_fnv_err_bytes, -1));
    FL_ASSERT_EQ_INT(fnvStateError, FNV1024blockin(&ctx, g_fnv_err_bytes, 3));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024stringin(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024stringin(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV1024stringin(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024filein(NULL, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024filein(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV1024filein(&ctx, g_fnv_err_string));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024result(NULL, g_fnv_hash));
    FL_ASSERT_EQ_INT(fnvNull, FNV1024result(&ctx, NULL));
    FL_ASSERT_EQ_INT(fnvStateError, FNV1024result(&ctx, g_fnv_hash));

    // --- Basis derivation (big-endian: XOR last byte) ---
    {
        char const basis_string[] = "chongo <Landon Curt Noll> /\\../";
        uint8_t    zero_basis[FNV1024size];
        memset(zero_basis, 0, sizeof zero_basis);
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV1024stringBasis(basis_string, g_fnv_hash, zero_basis));
        g_fnv_hash[FNV1024size - 1] ^= '\\';
        FL_ASSERT_MEM_EQ(k_fnv1024_s[0], g_fnv_hash, FNV1024size);
    }

    // --- Hash correctness ---
    for (int i = 0; i < FNV_N_TEST_STRINGS; ++i) {
        long int len = (long int)strlen(g_fnv_test_strings[i]);

        FL_ASSERT_EQ_INT(fnvSuccess, FNV1024string(g_fnv_test_strings[i], g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv1024_s[i], g_fnv_hash, FNV1024size);

        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV1024block(g_fnv_test_strings[i],
                                      (long int)(strlen(g_fnv_test_strings[i]) + 1),
                                      g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv1024_b[i], g_fnv_hash, FNV1024size);

        // Incremental
        FL_ASSERT_EQ_INT(fnvSuccess, FNV1024init(&ctx));
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV1024blockin(&ctx, g_fnv_test_strings[i], len / 2));
        if (i & 1) {
            FL_ASSERT_EQ_INT(fnvSuccess, FNV1024result(&ctx, g_fnv_hash));
            FL_ASSERT_EQ_INT(fnvSuccess, FNV1024initBasis(&ctx, g_fnv_hash));
        }
        FL_ASSERT_EQ_INT(fnvSuccess,
                         FNV1024stringin(&ctx, g_fnv_test_strings[i] + len / 2));
        FL_ASSERT_EQ_INT(fnvSuccess, FNV1024result(&ctx, g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv1024_s[i], g_fnv_hash, FNV1024size);

        // File hash
        FL_ASSERT_EQ_INT(fnvSuccess, FNV1024file(write_temp(g_fnv_test_strings[i], len),
                                                 g_fnv_hash));
        FL_ASSERT_MEM_EQ(k_fnv1024_s[i], g_fnv_hash, FNV1024size);
    }
}

// ---------------------------------------------------------------------------
// Suite registration
// ---------------------------------------------------------------------------

FL_SUITE_BEGIN(fnv)
FL_SUITE_ADD_EMBEDDED(test_fnv32)
FL_SUITE_ADD_EMBEDDED(test_fnv64)
FL_SUITE_ADD_EMBEDDED(test_fnv128)
FL_SUITE_ADD_EMBEDDED(test_fnv256)
FL_SUITE_ADD_EMBEDDED(test_fnv512)
FL_SUITE_ADD_EMBEDDED(test_fnv1024)
FL_SUITE_END;

FL_GET_TEST_SUITE("FNV", fnv)
