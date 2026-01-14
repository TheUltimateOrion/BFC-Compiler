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
	char *buffer;
	long file_size;
} brainfuck_program;


brainfuck_program *open_program(const char *file_path) {
	FILE *prog_file;

	if((prog_file = fopen(file_path, "rb"))) {
		brainfuck_program *prog = (brainfuck_program*) malloc(sizeof(brainfuck_program));

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

token *lex_program(brainfuck_program *prog) {
	token *program_tokens = malloc(prog->file_size * sizeof(token));
	size_t token_list_size = 0;
	size_t buffer_index = 0;

	while (prog->buffer[buffer_index] != '\0') {
		switch (prog->buffer[buffer_index]) {
			case '+': {
				program_tokens[token_list_size++] = TT_INC;
			} break;
			case '-': {
				program_tokens[token_list_size++] = TT_DEC;
			} break;
			case '>': {
				program_tokens[token_list_size++] = TT_PTR_RIGHT;
			} break;
			case '<': {
				program_tokens[token_list_size++] = TT_PTR_LEFT;
			} break;
			case '[': {
				program_tokens[token_list_size++] = TT_LOOP_START;
			} break;
			case ']': {
				program_tokens[token_list_size++] = TT_LOOP_END;
			} break;
			case '.': {
				program_tokens[token_list_size++] = TT_OUTPUT;
			} break;
			case ',': {
				program_tokens[token_list_size++] = TT_INPUT;
			} break;
			default: break;
		}

		++buffer_index;
	}

	if (token_list_size > 0) {
		token *tmp = realloc(program_tokens, token_list_size * sizeof(token));
		if (tmp != NULL) program_tokens = tmp;
	} else {
		free(program_tokens);
		program_tokens = NULL;
	}

	return program_tokens;
}

void close_program(brainfuck_program *prog) {
	free(prog->buffer);
	free(prog);
	prog = NULL;
}


char *buffer;

int main(int argc, char** argv) {
	const char *prog_file_path = argv[1];
	brainfuck_program *prog = open_program(prog_file_path);
	if(prog == NULL) {
		printf("Error: File '%s' does not exist!\n", prog_file_path);
		return EXIT_FAILURE;
	}
	
	token *program_tokens = lex_program(prog);

	free(program_tokens);

	close_program(prog);

	return EXIT_SUCCESS;
}
