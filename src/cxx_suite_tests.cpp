/**
 * @file cxx_suite_tests.cpp
 * @author Douglas Cuthbertson
 * @brief A test suite written as C++, exercised by the C driver.
 * @version 0.1
 * @date 2026-09-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Every other suite is C that also happens to compile as C++ when cxx is passed. This
 * one is C++ outright, and its cases cover what only C++ can express: a destructor that
 * runs while an exception unwinds, and the release-on-the-failure-path that follows from
 * it. fl_run_case catches the throw inside this module and hands the driver an outcome,
 * so the driver reading these results is the same faultline.exe built as C.
 */
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_TRUE, FL_ASSERT_NOT_NULL
#include <faultline/fl_macros.h>                   // FL_UNUSED
#include <faultline/fl_memory.h>                   // FL_MALLOC, FL_FREE
#include <faultline/fl_test.h> // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h>  // FL_THROW

#include <exception> // std::uncaught_exceptions

#include "fl_exception_service.c"  // fl_expected_failure and the other reasons
#include "fla_exception_service.c" // the module's own push/pop/throw
#include "fla_memory_service.c"    // g_fla_memory_service, fla_set_memory_service
#include "fla_timer_service.c"     // g_fla_timer_service, fla_set_timer_service

/// Destructor calls that ran at ordinary scope exit, across every case and pass.
static int g_blocks_destroyed = 0;

/// Destructor calls that ran with an exception in flight. Counting these apart is what
/// makes test_destructor_ran mean anything: a count of every destruction would be
/// satisfied by a case that never threw.
static int g_blocks_destroyed_unwinding = 0;

/**
 * @brief Owns one allocation and releases it when it goes out of scope.
 *
 * The destructor is what makes the failure path safe: an exception leaving a case body
 * destroys the guard on its way out, so the block is released without the body having a
 * cleanup branch. A C suite has to write that branch by hand, which is the class of bug
 * fault injection exists to find.
 */
class ScopedBlock {
  public:
    explicit ScopedBlock(size_t bytes)
        : block(FL_MALLOC(bytes)) {
    }

    ~ScopedBlock() {
        // The injected allocator returns NULL on the pass that fails this call site, and
        // releasing NULL would be recorded as an invalid free.
        if (block != nullptr) {
            FL_FREE(block);
        }

        if (std::uncaught_exceptions() > 0) {
            g_blocks_destroyed_unwinding++;
        } else {
            g_blocks_destroyed++;
        }
    }

    ScopedBlock(ScopedBlock const &)            = delete;
    ScopedBlock &operator=(ScopedBlock const &) = delete;

    void *get() const {
        return block;
    }

  private:
    void *block;
};

FL_TEST("a C++ case runs and passes", test_runs) {
    ScopedBlock block(64);
    FL_ASSERT_NOT_NULL(block.get());
}

// Throws fl_expected_failure, which a driver reports as a pass. The throw is what this
// case is for: it leaves the body while a ScopedBlock is alive, so the destructor runs
// during unwinding. test_destructor_ran below reads the count it leaves behind.
FL_TEST("a destructor runs while an exception unwinds", test_destructor_on_throw) {
    ScopedBlock block(64);
    FL_ASSERT_NOT_NULL(block.get());
    FL_THROW(fl_expected_failure);
}

// Reads what the case above left behind. Ordering matters: FL_SUITE_ADD fixes the run
// order, and this case is the one after the throw.
FL_TEST("the unwound destructor ran", test_destructor_ran) {
    FL_ASSERT_TRUE(g_blocks_destroyed_unwinding > 0);
    FL_ASSERT_TRUE(g_blocks_destroyed > 0);
}

// Two allocations, the second of which fails on one injection pass. The first is
// released by ~ScopedBlock as the assertion throws, so the driver reports no leak. The
// same body in C, holding the first pointer in a local, leaks it on that pass.
FL_TEST("the first allocation is released when the second fails",
        test_release_on_fault) {
    ScopedBlock first(128);
    FL_ASSERT_NOT_NULL(first.get());

    ScopedBlock second(128);
    FL_ASSERT_NOT_NULL(second.get());
}

FL_SUITE_BEGIN(cxx_suite)
FL_SUITE_ADD(test_runs)
FL_SUITE_ADD(test_destructor_on_throw)
FL_SUITE_ADD(test_destructor_ran)
FL_SUITE_ADD(test_release_on_fault)
FL_SUITE_END;

FL_GET_TEST_SUITE("C++ Suite", cxx_suite)
