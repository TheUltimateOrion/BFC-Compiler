#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <sys/types.h>

#define COL_OFF     "\033[m"
#define COL_INFO    "\033[1;1m"
#define COL_ERROR   "\033[1;31m"

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
	IR_ADD,
	IR_MOVE,
	IR_PUT,
	IR_GET,
	IR_SET,
	IR_LOOP,
} bfc_ir_token_type_t;

typedef enum {
	BFC_ERR_OK = 0,
	BFC_ERR_ARGS,
	BFC_ERR_IO,
	BFC_ERR_MISMATCHED_BRACKET,
	BFC_ERR_MISSING_BRACKET,
	BFC_ERR_ALLOC
} bfc_err_code_t;

typedef struct {
	union {
		struct {
			uint8_t do_assemble   : 1;
			uint8_t ask_help      : 1;
		};
		uint8_t flags;
	};
	char *input;
	char *outputs[UINT8_MAX];
} bfc_args_t;

typedef struct {
	bfc_token_type_t type;
size_t line;
	size_t col;
} bfc_token_t;

typedef struct {
	bfc_token_t *tokens;
	size_t length;
} bfc_token_stream_t;

typedef struct {
	char *path;
	char *buffer;
	size_t file_size;
	size_t line_count;
} bfc_program_t;

struct bfc_ir_block;

typedef struct {
    bfc_ir_token_type_t op;

    union {
        int32_t imm;
        struct bfc_ir_block *body;
    } val;
} bfc_ir_instr_t;

typedef struct {
	bfc_ir_instr_t *instr;

	size_t length;
	size_t capacity;
} bfc_ir_block_t;

typedef struct {
	bfc_err_code_t code;
	char msg[512];
	bfc_token_t token;
} bfc_error_t;

#define BFC_OK ((bfc_error_t) { .code = BFC_ERR_OK, .msg = {0}, .token = {0} })

void bfc_print_help_info(void);

bfc_error_t bfc_make_error(const bfc_err_code_t error_code, const char *msg);
bfc_error_t bfc_make_error_with_token(const bfc_err_code_t error_code, const char *msg, bfc_token_t token);
const char *bfc_get_error_code(const bfc_err_code_t error_code);
void bfc_log_error(const bfc_error_t err, const bfc_program_t *program);

bfc_error_t bfc_process_args(int argc, char **argv, bfc_args_t *cmd_args);

bfc_error_t bfc_program_create(const char *file_path, bfc_program_t **program);
void bfc_program_destroy(bfc_program_t **pprogram);
const char *bfc_program_getname(const bfc_program_t *program);
char *bfc_program_getline(const bfc_program_t *program, size_t n);

bfc_token_t bfc_make_token(bfc_token_type_t tok_type, uint32_t line, uint32_t col); 
bfc_error_t bfc_lex(const bfc_program_t *program, bfc_token_stream_t **token_stream);
void bfc_destroy_token_stream(bfc_token_stream_t **ptok_stream);

bfc_error_t bfc_parse_jump_table(const bfc_token_stream_t *tok_stream, ssize_t **jump_table);
void bfc_destroy_jump_table(ssize_t **pjump_table);

bfc_error_t bfc_ir_create(const bfc_token_stream_t *tok_stream, const ssize_t *jump_table, bfc_ir_block_t **root_block);
void bfc_ir_destroy(bfc_ir_block_t **root_block);

int main(int argc, char** argv) {
	int ret = EXIT_FAILURE;


	bfc_args_t cmd_args = {0};

	bfc_program_t *program = NULL;
	bfc_token_stream_t *tok_stream = NULL;
	ssize_t *jump_table = NULL;
	bfc_ir_block_t *root_block = NULL;

	bfc_error_t err;

	err = bfc_process_args(argc, argv, &cmd_args);
	if (err.code != BFC_ERR_OK) {
		bfc_log_error(err, NULL);

		goto end;
	}

	if (cmd_args.ask_help) {
		bfc_print_help_info();
		goto end;
	}

	err = bfc_program_create(cmd_args.input, &program);
	if (err.code != BFC_ERR_OK) {
		bfc_log_error(err, NULL);
		
		goto fail;
	}
	
	err = bfc_lex(program, &tok_stream);
	if (err.code != BFC_ERR_OK) {
		bfc_log_error(err, program);	
			
		goto fail;
	}

	err = bfc_parse_jump_table(tok_stream, &jump_table);
	if (err.code != BFC_ERR_OK) {
		bfc_log_error(err, program);

		goto fail;
	}

	err = bfc_ir_create(tok_stream, jump_table, &root_block);
	if (err.code != BFC_ERR_OK) {
		bfc_log_error(err, program);

		goto fail;
	}


	ret = EXIT_SUCCESS;

fail:
	if (jump_table) bfc_destroy_jump_table(&jump_table);
	if (tok_stream) bfc_destroy_token_stream(&tok_stream);
	if (program) bfc_program_destroy(&program);
	if (root_block) bfc_ir_destroy(&root_block);

end:
	return ret;
}

