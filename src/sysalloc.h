#ifndef SYSALLOC_H_
#define SYSALLOC_H_

/**
 * @file sysalloc.h
 * @author Douglas Cuthbertson
 * @brief declare mmap, direct_mmap, and munmap.
 * @version 0.1
 * @date 2024-08-15
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/size.h>
#include <stddef.h> // size_t

/* MORECORE and MMAP must return MFAIL on failure */
#define MFAIL ((void *)(MAX_SIZE_T))

void *mmap(size_t size);

void *direct_mmap(size_t size);

int munmap(void *addr, size_t size);

#endif // SYSALLOC_H_
