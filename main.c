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
    BF_OK = 0,
    BF_ERR_IO,
    BF_ERR_MISMATCHED_BRACKETS,
    BF_ERR_INTERNAL
} bf_err_code_t;

typedef struct {
	bf_err_code_t error_code;
	const char *msg;
} bfc_error_t;

#define BF_ERROR_OK ((bfc_error_t) { .error_code = BF_OK, .msg = NULL }) 

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


bfc_error_t inline bfc_make_error(bf_err_code_t error_code, const char *msg);

bfc_program_t *bfc_create_program(const char *file_path);
void bfc_delete_program(bfc_program_t **pprogram);

bfc_token_t inline bfc_make_token(bfc_token_type_t tok_type, uint32_t line, uint32_t col); 
void bfc_destroy_token_stream(bfc_token_stream_t **ptok_stream);

bfc_token_stream_t *bfc_lex(const bfc_program_t *program);

ssize_t *bfc_parse_jump_table(const bfc_token_stream_t *tok_stream);
void bfc_destroy_jump_table(ssize_t **pjump_table);

int main(int argc, char** argv) {
	if (argc < 2) {
    		fprintf(stderr, "Usage: %s <file.bf>\n", argv[0]);

    		return EXIT_FAILURE;
	}

	int ret = EXIT_FAILURE;

	const char *file_handle_path = argv[1];
	bfc_program_t *program = bfc_create_program(file_handle_path);
	if (program == NULL) {
		fprintf(stderr, "Error: Critical error during reading of file '%s'!\n", file_handle_path);
		
		goto end;
	}
	
	bfc_token_stream_t *tok_stream = bfc_lex(program);
	if (tok_stream == NULL) {
		fprintf(stderr, "Error: Critical error during lexer tokenization!\n");
		
		bfc_delete_program(&program);
		
		goto end;
	}

	ssize_t *jump_table = bfc_parse_jump_table(tok_stream);
	if (jump_table == NULL) {
		fprintf(stderr, "Error: Mismatched brackets\n");
		
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

bfc_error_t inline bfc_make_error(bf_err_code_t error_code, const char *msg) {
	return (bfc_error_t) {
		.error_code = error_code,
		.msg = msg
	};
}

bfc_program_t *bfc_create_program(const char *file_path) {
	FILE *file_handle;

	if((file_handle = fopen(file_path, "rb"))) {
		bfc_program_t *program = (bfc_program_t*) malloc(sizeof(bfc_program_t));
		if (!program) {
			fclose(file_handle);

			return NULL;
		}

		int seek_status = fseek(file_handle, 0, SEEK_END);
		if (seek_status != 0) {
			free(program);
			fclose(file_handle);

			return NULL;
		}
		long file_size = ftell(file_handle);
		if (file_size == -1L) {
			free(program);
			fclose(file_handle);

			return NULL;
		}

		seek_status = fseek(file_handle, 0, SEEK_SET);
		if (seek_status != 0) {
			free(program);
			fclose(file_handle);

			return NULL;
		}
	
		
		if (file_size < 0 || file_size > (LONG_MAX - 1)) {
			free(program);
			fclose(file_handle);

			return NULL;
		}

		program->buffer = (char*) malloc((file_size + 1) * sizeof(char));
		if (!program->buffer) {
			free(program);
			fclose(file_handle);

			return NULL;
		}

		program->file_size = file_size;

		program->path = malloc(strlen(file_path) + 1);
		if (!program->path) { 
			free(program->buffer); 
			free(program); 
			fclose(file_handle); 

			return NULL; 
		}

		strcpy(program->path, file_path);

		size_t end = fread(program->buffer, sizeof(char), program->file_size, file_handle);
		if (ferror(file_handle) != 0 || end != (size_t) program->file_size) {
			free(program->path);
			free(program->buffer);
			free(program);
			fclose(file_handle);

			return NULL;
		}

		program->buffer[end] = '\0';

		fclose(file_handle);

		return program;
	}

	return NULL;
}

void bfc_delete_program(bfc_program_t **pprogram) {
	if (!pprogram || !*pprogram) return;

	free((*pprogram)->path);
	free((*pprogram)->buffer);
	free(*pprogram);

	*pprogram = NULL;
}

bfc_token_t inline bfc_make_token(bfc_token_type_t tok_type, uint32_t line, uint32_t col) {
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

bfc_token_stream_t *bfc_lex(const bfc_program_t *program) {
	bfc_token_stream_t *tok_stream = (bfc_token_stream_t*) malloc(sizeof(bfc_token_stream_t));
	if (!tok_stream) return NULL;

	if (program->file_size == 0) {
		tok_stream->tokens = NULL;
		tok_stream->length = 0;

    		return tok_stream;
	}

	tok_stream->tokens = (bfc_token_t*) malloc(program->file_size * sizeof(bfc_token_t));
	if (!tok_stream->tokens) {
		free(tok_stream);

		return NULL;
	}

	size_t token_list_size = 0;
	size_t buffer_index = 0;

	uint32_t line = 0;
	uint32_t col = 0;

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
				col = 0;
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

	return tok_stream;
}

ssize_t *bfc_parse_jump_table(const bfc_token_stream_t *tok_stream) {
	size_t n = tok_stream->length;
	if (n == 0) return calloc(1, sizeof(ssize_t));

	const bfc_token_t *toks = tok_stream->tokens;

	ssize_t *jump_table = malloc(n * sizeof(ssize_t));
	if (!jump_table) return NULL;

	for (size_t i = 0; i < n; ++i) jump_table[i] = -1;

	size_t *stack = malloc(n * sizeof(size_t));
	if (!stack) { 
		free(jump_table);

		return NULL; 
	}

	size_t sp = 0;

	for (size_t i = 0; i < n; ++i) {
		if (toks[i].type == TT_LOOP_START) {
			stack[sp++] = i;
		} else if (toks[i].type == TT_LOOP_END) {
			if (sp == 0) { 
				free(stack);
				free(jump_table);

				return NULL; 
			}
			size_t j = stack[--sp];

			jump_table[j] = (ssize_t) i;
			jump_table[i] = (ssize_t) j;
		}
	}

	free(stack);

	if (sp != 0) {
		free(jump_table);

		return NULL;
	}

	return jump_table;
}

void bfc_destroy_jump_table(ssize_t **pjump_table) {
	if (!pjump_table || !*pjump_table) return;

	free(*pjump_table);

	*pjump_table = NULL;
}
