/**
 * @file bfc_token.c
 * @brief Token construction and token-stream cleanup.
 *
 * @details
 * Implements value construction and ownership-aware destruction for lexical tokens.
 */
#include "bfc_token.h"

#include <stdlib.h>
/**
 * @brief Constructs a token value from its kind and source coordinates.
 */

bfc_token_t bfc_make_token(bfc_token_type_t const tok_type, uint32_t const line, uint32_t const col)
{
    return (bfc_token_t) {.type = tok_type, .line = line, .col = col};
}
/**
 * @brief Releases the owned token array and stream object, then nulls the caller pointer.
 */

void bfc_token_stream_destroy(bfc_token_stream_t** ptok_stream)
{
    if (!ptok_stream || !*ptok_stream)
    {
        return;
    }

    free((*ptok_stream)->tokens);
    free(*ptok_stream);

    *ptok_stream = nullptr;
}
