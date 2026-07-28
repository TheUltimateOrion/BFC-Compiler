#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bfc_cli.h"
#include "bfc_codegen.h"
#include "bfc_error.h"
#include "bfc_io.h"
#include "bfc_ir.h"
#include "bfc_jumptable.h"
#include "bfc_lexer.h"
#include "bfc_target.h"

#define CHECK_ERROR(error_)                 \
    do                                      \
    {                                       \
        if ((error_).code != ERR_OK)        \
        {                                   \
            bfc_log_error(error_, program); \
            goto end;                       \
        }                                   \
    }                                       \
    while (0)

int main(int argc, char** argv)
{
    int ret = EXIT_FAILURE;

    bfc_args_t cmd_args = {0};

    [[gnu::cleanup(bfc_program_destroy)]]
    bfc_program_t* program = nullptr;

    [[gnu::cleanup(bfc_token_stream_destroy)]]
    bfc_token_stream_t* tok_stream = nullptr;

    [[gnu::cleanup(bfc_jump_table_destroy)]]
    int64_t* jump_table = nullptr;

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
        ret = EXIT_SUCCESS;
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

    bfc_target_t target;

    if (cmd_args.target)
    {
        err = bfc_target_parse(&target, cmd_args.target);

        CHECK_ERROR(err);
    }
    else
    {
        target = bfc_target_host();
    }

    err = bfc_codegen(&asm_prog, root_block, target);

    CHECK_ERROR(err);

    if (cmd_args.do_assemble)
    {
        char output_path[4096];

        const int length = snprintf(
            output_path, sizeof(output_path), cmd_args.output ? "%s" : "%s.s",
            cmd_args.output ? cmd_args.output : cmd_args.input
        );

        if (length < 0 || (size_t) length >= sizeof(output_path))
        {
            err = bfc_make_error(ERR_ARGS, "Output path is too long");
            CHECK_ERROR(err);
        }

        err = bfc_asm_write_file(asm_prog, output_path);
        CHECK_ERROR(err);
    }

    ret = EXIT_SUCCESS;

end:
    return ret;
}
