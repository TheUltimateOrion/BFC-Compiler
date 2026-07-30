#ifndef BFC_LEXER_H
#define BFC_LEXER_H

#include "bfc_cli.h"
#include "bfc_error.h"
#include "bfc_io.h"
#include "bfc_token.h"

/*
 * Tokenize a loaded source program.
 *
 * Non-Brainfuck characters are ignored. Unless f_no_comments is set, a
 * semicolon suppresses the remainder of its source line. On success, the
 * caller owns *token_stream and must release it with
 * bfc_token_stream_destroy().
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_lex(
    bfc_token_stream_t**       token_stream,
    bfc_program_t const* const program,
    bfc_args_t const           cmd_args
);

#endif  // BFC_LEXER_H
