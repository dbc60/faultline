/**
 * @file sysalloc_windows.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2024-08-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <memoryapi.h> // for VirtualAlloc, VirtualFree, VirtualQuery
#include <minwindef.h> // for MEM_COMMIT, MEM_RESERVE, PAGE_READWRITE, FALSE
#include <stddef.h>    // for size_t, NULL
#include <windows.h>   // lines 16-16
#include "sysalloc.h"  // for MFAIL, direct_mmap, mmap, munmap

// Use VirtualAlloc to reserve and commit one or more pages of memory.
void *mmap(size_t size) {
    void *addr = VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    return (addr != NULL) ? addr : MFAIL;
}

/* For direct MMAP, use MEM_TOP_DOWN to minimize interference */
void *direct_mmap(size_t size) {
    void *addr
        = VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE);
    return (addr != NULL) ? addr : MFAIL;
}

/* This function supports releasing coalesed segments */
int munmap(void *addr, size_t size) {
    MEMORY_BASIC_INFORMATION minfo;
    char                    *cptr = (char *)addr;

    // loop until all (possibly coalesed) segments are released.
    while (size) {
        if (VirtualQuery(cptr, &minfo, sizeof(minfo)) == 0) {
            return -1;
        }

        if (minfo.BaseAddress != cptr || minfo.AllocationBase != cptr
            || minfo.State != MEM_COMMIT || minfo.RegionSize > size) {
            return -1;
        }

        if (VirtualFree(cptr, 0, MEM_RELEASE) == FALSE) {
            return -1;
        }

        cptr += minfo.RegionSize;
        size -= minfo.RegionSize;
    }
    return 0;
}
