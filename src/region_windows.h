#ifndef REGION_WINDOWS_H_
#define REGION_WINDOWS_H_

/**
 * @file region_windows.h
 * @author Douglas Cuthbertson
 * @brief Windows-specific Region operations requiring VirtualQuery.
 * @version 0.1
 * @date 2026-02-15
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <stdbool.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

/**
 * @brief Check whether the memory immediately after this region's reservation is free
 * and large enough to extend by bytes_needed.
 *
 * @note Windows-specific: uses VirtualQuery. Must be compiled with Windows.h included.
 */
static inline bool region_can_extend(Region *region, size_t bytes_needed) {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = VirtualQuery((LPVOID)region->end_reserved, &mbi, sizeof(mbi));
    return (result != 0 && mbi.State == MEM_FREE && mbi.RegionSize >= bytes_needed);
}

#endif // REGION_WINDOWS_H_
