#ifndef BFC_MEMORY_H
#define BFC_MEMORY_H

#include <stddef.h>

[[gnu::malloc, gnu::alloc_size(1, 2)]]
void* bfc_malloc_array(size_t count, size_t element_size);

[[gnu::malloc, gnu::alloc_size(1, 2)]]
void* bfc_calloc_array(size_t count, size_t element_size);

[[gnu::alloc_size(2, 3)]]
void* bfc_realloc_array(void* allocation, size_t count, size_t element_size);

#define BFC_MALLOC_ARRAY(pointer, count) bfc_malloc_array((count), sizeof(*(pointer)))

#define BFC_CALLOC_ARRAY(pointer, count) bfc_calloc_array((count), sizeof(*(pointer)))

#define BFC_REALLOC_ARRAY(pointer, count) bfc_realloc_array((pointer), (count), sizeof(*(pointer)))

#endif
