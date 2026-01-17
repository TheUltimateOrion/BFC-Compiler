#include <sys/types.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef enum {
    BFC_OK = 0,
    BFC_ERR_IO,
    BFC_ERR_MISMATCHED_BRACKETS,
    BFC_ERR_ALLOC
} bfc_err_code_t;

typedef struct {
	bfc_token_type_t type;
	uint32_t line;
	uint32_t col;
} bfc_token_t;

typedef struct {
	bfc_token_t *tokens;
	size_t length;
} bfc_token_stream_t;

typedef struct {
	char *path;
	char *buffer;
	size_t file_size;
} bfc_program_t;

typedef struct {
	bfc_err_code_t code;
	char *msg;
	bfc_token_t token;
} bfc_error_t;

#define BFC_ERROR_OK ((bfc_error_t) { .code = BFC_OK, .msg = NULL, .token = {0} })

bfc_error_t bfc_make_error(const bfc_err_code_t error_code, char *msg);
bfc_error_t bfc_make_error_with_token(const bfc_err_code_t error_code, char *msg, bfc_token_t token);
const char *bfc_get_err_str(const bfc_err_code_t error_code);
void bfc_log_err(const bfc_error_t err, const bfc_program_t *program);

bfc_error_t bfc_create_program(const char *file_path, bfc_program_t **program);
void bfc_delete_program(bfc_program_t **pprogram);

bfc_token_t bfc_make_token(bfc_token_type_t tok_type, uint32_t line, uint32_t col); 
void bfc_destroy_token_stream(bfc_token_stream_t **ptok_stream);

bfc_error_t bfc_lex(const bfc_program_t *program, bfc_token_stream_t **token_stream);

bfc_error_t bfc_parse_jump_table(const bfc_token_stream_t *tok_stream, ssize_t **jtable);
void bfc_destroy_jump_table(ssize_t **pjump_table);

int main(int argc, char** argv) {
	if (argc < 2) {
    		fprintf(stderr, "Usage: %s <file.bf>\n", argv[0]);

    		return EXIT_FAILURE;
	}

	int ret = EXIT_FAILURE;

	bfc_error_t err;

	bfc_program_t *program = NULL;

	const char *file_path = argv[1];
	err = bfc_create_program(file_path, &program);
	if (err.code != BFC_OK) {
		bfc_log_err(err, NULL);
		
		goto end;
	}
	
	bfc_token_stream_t *tok_stream = NULL;
	err = bfc_lex(program, &tok_stream);
	if (err.code != BFC_OK) {
		bfc_log_err(err, program);	
	
		bfc_delete_program(&program);
		
		goto end;
	}

	ssize_t *jump_table = NULL;
	err = bfc_parse_jump_table(tok_stream, &jump_table);
	if (err.code != BFC_OK) {
		bfc_log_err(err, program);

		bfc_destroy_token_stream(&tok_stream);
		bfc_delete_program(&program);

		goto end;
	}

	ret = EXIT_SUCCESS;

	bfc_destroy_jump_table(&jump_table);
	bfc_destroy_token_stream(&tok_stream);
	bfc_delete_program(&program);

end:
	return ret;
}

bfc_error_t bfc_make_error(const bfc_err_code_t error_code, char *msg) {
	return (bfc_error_t) {
		.code = error_code,
		.msg = msg,
	};
}

bfc_error_t bfc_make_error_with_token(const bfc_err_code_t error_code, char *msg, bfc_token_t token) {
	return (bfc_error_t) {
		.code = error_code,
		.msg = msg,
		.token = token,
	};
}

const char *bfc_get_err_str(const bfc_err_code_t error_code) {
	switch (error_code) {
		case BFC_OK: {
			return "BFC_OK";
		} break;
		case BFC_ERR_ALLOC: {
			return "BFC_ERR_ALLOC";
		} break;
		case BFC_ERR_IO: {
			return "BFC_ERR_IO";
		} break;
		case BFC_ERR_MISMATCHED_BRACKETS: {
			return "BFC_ERR_MISMATCHED_BRACKETS";
		} break;
		default: {
			return "Unknown error!";
		} break;
	}
}

