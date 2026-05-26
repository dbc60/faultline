#ifndef REGION_OS_H_
#define REGION_OS_H_

/**
 * @file region_os.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-05-26
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "region.h" // Region
#include <stddef.h> // size_t

size_t extend_region(Region *region, size_t to_commit);

#endif // REGION_OS_H_
