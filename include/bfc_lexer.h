#ifndef __BFC_LEXER_H
#define __BFC_LEXER_H

#include <stddef.h>
#include <stdint.h>

#include "bfc_cli.h"
#include "bfc_error.h"
#include "bfc_io.h"

bfc_error_t bfc_lex(bfc_token_stream_t **token_stream, const bfc_program_t *const program, const bfc_args_t cmd_args);
void bfc_token_stream_destroy(bfc_token_stream_t **ptok_stream);

#endif // __BFC_LEXER_H