void bfc_log_err(const bfc_error_t err, const bfc_program_t *program) {
	if (err.code == BFC_ERR_MISMATCHED_BRACKETS) {
		fprintf(stderr, "%s[%d, %d]: %s: %s\n", program->path, err.token.line, err.token.col, bfc_get_err_str(err.code), err.msg);
		return;
	}

	fprintf(stderr, "bfc: %s: %s\n", bfc_get_err_str(err.code), err.msg);
}

bfc_error_t bfc_create_program(const char *file_path, bfc_program_t **program) {
	FILE *file_handle;

	if((file_handle = fopen(file_path, "rb"))) {
		bfc_program_t *prog = (bfc_program_t*) malloc(sizeof(bfc_program_t));
		if (!prog) {
			fclose(file_handle);

			return bfc_make_error(BFC_ERR_ALLOC, "Memory allocation failure!");
		}

		int seek_status = fseek(file_handle, 0, SEEK_END);
		if (seek_status != 0) {
			free(prog);
			fclose(file_handle);

			return bfc_make_error(BFC_ERR_IO, "Unable to seek the end of file!");
		}
		long file_size = ftell(file_handle);
		if (file_size == -1L) {
			free(prog);
			fclose(file_handle);

			return bfc_make_error(BFC_ERR_IO, "Unable to perform ftell on file!");
		}

		seek_status = fseek(file_handle, 0, SEEK_SET);
		if (seek_status != 0) {
			free(prog);
			fclose(file_handle);

			return bfc_make_error(BFC_ERR_IO, "Unable to seek the start of file!");
		}
		
		if (file_size < 0 || file_size > (LONG_MAX - 1)) {
			free(prog);
			fclose(file_handle);

			return bfc_make_error(BFC_ERR_IO, "Invalid file size!");
		}

		prog->buffer = (char*) malloc((file_size + 1) * sizeof(char));
		if (!prog->buffer) {
			free(prog);
			fclose(file_handle);

			return bfc_make_error(BFC_ERR_ALLOC, "Memory allocation failure!");
		}

		prog->file_size = file_size;

		prog->path = malloc(strlen(file_path) + 1);
		if (!prog->path) { 
			free(prog->buffer); 
			free(prog); 
			fclose(file_handle); 

			return bfc_make_error(BFC_ERR_ALLOC, "Memory allocation failure!"); 
		}

		strcpy(prog->path, file_path);

		size_t end = fread(prog->buffer, sizeof(char), prog->file_size, file_handle);
		if (ferror(file_handle) != 0 || end != (size_t) prog->file_size) {
			free(prog->path);
			free(prog->buffer);
			free(prog);
			fclose(file_handle);

			char err_str[512];
			snprintf(err_str, sizeof(err_str), "Unable to read from file '%s'!", file_path);

			return bfc_make_error(BFC_ERR_IO, err_str);
		}

		prog->buffer[end] = '\0';

		fclose(file_handle);

		*program = prog;
		return BFC_ERROR_OK;
	}


	char err_str[512];
	snprintf(err_str, sizeof(err_str), "No such file or directory: '%s'", file_path);
	return bfc_make_error(BFC_ERR_IO, err_str);;

}

void bfc_delete_program(bfc_program_t **pprogram) {
	if (!pprogram || !*pprogram) return;

	free((*pprogram)->path);
	free((*pprogram)->buffer);
	free(*pprogram);

	*pprogram = NULL;
}

bfc_token_t bfc_make_token(bfc_token_type_t tok_type, uint32_t line, uint32_t col) {
	return (bfc_token_t) {
		.type = tok_type, 
		.line = line, 
		.col = col
	};
}

void bfc_destroy_token_stream(bfc_token_stream_t **ptok_stream) {
	if (!ptok_stream || !*ptok_stream) return;

	free((*ptok_stream)->tokens);
	free(*ptok_stream);

	*ptok_stream = NULL;
}

