/*
 * Token construction and token-stream lifetime management.
 *
 * Token streams own their token arrays. Destruction accepts a pointer to the
 * caller's pointer so the released handle can be reset to nullptr.
 */

#include "bfc_token.h"

#include <stdlib.h>

/* Construct a complete token value without exposing partial initialization. */
bfc_token_t bfc_make_token(bfc_token_type_t const tok_type, uint32_t const line, uint32_t const col)
{
    return (bfc_token_t) {.type = tok_type, .line = line, .col = col};
}

/*
 * Release both ownership layers: the token array and its containing stream.
 * Null input is accepted so the function is suitable for cleanup attributes.
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
