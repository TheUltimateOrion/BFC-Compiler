#ifndef BFC_JUMPTABLE_H
#define BFC_JUMPTABLE_H

#include <stdint.h>

#include "bfc_error.h"
#include "bfc_token.h"

/*
 * Validate matching loop brackets and build a token-index jump table.
 *
 * Each bracket entry stores the index of its matching bracket; non-bracket
 * entries contain -1. On success, the caller owns *jump_table.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_parse_jump_table(int64_t** jump_table, bfc_token_stream_t const* const tok_stream);

/* Release a jump table and set the caller's pointer to null. */
void bfc_jump_table_destroy(int64_t** pjump_table);

#endif  // BFC_JUMPTABLE_H
