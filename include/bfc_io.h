#ifndef __BFC_IO_H
#define __BFC_IO_H

#include <stddef.h>

#include "bfc_error.h"

typedef struct {
	char *path;
	char *buffer;
	size_t file_size;
	size_t line_count;
} bfc_program_t;

bfc_error_t bfc_program_create(bfc_program_t **program, const char *file_path);
void bfc_program_destroy(bfc_program_t **pprogram);
const char *bfc_program_getname(const bfc_program_t *const program);
char *bfc_program_getline(const bfc_program_t *const program, const size_t n);

#endif // __BFC_IO_H
