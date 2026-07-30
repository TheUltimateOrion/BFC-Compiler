/*
 * Overflow-checked allocation helpers for typed arrays.
 *
 * Each function validates count * element_size before delegating to the C
 * allocator. A nullptr result therefore represents either arithmetic overflow
 * or an allocation failure.
 */

#include "bfc_memory.h"

#include <stdckdint.h>
#include <stdlib.h>

/* Allocate an uninitialized array after checking the byte-size product. */
void* bfc_malloc_array(size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return malloc(bytes);
}

/*
 * Allocate a zero-initialized array. calloc(1, bytes) is used only after the
 * multiplication has been checked explicitly.
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

/*
 * Resize an existing typed array. On failure, realloc leaves the caller's
 * original allocation valid; callers must assign the result through a
 * temporary pointer.
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
