/**
 * @file bfc_memory.h
 * @brief Checked array-allocation helpers.
 *
 * @details
 * Provides overflow-checked allocation functions for typed arrays and convenience macros that infer element size from pointer type.
 */
#ifndef BFC_MEMORY_H
#define BFC_MEMORY_H

#include <stddef.h>

/**
 * @brief Allocates an uninitialized array after checking count multiplication.
 *
 * @param[in] count Number of elements.
 * @param[in] element_size Size of each element in bytes.
 *
 * @return A newly allocated block, or `nullptr` on overflow or allocation failure.
 *
 * @note The returned allocation must be released with `free()`.
 */
[[gnu::malloc, gnu::alloc_size(1, 2)]]
void* bfc_malloc_array(size_t count, size_t element_size);

/**
 * @brief Allocates a zero-initialized array after checking count multiplication.
 *
 * @param[in] count Number of elements.
 * @param[in] element_size Size of each element in bytes.
 *
 * @return A zero-initialized block, or `nullptr` on overflow or allocation failure.
 *
 * @note The returned allocation must be released with `free()`.
 */
[[gnu::malloc, gnu::alloc_size(1, 2)]]
void* bfc_calloc_array(size_t count, size_t element_size);

/**
 * @brief Resizes a typed array after checking count multiplication.
 *
 * @param[in,out] allocation Existing allocation, or `nullptr`.
 * @param[in] count Requested number of elements.
 * @param[in] element_size Size of each element in bytes.
 *
 * @return The resized block, or `nullptr` on overflow or allocation failure.
 *
 * @note On failure, the original allocation remains valid.
 */
[[gnu::alloc_size(2, 3)]]
void* bfc_realloc_array(void* allocation, size_t count, size_t element_size);

/**
 * @brief Allocates an array and infers the element size from the pointer expression.
 */
#define BFC_MALLOC_ARRAY(pointer, count) bfc_malloc_array((count), sizeof(*(pointer)))

#define BFC_CALLOC_ARRAY(pointer, count) bfc_calloc_array((count), sizeof(*(pointer)))

#define BFC_REALLOC_ARRAY(pointer, count) bfc_realloc_array((pointer), (count), sizeof(*(pointer)))

#endif