bfc_error_t bfc_lex(const bfc_program_t *program, bfc_token_stream_t **token_stream) {
	bfc_token_stream_t *tok_stream = (bfc_token_stream_t*) malloc(sizeof(bfc_token_stream_t));
	if (!tok_stream) return bfc_make_error(BFC_ERR_ALLOC, "Memory allocation failure!");

	if (program->file_size == 0) {
		tok_stream->tokens = NULL;
		tok_stream->length = 0;

    		*token_stream = tok_stream;
		return BFC_ERROR_OK;
	}

	tok_stream->tokens = (bfc_token_t*) malloc(program->file_size * sizeof(bfc_token_t));
	if (!tok_stream->tokens) {
		free(tok_stream);

		return bfc_make_error(BFC_ERR_ALLOC, "Memory allocation failure!");
	}

	size_t token_list_size = 0;
	size_t buffer_index = 0;

	uint32_t line = 1;
	uint32_t col = 1;

	while (program->buffer[buffer_index] != '\0') {
		switch (program->buffer[buffer_index]) {
			case '+': {
				tok_stream->tokens[token_list_size++] = bfc_make_token(TT_INC, line, col);
			} break;
			case '-': {
				tok_stream->tokens[token_list_size++] = bfc_make_token(TT_DEC, line, col);
			} break;
			case '>': {
				tok_stream->tokens[token_list_size++] = bfc_make_token(TT_PTR_RIGHT, line, col);
			} break;
			case '<': {
				tok_stream->tokens[token_list_size++] = bfc_make_token(TT_PTR_LEFT, line, col);
			} break;
			case '[': {
				tok_stream->tokens[token_list_size++] = bfc_make_token(TT_LOOP_START, line, col);
			} break;
			case ']': {
				tok_stream->tokens[token_list_size++] = bfc_make_token(TT_LOOP_END, line, col);
			} break;
			case '.': {
				tok_stream->tokens[token_list_size++] = bfc_make_token(TT_OUTPUT, line, col); 
			} break;
			case ',': {
				tok_stream->tokens[token_list_size++] = bfc_make_token(TT_INPUT, line, col); 

			} break;
			case '\n': {
				++line;
				col = 1;
				++buffer_index;
				continue;
			} break;
			default: break;
		}

		++buffer_index;
		++col;
	}

	if (token_list_size > 0) {
		bfc_token_t *tmp = realloc(tok_stream->tokens, token_list_size * sizeof(bfc_token_t));
		if (tmp) tok_stream->tokens = tmp;
	} else {
		free(tok_stream->tokens);
		tok_stream->tokens = NULL;
	}
	
	tok_stream->length = token_list_size;

	*token_stream = tok_stream;

	return BFC_ERROR_OK;
}

bfc_error_t bfc_parse_jump_table(const bfc_token_stream_t *tok_stream, ssize_t **jtable) {
	char err_str[512];

	size_t n = tok_stream->length;
	if (n == 0) {
		*jtable = calloc(1, sizeof(ssize_t));

		return BFC_ERROR_OK;
	}

	const bfc_token_t *toks = tok_stream->tokens;

	ssize_t *jump_table = malloc(n * sizeof(ssize_t));
	if (!jump_table) return bfc_make_error(BFC_ERR_ALLOC, "Memory allocation failure!");
	for (size_t i = 0; i < n; ++i) jump_table[i] = -1;

	size_t *stack = malloc(n * sizeof(size_t));
	if (!stack) { 
		free(jump_table);

		return bfc_make_error(BFC_ERR_ALLOC, "Memory allocation failure!");
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

			jump_table[j] = (ssize_t) i;
			jump_table[i] = (ssize_t) j;
		}
	}
	
	if (sp != 0) goto mismatched_bracket;

	free(stack);

	*jtable = jump_table;
	return BFC_ERROR_OK;

extra_closing_bracket:
	snprintf(err_str, sizeof(err_str), "Found an extra ']' at line %d.", toks[i].line);

	free(stack);
	free(jump_table);
	
	return bfc_make_error_with_token(BFC_ERR_MISMATCHED_BRACKETS, err_str, toks[i]);

mismatched_bracket:
	snprintf(err_str, sizeof(err_str), "Missing a closing bracket ']' for opening bracket '[' at line %d.", toks[stack[sp - 1]].line);
	
	free(stack);
	free(jump_table);

	return bfc_make_error_with_token(BFC_ERR_MISMATCHED_BRACKETS, err_str, toks[stack[sp - 1]]);
}

void bfc_destroy_jump_table(ssize_t **pjump_table) {
	if (!pjump_table || !*pjump_table) return;

	free(*pjump_table);

	*pjump_table = NULL;
}
