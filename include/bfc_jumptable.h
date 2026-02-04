#ifndef __BFC_JUMPTABLE_H
#define __BFC_JUMPTABLE_H

#include <sys/types.h>

#include "bfc_error.h"

bfc_error_t bfc_parse_jump_table(ssize_t **jump_table, const bfc_token_stream_t *const tok_stream);
void bfc_jump_table_destroy(ssize_t **pjump_table);

#endif // __BFC_JUMPTABLE_H
