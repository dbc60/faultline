#ifndef DST_DEMO_SETJMP_H_
#define DST_DEMO_SETJMP_H_

/**
 * @file setjmp.h
 * @brief Declares jmp_buf for the WebAssembly demo, which never calls setjmp.
 *
 * Faultline's exception headers put a jmp_buf in FLExceptionEnvironment. wasi-libc's
 * setjmp.h stops the build with #error unless Wasm exception handling is enabled, and
 * the demo needs none: it has no FL_TRY, and its fl_throw traps.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

typedef int jmp_buf[1];

#endif // DST_DEMO_SETJMP_H_
