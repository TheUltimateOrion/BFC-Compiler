/**
 * @file bfc_jumptable.h
 * @brief Bracket validation and matching table interface.
 *
 * @details
 * Validates loop delimiters and records matching bracket token indices.
 */
#ifndef BFC_JUMPTABLE_H
#define BFC_JUMPTABLE_H

#include <stdint.h>

#include "bfc_error.h"
#include "bfc_token.h"

/**
 * @brief Validates loop delimiters and builds matching bracket indices.
 *
 * @param[out] jump_table Receives an allocated table indexed by token position.
 * @param[in] tok_stream Token stream to validate.
 *
 * @return `BFC_ERR_OK` on success; otherwise an allocation or bracket error.
 *
 * @note Non-bracket entries contain `-1`.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_parse_jump_table(int64_t** jump_table, bfc_token_stream_t const* const tok_stream);

/**
 * @brief Releases a jump table and nulls the caller pointer.
 */
void bfc_jump_table_destroy(int64_t** pjump_table);

#endif  // BFC_JUMPTABLE_H
