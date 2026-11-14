#ifndef POISON_H_
#define POISON_H_

/**
 * @file poison.h
 * @author Douglas Cuthbertson
 * @brief Mark bytes an allocator owns, but its caller must not touch, so that
 * AddressSanitizer reports any access to them.
 * @version 0.1
 * @date 2026-11-14
 *
 * ASan sees only the CRT heap, the stack and globals. An allocator that carves blocks
 * out of memory it obtained elsewhere (e.g. VirtualAlloc) is invisible to it until the
 * allocator poisons the bytes no caller owns: freed blocks, and the bytes past a
 * request. Without ASan both macros compile to nothing.
 *
 * ASan tracks memory in 8-byte granules. Poisoning a range whose end is not
 * granule-aligned leaves the last partial granule addressable, so a poisoned range
 * never covers a byte past its end.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h> // ASAN_POISON_MEMORY_REGION, ASAN_UNPOISON_...

#define POISON_BYTES(ADDR, SIZE)   ASAN_POISON_MEMORY_REGION((ADDR), (SIZE))
#define UNPOISON_BYTES(ADDR, SIZE) ASAN_UNPOISON_MEMORY_REGION((ADDR), (SIZE))
#else
#define POISON_BYTES(ADDR, SIZE)   ((void)(ADDR), (void)(SIZE))
#define UNPOISON_BYTES(ADDR, SIZE) ((void)(ADDR), (void)(SIZE))
#endif

#endif // POISON_H_