void bfc_print_help_info(void) {
	printf("OVERVIEW: bfc Brainfuck compiler\n\n");
	printf("USAGE: bfc [options] <file.bf>\n\n");
	printf("OPTIONS:\n");
	printf("  %-20s %s\n", "--help / -h", "Display available options");
	printf("  %-20s %s\n", "-o <file>",   "Write output to <file>");
	printf("  %-20s %s\n", "-S",          "Only run compilation steps");
	
}

bfc_error_t bfc_make_error(const bfc_err_code_t error_code, const char *msg) {
    bfc_error_t err = {0};

    err.code = error_code;

    if (msg) snprintf(err.msg, sizeof(err.msg), "%s", msg);

    return err;
}

bfc_error_t bfc_make_error_with_token(const bfc_err_code_t error_code, const char *msg, bfc_token_t token) {
    bfc_error_t err = {0};

    err.code = error_code;
    err.token = token;

    if (msg) snprintf(err.msg, sizeof(err.msg), "%s", msg);

    return err;
}

const char *bfc_get_error_code(const bfc_err_code_t error_code) {
	switch (error_code) {
		case BFC_ERR_OK: {
			return "ERROR_OK";
		} break;
		case BFC_ERR_ARGS: {
			return "ERROR_ARGS";
		} break;
		case BFC_ERR_ALLOC: {
			return "ERROR_ALLOC";
		} break;
		case BFC_ERR_IO: {
			return "ERROR_IO";
		} break;
		case BFC_ERR_MISMATCHED_BRACKET: {
			return "ERROR_MISMATCHED_BRACKET";
		} break;
		case BFC_ERR_MISSING_BRACKET: {
			return "ERROR_MISSING_BRACKET";
		} break;
		default: {
			return "Unknown error";
		} break;
	}
}

void bfc_log_error(const bfc_error_t err, const bfc_program_t *program) {
	if (err.code == BFC_ERR_MISSING_BRACKET || err.code == BFC_ERR_MISMATCHED_BRACKET) {
		fprintf(stderr, COL_INFO "%s[%lu, %lu]: " COL_ERROR "%s" COL_OFF COL_INFO ": %s\n" COL_OFF, bfc_program_getname(program), err.token.line, err.token.col, bfc_get_error_code(err.code), err.msg);

	
		char *line_buf = bfc_program_getline(program, (size_t)err.token.line);

		int line_num_width = (err.token.line > 0) ? (int)log10(err.token.line) + 1 : 1;

		fprintf(stderr, "   %lu | %s\n", (size_t) err.token.line, line_buf);
		fprintf(stderr, "   %*s | %*c\n", line_num_width, "", (int)err.token.col, '^');
		
		free(line_buf);
		return;
	}

	fprintf(stderr, COL_INFO "bfc: " COL_ERROR "%s" COL_OFF COL_INFO ": %s\n" COL_OFF, bfc_get_error_code(err.code), err.msg);
}

bfc_error_t bfc_process_args(int argc, char **argv, bfc_args_t *cmd_args) {
	cmd_args->flags = 0;
	cmd_args->input = "";
	
	int i = 1;
	uint8_t output_num = 0;
	while (i < argc) {
		if (strcmp(argv[i], "-o") == 0) {
			if (i == argc - 1) return bfc_make_error(BFC_ERR_ARGS, "Argument to '-o' is missing (expected 1 value)");

			cmd_args->outputs[output_num] = argv[i + 1];
			++output_num;
		} else if (strcmp(argv[i], "-S") == 0) {
			cmd_args->do_assemble = 0x1;
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			cmd_args->ask_help = 0x1;
			
			return BFC_OK;
		} else if (argv[i][0] == '-') {
			char err_str[512];
			snprintf(err_str, sizeof(err_str), "Unknown argument: '%s'", argv[i]);

			return bfc_make_error(BFC_ERR_ARGS, err_str);
		} else {
			if (strcmp(cmd_args->input, "") != 0) return bfc_make_error(BFC_ERR_ARGS, "Too many input file paths given!");

			cmd_args->input = argv[i];
		}
		++i;
	}

	if (strcmp(cmd_args->input, "") == 0) return bfc_make_error(BFC_ERR_ARGS, "No input files!");

	return BFC_OK; 
}

