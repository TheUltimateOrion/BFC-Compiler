/**
 * @file bfc_io.h
 * @brief Source-file loading and source-line access.
 *
 * @details
 * Defines the owning source-program object and its lifecycle.
 */
#ifndef BFC_IO_H
#define BFC_IO_H

#include <stddef.h>

#include "bfc_error.h"

/**
 * @brief Owning representation of a loaded source file.
 *
 * @note `path` and `buffer` are released by `bfc_program_destroy()`.
 */
typedef struct bfc_program_t
{
    char*  path;
    char*  buffer;
    size_t file_size;
    size_t line_count;
} bfc_program_t;

/**
 * @brief Loads an entire source file into memory.
 *
 * @param[out] program Receives the allocated source object.
 * @param[in] file_path Path of the source file to load.
 *
 * @return `BFC_ERR_OK` on success; otherwise an I/O or allocation error.
 *
 * @note Release the result with `bfc_program_destroy()`.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_program_create(bfc_program_t** program, char const* file_path);

/**
 * @brief Releases a source program and nulls the caller pointer.
 */
void bfc_program_destroy(bfc_program_t** pprogram);

/**
 * @brief Returns the base name of the source path.
 *
 * @return A borrowed pointer into `program->path`; do not free it.
 */
[[nodiscard, gnu::pure, gnu::nonnull(1), gnu::returns_nonnull]]
char const* bfc_program_getname(bfc_program_t const* const program);

/**
 * @brief Copies one source line into a new allocation.
 *
 * @param[in] program Loaded source program.
 * @param[in] n One-based line number.
 *
 * @return A newly allocated line string, or `nullptr` when unavailable.
 *
 * @note The caller must free the returned string.
 */
[[nodiscard, gnu::malloc, gnu::nonnull(1)]]
char* bfc_program_getline(bfc_program_t const* const program, size_t const n);

#endif  // BFC_IO_H
