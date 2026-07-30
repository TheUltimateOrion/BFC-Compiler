/*
 * Generic assembly generation.
 *
 * This module owns backend selection, recursive IR traversal, assembly-buffer
 * management, formatted emission, and assembly-file output. Architecture- and
 * OS-specific instruction syntax belongs in backend modules.
 */

#include "bfc_codegen_internal.h"

#include <stdarg.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bfc_config.h"
#include "bfc_memory.h"

/*
 * Resolve the exact architecture/OS pair to an immutable callback table.
 * Return nullptr when the requested backend is not compiled into this build.
 */
[[gnu::pure]]
static const bfc_backend_t* bfc_backend_select(bfc_target_t target)
{
    if (target.arch == BFC_ARCH_AARCH64 && target.os == BFC_OS_MACOS)
    {
        return &BFC_BACKEND_MACOS_AARCH64;
    }

    if (target.arch == BFC_ARCH_X86_64 && target.os == BFC_OS_MACOS)
    {
        return &BFC_BACKEND_MACOS_X86_64;
    }

    return nullptr;
}

/*
 * Emit loop structure generically while delegating branch syntax to the
 * backend. label_id guarantees unique labels across nested and sibling loops.
 */
[[gnu::nonnull(1, 2)]]
static bfc_error_t bfc_codegen_emit_loop(bfc_asm_t* asm_prog, const bfc_ir_block_t* body)
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

/*
 * Append text while preserving two invariants:
 *   - length excludes the trailing null byte
 *   - buffer[length] is always '\0'
 *
 * Size arithmetic is checked before geometrically growing the byte buffer.
 */
bfc_error_t bfc_codegen_emit_text(bfc_asm_t* asm_prog, const char* text)
{
    const size_t text_length = strlen(text);
    size_t       required;

    if (ckd_add(&required, asm_prog->length, text_length) || ckd_add(&required, required, 1))
    {
        return bfc_make_error(ERR_ALLOC, "Assembly buffer size overflow");
    }

    if (required > asm_prog->capacity)
    {
        size_t new_capacity = asm_prog->capacity;

        while (new_capacity < required)
        {
            size_t doubled_capacity;

            if (ckd_mul(&doubled_capacity, new_capacity, 2))
            {
                return bfc_make_error(ERR_ALLOC, "Assembly buffer capacity overflow");
            }

            new_capacity = doubled_capacity;
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

/*
 * Traverse one IR block and dispatch each operation through the selected
 * backend. IR_LOOP recurses through the generic loop emitter above.
 */
bfc_error_t bfc_codegen_emit_block(bfc_asm_t* asm_prog, const bfc_ir_block_t* ir_block)
{
    for (size_t i = 0; i < ir_block->length; ++i)
    {
        const bfc_ir_instr_t* instr = &ir_block->instructions[i];
        bfc_error_t           err;

        switch (instr->op)
        {
            case IR_ADD: err = asm_prog->backend->emit_op_add(asm_prog, instr->val.imm); break;

            case IR_MOVE: err = asm_prog->backend->emit_op_move(asm_prog, instr->val.imm); break;

            case IR_GET: err = asm_prog->backend->emit_op_get(asm_prog); break;

            case IR_PUT: err = asm_prog->backend->emit_op_put(asm_prog); break;

            case IR_SET: err = asm_prog->backend->emit_op_set(asm_prog, instr->val.imm); break;

            case IR_LOOP: err = bfc_codegen_emit_loop(asm_prog, instr->val.body); break;

            default: return bfc_make_error(ERR_INTERNAL, "Unknown IR instruction");
        }

        if (err.code != ERR_OK)
        {
            return err;
        }
    }

    return BFC_ERR_OK;
}

/*
 * Format one assembly fragment into a bounded temporary buffer before
 * appending it. The format attribute lets the compiler validate call sites.
 */
[[gnu::nonnull(1, 2), gnu::format(printf, 2, 3)]]
bfc_error_t bfc_codegen_emitf(bfc_asm_t* asm_prog, const char* format, ...)
{
    char buffer[256];

    va_list args;
    va_start(args, format);

    const int length = vsnprintf(buffer, sizeof(buffer), format, args);

    va_end(args);

    if (length < 0 || (size_t) length >= sizeof(buffer))
    {
        return bfc_make_error(ERR_INTERNAL, "Formatted assembly text is too long");
    }

    return bfc_codegen_emit_text(asm_prog, buffer);
}

/*
 * Allocate and populate an assembly object.
 *
 * Ownership is transferred to *out_asm only after every emission stage
 * succeeds. Any intermediate failure destroys the partially built object.
 */
bfc_error_t bfc_codegen(bfc_asm_t** out_asm, const bfc_ir_block_t* ir_block, bfc_target_t target)
{
    *out_asm = nullptr;

    const bfc_backend_t* backend = bfc_backend_select(target);

    if (!backend)
    {
        return bfc_make_error(ERR_ARGS, "No backend is available for the requested target");
    }

    bfc_asm_t* asm_prog = nullptr;
    asm_prog            = BFC_CALLOC_ARRAY(asm_prog, 1);

    if (!asm_prog)
    {
        return BFC_ERR_ALLOC;
    }

    asm_prog->capacity = BFC_INITIAL_ASM_CAPACITY;
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

/* Release the generated text buffer and its owning assembly object. */
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

/*
 * Write exactly length bytes; the internal null terminator is not part of the
 * assembly file.
 */
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
