/**
 * @file region.c
 * @author Douglas Cuthbertson
 * @brief The implementation for the Region library.
 * @version 0.1
 * @date 2022-02-12
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "bits.h"                         // ALIGN_UP
#include "region.h"                       // Region
#include "region_os.h"                    // extend_region() prototype
#include "fl_lock.h"                      // fl_lock_acquire, fl_lock_release
#include <faultline/fl_exception_types.h> // FLExceptionReason
#include <faultline/fl_try.h>             // FL_TRY, FL_FINALLY, FL_END_TRY

#include <stddef.h> // size_t

// The number of additional bytes to commit to ensure at least SZ bytes are committed
#define REGION_TO_COMMIT(RGN, SZ) \
    (REGION_BYTES_COMMITTED(RGN) < (SZ) ? (SZ) - REGION_BYTES_COMMITTED(RGN) : 0)

// region's reserved bytes are a multiple of the system granularity
#define REGION_IS_RESERVED(RGN)                                                       \
    (((size_t)atomic_load(&(RGN)->end_reserved) - (size_t)(RGN)) % (RGN)->granularity \
     == 0)

#if defined(__cplusplus)
extern "C" {
#endif

// Region exceptions
FLExceptionReason region_out_of_memory          = "region out of memory";
FLExceptionReason region_initialization_failure = "region initialization failure";

#if defined(__cplusplus)
}
#endif

size_t region_extend(Region *region, size_t to_commit) {
    size_t committed = 0;

    fl_lock_acquire(&region->lock);
    FL_TRY {
        to_commit = ALIGN_UP(to_commit, region->page_size);
        committed = extend_region(region, to_commit);
    }
    FL_FINALLY {
        fl_lock_release(&region->lock);
    }
    FL_END_TRY;

    return committed;
}
