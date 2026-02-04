#ifndef __BFC_TOKEN_H
#define __BFC_TOKEN_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
	TT_INC,
	TT_DEC,
	TT_PTR_RIGHT,
	TT_PTR_LEFT,
	TT_LOOP_START,
	TT_LOOP_END,
	TT_OUTPUT,
	TT_INPUT
} bfc_token_type_t;

typedef struct {
	bfc_token_type_t type;
size_t line;
	size_t col;
} bfc_token_t;

typedef struct {
	bfc_token_t *tokens;
	size_t length;
} bfc_token_stream_t;

bfc_token_t bfc_make_token(const bfc_token_type_t tok_type, const uint32_t line, const uint32_t col);

#endif // __BFC_TOKEN_H
