#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
	TT_INC,
	TT_DEC,
	TT_PTR_RIGHT,
	TT_PTR_LEFT,
	TT_LOOP_START,
	TT_LOOP_END,
	TT_OUTPUT,
	TT_INPUT
} token;

typedef struct {
	token *tokens;
	size_t tokens_length;
} token_stream;

void destroy_token_stream(token_stream *tok_stream) {
	free(tok_stream->tokens);
	free(tok_stream);
	tok_stream = NULL;
}

typedef struct {
	char *buffer;
	long file_size;
} bf_program;

bf_program *open_program(const char *file_path) {
	FILE *prog_file;

	if((prog_file = fopen(file_path, "rb"))) {
		bf_program *prog = (bf_program*) malloc(sizeof(bf_program));

		fseek(prog_file, 0, SEEK_END);
		long file_size = ftell(prog_file);
		rewind(prog_file);
	
		prog->buffer = (char*) malloc((file_size + 1) * sizeof(char));
		prog->file_size = file_size;
		
		if (prog->buffer) {
			fread(prog->buffer, sizeof(char), prog->file_size, prog_file);
			prog->buffer[file_size] = '\0';
		}

		fclose(prog_file);

		return prog;
	}

	return NULL;
}

token_stream *lex_program(bf_program *prog) {
	token_stream *tok_stream = (token_stream*) malloc(sizeof(token_stream));
	tok_stream->tokens = (token*) malloc(prog->file_size * sizeof(token));
	size_t token_list_size = 0;
	size_t buffer_index = 0;

	while (prog->buffer[buffer_index] != '\0') {
		switch (prog->buffer[buffer_index]) {
			case '+': {
				tok_stream->tokens[token_list_size++] = TT_INC;
			} break;
			case '-': {
				tok_stream->tokens[token_list_size++] = TT_DEC;
			} break;
			case '>': {
				tok_stream->tokens[token_list_size++] = TT_PTR_RIGHT;
			} break;
			case '<': {
				tok_stream->tokens[token_list_size++] = TT_PTR_LEFT;
			} break;
			case '[': {
				tok_stream->tokens[token_list_size++] = TT_LOOP_START;
			} break;
			case ']': {
				tok_stream->tokens[token_list_size++] = TT_LOOP_END;
			} break;
			case '.': {
				tok_stream->tokens[token_list_size++] = TT_OUTPUT;
			} break;
			case ',': {
				tok_stream->tokens[token_list_size++] = TT_INPUT;
			} break;
			default: break;
		}

		++buffer_index;
	}

	if (token_list_size > 0) {
		token *tmp = realloc(tok_stream->tokens, token_list_size * sizeof(token));
		if (tmp != NULL) tok_stream->tokens = tmp;
	} else {
		free(tok_stream->tokens);
		tok_stream->tokens = NULL;
	}
	
	tok_stream->tokens_length = token_list_size;

	return tok_stream;
}

void close_program(bf_program *prog) {
	free(prog->buffer);
	free(prog);
	prog = NULL;
}

int main(int argc, char** argv) {
	const char *prog_file_path = argv[1];
	bf_program *prog = open_program(prog_file_path);
	if (prog == NULL) {
		printf("Error: File '%s' does not exist!\n", prog_file_path);
		return EXIT_FAILURE;
	}
	
	token_stream *tok_stream = lex_program(prog);

	destroy_token_stream(tok_stream);
	close_program(prog);

	return EXIT_SUCCESS;
}
