#include "bfc_jumptable.h"

#include <stdio.h>
#include <stdlib.h>

bfc_error_t bfc_parse_jump_table(ssize_t **jump_table, const bfc_token_stream_t *const tok_stream) {

	bfc_error_t err;
	char err_str[512];

	size_t n = tok_stream->length;
	if (n == 0) {
		*jump_table = calloc(1, sizeof(ssize_t));

		return BFC_ERR_OK;
	}

	const bfc_token_t *toks = tok_stream->tokens;

	ssize_t *jtable = malloc(n * sizeof(ssize_t));
	if (!jtable) return BFC_ERR_ALLOC;

	for (size_t i = 0; i < n; ++i) jtable[i] = -1;

	size_t *stack = malloc(n * sizeof(size_t));
	if (!stack) { 
		free(jtable);

		return BFC_ERR_ALLOC;
	}

	size_t sp = 0;

	size_t i;
	size_t j;
	for (i = 0; i < n; ++i) {
		if (toks[i].type == TT_LOOP_START) {
			stack[sp++] = i;
		} else if (toks[i].type == TT_LOOP_END) {
			if (sp == 0) goto extra_closing_bracket;
			j = stack[--sp];

			jtable[j] = (ssize_t) i;
			jtable[i] = (ssize_t) j;
		}
	}
	
	if (sp != 0) goto missing_closing_bracket;

	free(stack);

	*jump_table = jtable;
	return BFC_ERR_OK;

extra_closing_bracket:
	snprintf(err_str, sizeof(err_str), "Found an extra ']' at line %lu.", toks[i].line);

	err = bfc_make_error_with_token(ERR_MISMATCHED_BRACKET, err_str, toks[i]);

	free(stack);
	free(jtable);
	
	return err;

missing_closing_bracket:
	snprintf(
		err_str, sizeof(err_str), 
		"Missing a closing bracket ']' for opening bracket '[' at line %lu.", 
		toks[stack[sp - 1]].line
	);
	
	err = bfc_make_error_with_token(ERR_MISSING_BRACKET, err_str, toks[stack[sp - 1]]);

	free(stack);
	free(jtable);

	return err;
}

void bfc_jump_table_destroy(ssize_t **pjump_table) {

	if (!pjump_table || !*pjump_table) return;

	free(*pjump_table);

	*pjump_table = NULL;
}

