#ifndef WIN32_PLATFORM_H_
#define WIN32_PLATFORM_H_

/**
 * @file win32_platform.h
 * @author Douglas Cuthbertson
 * @brief Some platform-specific functions implemented for Windows.
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // u32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h> // GetSystemInfo

#if defined(__cplusplus)
extern "C" {
#endif

static inline void get_memory_info(u32 *granularity, u32 *page_size) {
    SYSTEM_INFO system_info;

    GetSystemInfo(&system_info);
    if (granularity != 0) {
        *granularity = system_info.dwAllocationGranularity;
    }

    if (page_size != 0) {
        *page_size = system_info.dwPageSize;
    }
}

#if defined(__cplusplus)
}
#endif

#endif // WIN32_PLATFORM_H_
