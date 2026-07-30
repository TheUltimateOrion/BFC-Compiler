#ifndef BFC_IO_H
#define BFC_IO_H

#include <stddef.h>

#include "bfc_error.h"

/*
 * Owning representation of one loaded Brainfuck source file.
 * path and buffer are allocated by bfc_program_create().
 */
typedef struct bfc_program_t
{
    char*  path;
    char*  buffer;
    size_t file_size;
    size_t line_count;
} bfc_program_t;

/*
 * Load file_path into a newly allocated program object.
 * On success, the caller owns *program and must destroy it.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_program_create(bfc_program_t** program, char const* file_path);

/*
 * Release a source program and set the caller's pointer to null.
 * Passing null or a pointer to null is permitted.
 */
void bfc_program_destroy(bfc_program_t** pprogram);

/* Return a borrowed pointer to the basename within program->path. */
[[nodiscard, gnu::pure, gnu::nonnull(1), gnu::returns_nonnull]]
char const* bfc_program_getname(bfc_program_t const* const program);

/*
 * Return a newly allocated copy of the requested one-based source line.
 * The caller must free the returned string. Null indicates an invalid line or
 * an allocation failure.
 */
[[nodiscard, gnu::malloc, gnu::nonnull(1)]]
char* bfc_program_getline(bfc_program_t const* const program, size_t const n);

#endif  // BFC_IO_H
