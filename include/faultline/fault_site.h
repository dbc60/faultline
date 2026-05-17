#ifndef FAULT_SITE_H_
#define FAULT_SITE_H_

/**
 * @file fault_site.h
 * @author Douglas Cuthbertson
 * @brief Definition of FaultSite and related data structures for fault injection site
 * tracking.
 * @version 0.2.0
 * @date 2025-07-24
 *
 * FaultSite represents a specific location in code where fault injection can occur,
 * such as memory allocation calls. This system enables discovery, enumeration, and
 * selective testing of fault injection sites.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include <faultline/buffer.h>               // Buffer API
#include <faultline/fl_abbreviated_types.h> // i64
#include <stddef.h>                         // size_t

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Information about a specific fault injection site
 *
 * A FaultSite represents a location in the code where fault injection can occur.
 * Sites are discovered during the initial non-injection pass and can be selectively
 * enabled or disabled for targeted testing.
 */
typedef struct FaultSite {
    i64         injection_index; ///< fault-injection index
    char const *file; ///< source file where the call to an allocation function occurs
    int         line; ///< line number in source file
} FaultSite;

/**
 * @brief Define FaultSiteBuffer, a typed buffer for managing collections of fault sites
 *
 * This macro defines the following functions:
 *  init_fault_site_buffer(FaultSiteBuffer *buf, size_t capacity, void *mem)
 *  new_fault_site_buffer(size_t capacity)
 *  destroy_fault_site_buffer(FaultSiteBuffer *buf)
 *  fault_site_buffer_put(FaultSiteBuffer *buf, FaultSite const *item)
 *  fault_site_buffer_get(FaultSiteBuffer *buf, size_t index)
 *  fault_site_buffer_count(FaultSiteBuffer *buf)
 *  fault_site_buffer_clear(FaultSiteBuffer *buf)
 *  fault_site_buffer_clear_and_release(FaultSiteBuffer *buf)
 *  fault_site_buffer_allocate_next_free_slot(FaultSiteBuffer *buf)
 *  fault_site_buffer_copy(FaultSiteBuffer *dst, FaultSiteBuffer *src)
 */
DEFINE_TYPED_BUFFER(FaultSite, fault_site)

/**
 * @brief Initialize a FaultSite with the given parameters
 *
 * @param site the FaultSite to initialize
 * @param id unique sequential ID for this site
 * @param type type of allocation operation
 * @param file source file where site occurs
 * @param line line number in source file
 * @param allocation_size size of allocation (0 for free operations)
 */
static inline void fault_site_init(FaultSite *site, i64 index, char const *file,
                                   int line) {
    site->injection_index = index;
    site->file            = file;
    site->line            = line;
}

#if defined(__cplusplus)
}
#endif

#endif // FAULT_SITE_H_
