#include "bfc_memory.h"

#include <stdckdint.h>
#include <stdlib.h>

void* bfc_malloc_array(size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return malloc(bytes);
}

void* bfc_calloc_array(size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return calloc(1, bytes);
}

void* bfc_realloc_array(void* allocation, size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return realloc(allocation, bytes);
}
