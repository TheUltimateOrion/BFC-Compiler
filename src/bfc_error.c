#include "bfc_error.h"

#include "bfc_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

bfc_error_t bfc_make_error(const bfc_err_code_t error_code, const char *msg) {

    bfc_error_t err = {0};

    err.code = error_code;

    if (msg) snprintf(err.msg, sizeof(err.msg), "%s", msg);

    return err;
}

bfc_error_t bfc_make_error_with_token(const bfc_err_code_t error_code, const char *msg, const bfc_token_t token) {

    bfc_error_t err = {0};

    err.code = error_code;
    err.token = token;

    if (msg) snprintf(err.msg, sizeof(err.msg), "%s", msg);

    return err;
}

const char *bfc_get_error_code(const bfc_err_code_t error_code) {
	switch (error_code) {
		case ERR_OK: {
			return "ERROR_OK";
		} break;
		case ERR_ARGS: {
			return "ERROR_ARGS";
		} break;
		case ERR_ALLOC: {
			return "ERROR_ALLOC";
		} break;
		case ERR_IO: {
			return "ERROR_IO";
		} break;
		case ERR_MISMATCHED_BRACKET: {
			return "ERROR_MISMATCHED_BRACKET";
		} break;
		case ERR_MISSING_BRACKET: {
			return "ERROR_MISSING_BRACKET";
		} break;
		default: {
			return "Unknown error";
		} break;
	}
}

void bfc_log_error(const bfc_error_t err, const struct bfc_program_t *const program) {

	if (err.code == ERR_MISSING_BRACKET || err.code == ERR_MISMATCHED_BRACKET) {
		fprintf(
			stderr, COL_INFO "%s[%lu, %lu]: " COL_ERROR "%s" COL_OFF COL_INFO ": %s\n" COL_OFF, 
			bfc_program_getname((bfc_program_t*) program), err.token.line, err.token.col, bfc_get_error_code(err.code), err.msg
		);

	
		char *line_buf = bfc_program_getline((bfc_program_t*) program, (size_t)err.token.line);

		int line_num_width = (err.token.line > 0) ? (int)log10(err.token.line) + 1 : 1;

		fprintf(stderr, "   %lu | %s\n", (size_t) err.token.line, line_buf);
		fprintf(stderr, "   %*s | %*c\n", line_num_width, "", (int)err.token.col, '^');
		
		free(line_buf);

		return;
	}

	fprintf(
		stderr, COL_INFO "bfc: " COL_ERROR "%s" COL_OFF COL_INFO ": %s\n" COL_OFF, 
		bfc_get_error_code(err.code), err.msg
	);
}
