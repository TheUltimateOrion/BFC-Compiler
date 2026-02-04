#include "bfc_lexer.h"

#include <stdlib.h>

bfc_token_t bfc_make_token(const bfc_token_type_t tok_type, const uint32_t line, const uint32_t col) {

	return (bfc_token_t) {
		.type = tok_type, 
		.line = line, 
		.col = col
	};
}

void bfc_token_stream_destroy(bfc_token_stream_t **ptok_stream) {

	if (!ptok_stream || !*ptok_stream) return;

	free((*ptok_stream)->tokens);
	free(*ptok_stream);

	*ptok_stream = NULL;
}

bfc_error_t bfc_lex(bfc_token_stream_t **token_stream, const bfc_program_t *const program, const bfc_args_t cmd_args) {

	bfc_error_t err = BFC_ERR_OK;

	*token_stream = NULL;
	
	bfc_token_stream_t *tok_stream = NULL;

	if (program->file_size == 0) goto end;

	err = BFC_ERR_ALLOC;

	tok_stream = (bfc_token_stream_t*) malloc(sizeof(bfc_token_stream_t));
	if (!tok_stream) goto end;

	tok_stream->tokens = NULL;
	tok_stream->length = 0;

	tok_stream->tokens = (bfc_token_t*) malloc(program->file_size * sizeof(bfc_token_t));
	if (!tok_stream->tokens) goto end;

	size_t token_list_size = 0;
	size_t buffer_index = 0;

	uint32_t line = 1;
	uint32_t col = 1;

	uint8_t in_comment = 0;

#define EMIT_TOKEN(toktype) \
    do { \
	if (!in_comment) \
		tok_stream->tokens[token_list_size++] = bfc_make_token((toktype), line, col); \
    } while (0)

	while (program->buffer[buffer_index] != '\0') {
		switch (program->buffer[buffer_index]) {
			case ';': {
				if (cmd_args.f_no_comments) break;

				in_comment = 1;
			} break;
			case '+': {
				EMIT_TOKEN(TT_INC);
			} break;

			case '-': {
				EMIT_TOKEN(TT_DEC);
			} break;

			case '<': {
				EMIT_TOKEN(TT_PTR_LEFT);
			} break;

			case '>': {
				EMIT_TOKEN(TT_PTR_RIGHT);
			} break;

			case '[': {
				EMIT_TOKEN(TT_LOOP_START);
			} break;

			case ']': {
				EMIT_TOKEN(TT_LOOP_END);
			} break;

			case ',': {
				EMIT_TOKEN(TT_INPUT);
			} break;

			case '.': {
				EMIT_TOKEN(TT_OUTPUT);
			} break;

			case '\n': {
				++line;
				col = 1;
				++buffer_index;
				in_comment = 0;
				continue;
			} break;

			default: break;
		}

		++buffer_index;
		++col;
	}

#undef EMIT_TOKEN

	if (token_list_size > 0) {
		bfc_token_t *tmp = realloc(tok_stream->tokens, token_list_size * sizeof(bfc_token_t));
		if (tmp) tok_stream->tokens = tmp;
	} else {
		free(tok_stream->tokens);
		tok_stream->tokens = NULL;
	}
	
	tok_stream->length = token_list_size;

	*token_stream = tok_stream;

	tok_stream = NULL;

	err = BFC_ERR_OK;

end:
	if (tok_stream) {
		free(tok_stream->tokens);
		free(tok_stream);
	}

	return err;
}
