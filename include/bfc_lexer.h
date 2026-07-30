/**
 * @file bfc_lexer.h
 * @brief Brainfuck lexical analysis interface.
 *
 * @details
 * Transforms a loaded source program into a location-aware token stream.
 */
#ifndef BFC_LEXER_H
#define BFC_LEXER_H

#include "bfc_cli.h"
#include "bfc_error.h"
#include "bfc_io.h"
#include "bfc_token.h"

/**
 * @brief Tokenizes a loaded Brainfuck source program.
 *
 * @param[out] token_stream Receives the allocated token stream.
 * @param[in] program Loaded source program.
 * @param[in] cmd_args Lexer-affecting command-line options.
 *
 * @return `BFC_ERR_OK` on success; otherwise an allocation error.
 *
 * @note Release the result with `bfc_token_stream_destroy()`.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_lex(
    bfc_token_stream_t**       token_stream,
    bfc_program_t const* const program,
    bfc_args_t const           cmd_args
);

#endif  // BFC_LEXER_H
