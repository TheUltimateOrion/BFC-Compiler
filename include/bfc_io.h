#ifndef __BFC_IO_H
#define __BFC_IO_H

#include <stddef.h>

#include "bfc_error.h"

typedef struct
{
    char*  path;
    char*  buffer;
    size_t file_size;
    size_t line_count;
} bfc_program_t;

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_program_create(bfc_program_t** program, char const* file_path);

void        bfc_program_destroy(bfc_program_t** pprogram);

[[nodiscard, gnu::pure, gnu::nonnull(1), gnu::returns_nonnull]]
char const* bfc_program_getname(bfc_program_t const* const program);

[[nodiscard, gnu::malloc, gnu::nonnull(1)]]
char* bfc_program_getline(bfc_program_t const* const program, size_t const n);

#endif  // __BFC_IO_H
