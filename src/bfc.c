#include "bfc_cli.h"
#include "bfc_codegen.h"
#include "bfc_error.h"
#include "bfc_io.h"
#include "bfc_ir.h"
#include "bfc_jumptable.h"
#include "bfc_lexer.h"

#include <stdlib.h>

#define CHECK_ERROR(err)                                     \
    if (err.code != ERR_OK)                                  \
    {                                                        \
        bfc_log_error(err, (struct bfc_program_t*) program); \
        goto end;                                            \
    }

int main(int argc, char** argv)
{
    int ret = EXIT_FAILURE;

    bfc_args_t cmd_args = {0};

    [[gnu::cleanup(bfc_program_destroy)]]
    bfc_program_t* program = nullptr;

    [[gnu::cleanup(bfc_token_stream_destroy)]]
    bfc_token_stream_t* tok_stream = nullptr;

    [[gnu::cleanup(bfc_jump_table_destroy)]]
    ssize_t* jump_table = nullptr;

    [[gnu::cleanup(bfc_ir_destroy)]]
    bfc_ir_block_t* root_block = nullptr;

    [[gnu::cleanup(bfc_asm_destroy)]]
    bfc_asm_t* asm_prog = nullptr;

    bfc_error_t err;

    err = bfc_process_args(&cmd_args, argc, argv);
    CHECK_ERROR(err);

    if (cmd_args.ask_help)
    {
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
    return ret;
}
