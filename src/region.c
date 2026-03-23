/**
 * @file region.c
 * @author Douglas Cuthbertson
 * @brief The implementation for the Region library.
 * @version 0.1
 * @date 2022-02-12
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "bits.h"   // ALIGN_UP
#include "region.h" // Region
#if defined(_WIN32) || defined(_WIN64)
#include "fl_threads.c" // mtx_init in region_windows.c
#include "region_windows.c"
#elif defined(__APPLE__)
// #include "region_apple.c"
#error Apple implementation TBD
#elif defined(__FreeBSD__)
// #include "region_freebsd.c"
#error FreeBSD implementation TBD
#elif defined(__linux__)
// #include "region_linux.c"
#error Linux implementation TBD
#else
#error Unsupported platform
#endif

#include <faultline/fl_exception_types.h> // FLExceptionReason
#include <faultline/fl_try.h>             // FL_THROW

#include <stddef.h> // size_t

// The number of additional bytes to commit to ensure at least SZ bytes are committed
#define REGION_TO_COMMIT(RGN, SZ) \
    (REGION_BYTES_COMMITTED(RGN) < (SZ) ? (SZ) - REGION_BYTES_COMMITTED(RGN) : 0)

// region's reserved bytes are a multiple of the system granularity
#define REGION_IS_RESERVED(RGN) \
    (((size_t)(RGN)->end_reserved - (size_t)RGN) % RGN->granularity == 0)

// Region exceptions
FLExceptionReason region_out_of_memory          = "region out of memory";
FLExceptionReason region_initialization_failure = "region initialization failure";

size_t region_extend(Region *region, size_t to_commit) {
    size_t committed = 0;

    mtx_lock(&region->lock);
    FL_TRY {
        to_commit = ALIGN_UP(to_commit, region->page_size);
        committed = extend_region(region, to_commit);
    }
    FL_FINALLY {
        mtx_unlock(&region->lock);
    }
    FL_END_TRY;

    return committed;
}
