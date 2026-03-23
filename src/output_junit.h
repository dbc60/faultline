#ifndef OUTPUT_JUNIT_H_
#define OUTPUT_JUNIT_H_

/**
 * @file output_junit.h
 * @author Douglas Cuthbertson
 * @brief JUNIT XML implementation
 * @version 0.1
 * @date 2026-03-14
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/arena.h>      // Arena
#include <faultline/fl_context.h> // FLContext
#include <stdio.h>                // FILE

/**
 * @brief determine what to free in destroy_junit
 *
 */
typedef enum {
    JUNIT_PATH_ONLY,
    JUNIT_ALL,
} JUnitReleaseType;

typedef struct {
    Arena           *arena;   ///* the arena to use for allocations
    char            *path;    ///< the path to a file to store JUNIT XML data
    size_t           size;    ///< size of path buffer in bytes
    FILE            *file;    ///< handle to a file
    JUnitReleaseType release; ///< true if allocated by new_junit, false otherwise
} JUnitXML;

/**
 * @brief initialize a JUnitXML object in place
 *
 * @param junit
 * @param path
 */
void junit_init(JUnitXML *junit, Arena *arena, char const *path);

/**
 * @brief return a freshly allocated and initialized JUnitXML object
 *
 * @param path the path to a file to stor JUNIT XML data
 * @return JUnitXML* a freshly allocated JUnitXML object
 * @throw
 */
JUnitXML *new_junit(Arena *arena, char const *path);

/**
 * @brief free resources allocated by new_junit
 *
 * @param junit the JUnitXML object to be freed
 */
void destroy_junit(JUnitXML *junit);

/**
 * @brief write an XML header and <testsuites> opening tag
 *
 * @param junit JUnitXML object
 * @return int 0 on success, non-zero on I/O error
 */
int junit_begin(JUnitXML *junit);

/**
 * @brief write the </testsuites> closing tag
 *
 * @param junit
 * @return int 0 on success, non-zero on I/O error
 */
int junit_end(JUnitXML *junit);

/**
 * @brief write test results for a single test suite
 *
 * @param junit JUnitXML object
 * @param fctx context to record
 * @return int 0 on success, non-zero on I/O error
 */
int junit_write(JUnitXML *junit, FLContext *fctx);

#endif // OUTPUT_JUNIT_H_
