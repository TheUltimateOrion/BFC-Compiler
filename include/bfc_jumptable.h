#ifndef BFC_JUMPTABLE_H
#define BFC_JUMPTABLE_H

#include <stdint.h>

#include "bfc_error.h"
#include "bfc_token.h"

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_parse_jump_table(int64_t** jump_table, bfc_token_stream_t const* const tok_stream);

void bfc_jump_table_destroy(int64_t** pjump_table);

#endif  // BFC_JUMPTABLE_H
