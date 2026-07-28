#ifndef BFC_LEXER_H
#define BFC_LEXER_H

#include "bfc_cli.h"
#include "bfc_error.h"
#include "bfc_io.h"
#include "bfc_token.h"

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_lex(
    bfc_token_stream_t**       token_stream,
    bfc_program_t const* const program,
    bfc_args_t const           cmd_args
);

void bfc_token_stream_destroy(bfc_token_stream_t** ptok_stream);

#endif  // BFC_LEXER_H
