#include "bfc_codegen_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bfc_error_t bfc_codegen_emit_text(bfc_asm_t* asm_prog, const char* text)
{
    const size_t text_length = strlen(text);
    const size_t required    = asm_prog->length + text_length + 1;

    if (required > asm_prog->capacity)
    {
        size_t new_capacity = asm_prog->capacity;

        while (new_capacity < required)
        {
            new_capacity *= 2;
        }

        char* new_buffer = realloc(asm_prog->buffer, new_capacity);

        if (!new_buffer)
        {
            return BFC_ERR_ALLOC;
        }

        asm_prog->buffer   = new_buffer;
        asm_prog->capacity = new_capacity;
    }

    memcpy(asm_prog->buffer + asm_prog->length, text, text_length + 1);

    asm_prog->length += text_length;

    return BFC_ERR_OK;
}

static bfc_error_t emit_loop(bfc_asm_t* asm_prog, const bfc_ir_block_t* body)
{
    const size_t id = asm_prog->label_id++;

    char start_label[64];
    char end_label[64];
    char label_line[80];

    snprintf(start_label, sizeof(start_label), ".L_bfc_loop_%zu", id);

    snprintf(end_label, sizeof(end_label), ".L_bfc_loop_%zu_end", id);

    snprintf(label_line, sizeof(label_line), "%s:\n", start_label);

    bfc_error_t err = bfc_codegen_emit_text(asm_prog, label_line);

    if (err.code == ERR_OK)
    {
        err = asm_prog->backend->emit_loop_test_z(asm_prog, end_label);
    }

    if (err.code == ERR_OK)
    {
        err = bfc_codegen_emit_block(asm_prog, body);
    }

    if (err.code == ERR_OK)
    {
        err = asm_prog->backend->emit_loop_test_nz(asm_prog, start_label);
    }

    if (err.code == ERR_OK)
    {
        snprintf(label_line, sizeof(label_line), "%s:\n", end_label);

        err = bfc_codegen_emit_text(asm_prog, label_line);
    }

    return err;
}

bfc_error_t bfc_codegen_emit_block(bfc_asm_t* asm_prog, const bfc_ir_block_t* ir_block)
{
    for (size_t i = 0; i < ir_block->length; ++i)
    {
        const bfc_ir_instr_t* instr = &ir_block->instr[i];
        bfc_error_t           err;

        switch (instr->op)
        {
            case IR_ADD: err = asm_prog->backend->emit_op_add(asm_prog, instr->val.imm); break;

            case IR_MOVE: err = asm_prog->backend->emit_op_move(asm_prog, instr->val.imm); break;

            case IR_GET: err = asm_prog->backend->emit_op_get(asm_prog); break;

            case IR_PUT: err = asm_prog->backend->emit_op_put(asm_prog); break;

            case IR_SET: err = asm_prog->backend->emit_op_set(asm_prog, instr->val.imm); break;

            case IR_LOOP: err = emit_loop(asm_prog, (const bfc_ir_block_t*) instr->val.body); break;

            default: return bfc_make_error(ERR_INTERNAL, "Unknown IR instruction");
        }

        if (err.code != ERR_OK)
        {
            return err;
        }
    }

    return BFC_ERR_OK;
}

bfc_error_t bfc_codegen(bfc_asm_t** out_asm, const bfc_ir_block_t* ir_block)
{
    *out_asm = nullptr;

    const bfc_backend_t* backend = bfc_backend_select();

    if (!backend)
    {
        return bfc_make_error(ERR_INTERNAL, "Unsupported code generation target");
    }

    bfc_asm_t* asm_prog = calloc(1, sizeof(*asm_prog));

    if (!asm_prog)
    {
        return BFC_ERR_ALLOC;
    }

    asm_prog->capacity = 4096;
    asm_prog->backend  = backend;
    asm_prog->buffer   = malloc(asm_prog->capacity);

    if (!asm_prog->buffer)
    {
        free(asm_prog);
        return BFC_ERR_ALLOC;
    }

    asm_prog->buffer[0] = '\0';

    bfc_error_t err = backend->emit_header(asm_prog);

    if (err.code == ERR_OK)
    {
        err = backend->emit_data_section(asm_prog);
    }

    if (err.code == ERR_OK)
    {
        err = backend->emit_symbol(asm_prog);
    }

    if (err.code == ERR_OK)
    {
        err = bfc_codegen_emit_block(asm_prog, ir_block);
    }

    if (err.code == ERR_OK)
    {
        err = backend->emit_end(asm_prog);
    }

    if (err.code != ERR_OK)
    {
        bfc_asm_destroy(&asm_prog);
        return err;
    }

    *out_asm = asm_prog;
    return BFC_ERR_OK;
}

void bfc_asm_destroy(bfc_asm_t** pasm_prog)
{
    if (!pasm_prog || !*pasm_prog)
    {
        return;
    }

    free((*pasm_prog)->buffer);
    free(*pasm_prog);

    *pasm_prog = nullptr;
}

const bfc_backend_t* bfc_backend_select(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    return bfc_backend_x86_64();
#elif defined(__i386__) || defined(_M_IX86)
    return bfc_backend_i386();
#elif defined(__aarch64__) || defined(_M_ARM64)
    return bfc_backend_aarch64();
#elif defined(__arm__) || defined(_M_ARM)
    return bfc_backend_arm32();
#else
    return nullptr;
#endif
}

bfc_error_t bfc_asm_write_file(const bfc_asm_t* asm_prog, const char* path)
{
    FILE* file = fopen(path, "wb");

    if (!file)
    {
        return bfc_make_error(ERR_IO, "Could not open assembly output file");
    }

    const size_t written = fwrite(asm_prog->buffer, 1, asm_prog->length, file);

    if (written != asm_prog->length)
    {
        fclose(file);

        return bfc_make_error(ERR_IO, "Could not write complete assembly output");
    }

    if (fclose(file) != 0)
    {
        return bfc_make_error(ERR_IO, "Could not close assembly output file");
    }

    return BFC_ERR_OK;
}