bfc_error_t bfc_program_create(const char *file_path, bfc_program_t **program) {
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

		prog->line_count = 0;
		size_t i = 0;
		while (prog->buffer[i] != '\0') {
			if (prog->buffer[i] == '\n') ++prog->line_count;
			++i;
		}
		
		*program = prog;
		return BFC_OK;
	}


	char err_str[512];
	snprintf(err_str, sizeof(err_str), "No such file or directory: '%s'", file_path);
	return bfc_make_error(BFC_ERR_IO, err_str);;

}

void bfc_program_destroy(bfc_program_t **pprogram) {
	if (!pprogram || !*pprogram) return;

	free((*pprogram)->path);
	free((*pprogram)->buffer);
	free(*pprogram);

	*pprogram = NULL;
}

const char *bfc_program_getname(const bfc_program_t *program) {
	const char *start = program->path;
	const char *end = program->path;
	while (end != NULL) {
		end = strchr(start, '/');
	}

	return start;
}

char *bfc_program_getline(const bfc_program_t *program, size_t n) {
	if (n > program->line_count) return NULL;

	size_t current_line = 1;
	const char *start = program->buffer;
	const char *end = program->buffer;

	while (current_line < n) {
		end = strchr(start, '\n');
		if (end == NULL) return NULL;

		start = end + 1;
		++current_line;
	}

	end = strchr(start, '\n');

	size_t line_len;
	if (end == NULL) {
        	line_len = strlen(start);
	} else {
		line_len = end - start;
	}

	if (line_len > 4096) return NULL;

	char *line_buf = (char*) malloc(4096 * sizeof(char));

	strncpy(line_buf, start, line_len);
	line_buf[line_len] = '\0';

	return line_buf;
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
		return BFC_OK;
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

	return BFC_OK;
}

bfc_error_t bfc_parse_jump_table(const bfc_token_stream_t *tok_stream, ssize_t **jump_table) {
	bfc_error_t err;
	char err_str[512];

	size_t n = tok_stream->length;
	if (n == 0) {
		*jump_table = calloc(1, sizeof(ssize_t));

		return BFC_OK;
	}

	const bfc_token_t *toks = tok_stream->tokens;

	ssize_t *jtable = malloc(n * sizeof(ssize_t));
	if (!jtable) return bfc_make_error(BFC_ERR_ALLOC, "Memory allocation failure!");

	for (size_t i = 0; i < n; ++i) jtable[i] = -1;

	size_t *stack = malloc(n * sizeof(size_t));
	if (!stack) { 
		free(jtable);

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

			jtable[j] = (ssize_t) i;
			jtable[i] = (ssize_t) j;
		}
	}
	
	if (sp != 0) goto missing_closing_bracket;

	free(stack);

	*jump_table = jtable;
	return BFC_OK;

extra_closing_bracket:
	snprintf(err_str, sizeof(err_str), "Found an extra ']' at line %lu.", toks[i].line);

	err = bfc_make_error_with_token(BFC_ERR_MISMATCHED_BRACKET, err_str, toks[i]);

	free(stack);
	free(jtable);
	
	return err;

missing_closing_bracket:
	snprintf(err_str, sizeof(err_str), "Missing a closing bracket ']' for opening bracket '[' at line %lu.", toks[stack[sp - 1]].line);
	
	err = bfc_make_error_with_token(BFC_ERR_MISSING_BRACKET, err_str, toks[stack[sp - 1]]);

	free(stack);
	free(jtable);

	return err;
}

void bfc_destroy_jump_table(ssize_t **pjump_table) {
	if (!pjump_table || !*pjump_table) return;

	free(*pjump_table);

	*pjump_table = NULL;
}

bfc_error_t bfc_ir_create(const bfc_token_stream_t *tok_stream, const ssize_t *jump_table, bfc_ir_block_t **root_block) {
	// TODO
	
	return BFC_OK;
}
void bfc_ir_destroy(bfc_ir_block_t **root_block) {
	if (!root_block || !*root_block) return;

	free(*root_block);

	*root_block = NULL;
}
