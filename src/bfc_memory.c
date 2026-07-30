/**
 * @file bfc_memory.c
 * @brief Checked array-allocation implementation.
 *
 * @details
 * Uses C23 checked arithmetic to reject element-count multiplication overflow before allocation.
 */
#include "bfc_memory.h"

#include <stdckdint.h>
#include <stdlib.h>
/**
 * @brief Allocates an uninitialized typed array after checked size multiplication.
 */

void* bfc_malloc_array(size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return malloc(bytes);
}
/**
 * @brief Allocates a zero-filled typed array after checked size multiplication.
 */

void* bfc_calloc_array(size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return calloc(1, bytes);
}
/**
 * @brief Resizes a typed array after checked size multiplication.
 */

void* bfc_realloc_array(void* allocation, size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return realloc(allocation, bytes);
}
