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
} token_type;

typedef struct {
	token_type type;
	uint32_t line;
	uint32_t col;
} token;

token create_token(token_type tok_type, uint32_t line, uint32_t col) {
	return (token) {
		.type = tok_type, 
		.line = line, 
		.col = col
	};
}

typedef struct {
	token *tokens;
	size_t length;
} token_stream;

void destroy_token_stream(token_stream **ptok_stream) {
	if (!ptok_stream || !*ptok_stream) return;

	free((*ptok_stream)->tokens);
	free(*ptok_stream);

	*ptok_stream = NULL;
}

typedef struct {
	char *path;
	char *buffer;
	size_t file_size;
} bf_program;

bf_program *create_program(const char *file_path) {
	FILE *file_handle;

	if((file_handle = fopen(file_path, "rb"))) {
		bf_program *program = (bf_program*) malloc(sizeof(bf_program));
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

token_stream *lex_program(const bf_program *program) {
	token_stream *tok_stream = (token_stream*) malloc(sizeof(token_stream));
	if (!tok_stream) return NULL;

	if (program->file_size == 0) {
		tok_stream->tokens = NULL;
		tok_stream->length = 0;

    		return tok_stream;
	}

	tok_stream->tokens = (token*) malloc(program->file_size * sizeof(token));
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
				tok_stream->tokens[token_list_size++] = create_token(TT_INC, line, col);
			} break;
			case '-': {
				tok_stream->tokens[token_list_size++] = create_token(TT_DEC, line, col);
			} break;
			case '>': {
				tok_stream->tokens[token_list_size++] = create_token(TT_PTR_RIGHT, line, col);
			} break;
			case '<': {
				tok_stream->tokens[token_list_size++] = create_token(TT_PTR_LEFT, line, col);
			} break;
			case '[': {
				tok_stream->tokens[token_list_size++] = create_token(TT_LOOP_START, line, col);
			} break;
			case ']': {
				tok_stream->tokens[token_list_size++] = create_token(TT_LOOP_END, line, col);
			} break;
			case '.': {
				tok_stream->tokens[token_list_size++] = create_token(TT_OUTPUT, line, col); 
			} break;
			case ',': {
				tok_stream->tokens[token_list_size++] = create_token(TT_INPUT, line, col); 

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
		token *tmp = realloc(tok_stream->tokens, token_list_size * sizeof(token));
		if (tmp) tok_stream->tokens = tmp;
	} else {
		free(tok_stream->tokens);
		tok_stream->tokens = NULL;
	}
	
	tok_stream->length = token_list_size;

	return tok_stream;
}

void delete_program(bf_program **pprogram) {
	if (!pprogram || !*pprogram) return;

	free((*pprogram)->path);
	free((*pprogram)->buffer);
	free(*pprogram);

	*pprogram = NULL;
}

ssize_t *parse_jump_table(const token_stream *tok_stream) {
	size_t n = tok_stream->length;
	if (n == 0) return calloc(1, sizeof(ssize_t));

	const token *toks = tok_stream->tokens;

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

void destroy_jump_table(ssize_t **pjump_table) {
	if (!pjump_table || !*pjump_table) return;

	free(*pjump_table);

	*pjump_table = NULL;
}

int main(int argc, char** argv) {
	if (argc < 2) {
    		fprintf(stderr, "Usage: %s <file.bf>\n", argv[0]);

    		return EXIT_FAILURE;
	}

	int ret = EXIT_FAILURE;

	const char *file_handle_path = argv[1];
	bf_program *program = create_program(file_handle_path);
	if (program == NULL) {
		fprintf(stderr, "Error: Critical error during reading of file '%s'!\n", file_handle_path);
		
		goto end;
	}
	
	token_stream *tok_stream = lex_program(program);
	if (tok_stream == NULL) {
		fprintf(stderr, "Error: Critical error during lexer tokenization!\n");
		
		delete_program(&program);
		
		goto end;
	}

	ssize_t *jump_table = parse_jump_table(tok_stream);
	if (jump_table == NULL) {
		fprintf(stderr, "Error: Mismatched brackets\n");
		
		destroy_token_stream(&tok_stream);
		delete_program(&program);

		goto end;
	}

	ret = EXIT_SUCCESS;

	destroy_jump_table(&jump_table);
	destroy_token_stream(&tok_stream);
	delete_program(&program);

end:
	return ret;
}
