#ifndef __BFC_TOKEN_H
#define __BFC_TOKEN_H

#include <stddef.h>
#include <stdint.h>

#define TOKEN_LIST        \
	X(TT_INC,        '+') \
	X(TT_DEC,        '-') \
	X(TT_PTR_RIGHT,  '>') \
	X(TT_PTR_LEFT,   '<') \
	X(TT_LOOP_START, '[') \
	X(TT_LOOP_END,   ']') \
	X(TT_OUTPUT,     '.') \
	X(TT_INPUT,      ',')

typedef enum {
#define X(tok_type, ...) tok_type,
	TOKEN_LIST
#undef X
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
