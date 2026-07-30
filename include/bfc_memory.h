#ifndef BFC_MEMORY_H
#define BFC_MEMORY_H

#include <stddef.h>

/*
 * Allocate an array of count elements, each element_size bytes wide.
 *
 * The multiplication is checked before allocation. A null pointer is returned
 * when the byte count overflows or the allocation fails.
 */
[[gnu::malloc, gnu::alloc_size(1, 2)]]
void* bfc_malloc_array(size_t count, size_t element_size);

/*
 * Allocate and zero-initialize an array using checked size multiplication.
 * Returns null on arithmetic overflow or allocation failure.
 */
[[gnu::malloc, gnu::alloc_size(1, 2)]]
void* bfc_calloc_array(size_t count, size_t element_size);

/*
 * Resize an existing array using checked size multiplication.
 *
 * On failure, the original allocation remains valid and must still be freed by
 * the caller. Passing a null allocation has the same effect as malloc.
 */
[[gnu::alloc_size(2, 3)]]
void* bfc_realloc_array(void* allocation, size_t count, size_t element_size);

/*
 * Typed convenience wrappers. The pointer expression supplies only the element
 * type for malloc/calloc and is not evaluated there. The realloc form evaluates
 * the pointer once as the allocation argument.
 */
#define BFC_MALLOC_ARRAY(pointer, count) bfc_malloc_array((count), sizeof(*(pointer)))
#define BFC_CALLOC_ARRAY(pointer, count) bfc_calloc_array((count), sizeof(*(pointer)))
#define BFC_REALLOC_ARRAY(pointer, count) bfc_realloc_array((pointer), (count), sizeof(*(pointer)))

#endif  // BFC_MEMORY_H
