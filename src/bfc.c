#include "bfc_cli.h"
#include "bfc_codegen.h"
#include "bfc_error.h"
#include "bfc_io.h"
#include "bfc_ir.h"
#include "bfc_jumptable.h"
#include "bfc_lexer.h"

#include <stdlib.h>

#define CHECK_ERROR(err)            \
	if (err.code != ERR_OK) {            \
		bfc_log_error(err, (struct bfc_program_t*) program); \
		goto end;                    \
	}


int main(int argc, char** argv) {

	int ret = EXIT_FAILURE;

	bfc_args_t cmd_args = {0};

	bfc_program_t *program         = NULL;
	bfc_token_stream_t *tok_stream = NULL;
	ssize_t *jump_table            = NULL;
	bfc_ir_block_t *root_block     = NULL;
	bfc_asm_t *asm_prog            = NULL;

	bfc_error_t err;

	err = bfc_process_args(&cmd_args, argc, argv);
	CHECK_ERROR(err);

	if (cmd_args.ask_help) {
		bfc_cmd_help();
		goto end;
	}

	err = bfc_program_create(&program, cmd_args.input);
	CHECK_ERROR(err);
	
	err = bfc_lex(&tok_stream, program, cmd_args);
	CHECK_ERROR(err);

	err = bfc_parse_jump_table(&jump_table, tok_stream);
	CHECK_ERROR(err);

	err = bfc_ir_create(&root_block, tok_stream);
	CHECK_ERROR(err);

	err = bfc_ir_optimize_rep(&root_block);
	CHECK_ERROR(err);

	err = bfc_codegen(&asm_prog, root_block);
	CHECK_ERROR(err);

	ret = EXIT_SUCCESS;

end:
	if (program)    bfc_program_destroy(&program);
	if (tok_stream) bfc_token_stream_destroy(&tok_stream);
	if (jump_table) bfc_jump_table_destroy(&jump_table);
	if (root_block) bfc_ir_destroy(&root_block);
	if (asm_prog)   bfc_asm_destroy(&asm_prog);

	return ret;
}
