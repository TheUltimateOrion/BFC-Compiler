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

void destroy_token_stream(token_stream *tok_stream) {
	free(tok_stream->tokens);
	free(tok_stream);
	tok_stream = NULL;
}

typedef struct {
	char *program_path;
	char *buffer;
	long file_size;
} bf_program;


bf_program *create_program(const char *file_path) {
	FILE *prog_file;

	if((prog_file = fopen(file_path, "rb"))) {
		bf_program *prog = (bf_program*) malloc(sizeof(bf_program));

		fseek(prog_file, 0, SEEK_END);
		long file_size = ftell(prog_file);
		rewind(prog_file);
	
		prog->buffer = (char*) malloc((file_size + 1) * sizeof(char));
		prog->file_size = file_size;
		prog->program_path = (char*) malloc(512 * sizeof(char));
		strcpy(prog->program_path, file_path);
		
		if (prog->buffer) {
			fread(prog->buffer, sizeof(char), prog->file_size, prog_file);
			prog->buffer[file_size] = '\0';
		}

		fclose(prog_file);

		return prog;
	}

	return NULL;
}

token_stream *lex_program(const bf_program *prog) {
	token_stream *tok_stream = (token_stream*) malloc(sizeof(token_stream));
	tok_stream->tokens = (token*) malloc(prog->file_size * sizeof(token));
	size_t token_list_size = 0;
	size_t buffer_index = 0;

	uint32_t line = 0;
	uint32_t col = 0;

	while (prog->buffer[buffer_index] != '\0') {
		switch (prog->buffer[buffer_index]) {
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
		if (tmp != NULL) tok_stream->tokens = tmp;
	} else {
		free(tok_stream->tokens);
		tok_stream->tokens = NULL;
	}
	
	tok_stream->length = token_list_size;

	return tok_stream;
}

void delete_program(bf_program *prog) {
	free(prog->program_path);
	free(prog->buffer);
	free(prog);
	prog = NULL;
}

int64_t *parse_jump_table(const token_stream *tok_stream) {
	uint32_t n = tok_stream->length;
	const token *toks = tok_stream->tokens;

	int64_t *jump_table = malloc(n * sizeof(int64_t));
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
			jump_table[j] = (int64_t) i;
			jump_table[i] = (int64_t) j;
		}
	}

	free(stack);

	if (sp != 0) {
		free(jump_table); 
		return NULL;
	}

	return jump_table;
}

void destroy_jump_table(int64_t *jump_table) {
	free(jump_table);
	jump_table = NULL;
}

int main(int argc, char** argv) {
	(void) argc;
	(void) argv;

	const char *prog_file_path = argv[1];
	bf_program *program = create_program(prog_file_path);
	if (program == NULL) {
		printf("Error: File '%s' does not exist!\n", prog_file_path);
		return EXIT_FAILURE;
	}
	
	token_stream *tok_stream = lex_program(program);

	int64_t *jump_table = parse_jump_table(tok_stream);
	if (jump_table == NULL) {
		printf("Error: Mismatched brackets\n");
		return EXIT_FAILURE;
	}
	
	destroy_jump_table(jump_table);
	destroy_token_stream(tok_stream);
	delete_program(program);

	return EXIT_SUCCESS;
}
