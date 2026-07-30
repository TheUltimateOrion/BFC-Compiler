#include "bfc_ir.h"

#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "bfc_config.h"
#include "bfc_error.h"
#include "bfc_memory.h"

typedef struct
{
    bfc_ir_block_t** blocks;

    size_t length;
    size_t capacity;
} bfc_ir_stack_t;

bfc_ir_instr_t bfc_ir_make_imm_instr(bfc_ir_token_type_t const ir_token_type, int64_t const imm)
{
    return (bfc_ir_instr_t) {
        .op  = ir_token_type,
        .val = {imm},
    };
}

bfc_ir_instr_t bfc_ir_make_zero_instr(bfc_ir_token_type_t const ir_token_type)
{
    return (bfc_ir_instr_t) {
        .op = ir_token_type,
    };
}

bfc_error_t bfc_ir_create(bfc_ir_block_t** root_block, bfc_token_stream_t const* const tok_stream)
{
    bfc_error_t err = BFC_ERR_ALLOC;
    *root_block     = nullptr;

    bfc_ir_stack_t stack = (bfc_ir_stack_t) {
        .capacity = BFC_INITIAL_IR_STACK_CAPACITY,
        .length   = 0,
    };

    stack.blocks = BFC_CALLOC_ARRAY(stack.blocks, stack.capacity);
    if (!stack.blocks)
    {
        goto end;
    }

    stack.blocks[stack.length] = BFC_CALLOC_ARRAY(stack.blocks[stack.length], 1);
    if (!stack.blocks[stack.length])
    {
        goto end;
    }

    bfc_ir_block_t* current_block = stack.blocks[stack.length++];
    current_block->instructions   = nullptr;
    current_block->capacity       = BFC_INITIAL_IR_CAPACITY;
    current_block->length         = 0;

    current_block->instructions
        = BFC_MALLOC_ARRAY(current_block->instructions, current_block->capacity);
    if (!current_block->instructions)
    {
        goto end;
    }

    size_t i = 0;
    while (i < tok_stream->length)
    {
        if (stack.length >= stack.capacity)
        {
            size_t new_capacity;

            if (ckd_mul(&new_capacity, stack.capacity, 2))
            {
                goto end;
            }

            bfc_ir_block_t** tmp = BFC_REALLOC_ARRAY(stack.blocks, new_capacity);

            if (!tmp)
            {
                goto end;
            }

            stack.blocks   = tmp;
            stack.capacity = new_capacity;
        }

        if (current_block->length >= current_block->capacity)
        {
            size_t new_capacity;

            if (ckd_mul(&new_capacity, current_block->capacity, 2))
            {
                goto end;
            }

            bfc_ir_instr_t* tmp = BFC_REALLOC_ARRAY(current_block->instructions, new_capacity);

            if (!tmp)
            {
                goto end;
            }

            current_block->instructions = tmp;
            current_block->capacity     = new_capacity;
        }

        switch (tok_stream->tokens[i].type)
        {
            case TT_INC: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_imm_instr(IR_ADD, 1);
            }
            break;

            case TT_DEC: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_imm_instr(IR_ADD, -1);
            }
            break;

            case TT_PTR_LEFT: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_imm_instr(IR_MOVE, -1);
            }
            break;

            case TT_PTR_RIGHT: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_imm_instr(IR_MOVE, 1);
            }
            break;

            case TT_INPUT: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_zero_instr(IR_GET);
            }
            break;

            case TT_OUTPUT: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_zero_instr(IR_PUT);
            }
            break;

            case TT_LOOP_START: {
                bfc_ir_block_t* loop_body = nullptr;
                loop_body                 = BFC_CALLOC_ARRAY(loop_body, 1);

                bfc_ir_instr_t loop_instr = (bfc_ir_instr_t) {
                    .op  = IR_LOOP,
                    .val = {.body = loop_body},
                };

                if (!loop_instr.val.body)
                {
                    goto end;
                }

                current_block->instructions[current_block->length++] = loop_instr;

                stack.blocks[stack.length] = loop_instr.val.body;

                current_block           = stack.blocks[stack.length++];
                current_block->capacity = BFC_INITIAL_IR_CAPACITY;
                current_block->length   = 0;

                current_block->instructions
                    = BFC_MALLOC_ARRAY(current_block->instructions, current_block->capacity);

                if (!current_block->instructions)
                {
                    goto end;
                }
            }
            break;

            case TT_LOOP_END: {
                --stack.length;
                current_block = stack.blocks[stack.length - 1];
            }
            break;
        }

        ++i;
    }

    err = BFC_ERR_OK;

end:
    if (stack.blocks)
    {
        if (err.code == ERR_OK)
        {
            *root_block = stack.blocks[0];
        }
        else if (stack.blocks[0])
        {
            bfc_ir_destroy(&stack.blocks[0]);
        }

        free(stack.blocks);
    }

    return err;
}

bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t** ir_block)
{
    if ((*ir_block)->length == 0)
    {
        return BFC_ERR_OK;
    }

    bfc_error_t err = BFC_ERR_ALLOC;

    bfc_ir_block_t* optimized_block = nullptr;
    optimized_block                 = BFC_CALLOC_ARRAY(optimized_block, 1);

    if (!optimized_block)
    {
        goto end;
    }

    optimized_block->capacity = (*ir_block)->capacity;

    optimized_block->instructions
        = BFC_MALLOC_ARRAY(optimized_block->instructions, optimized_block->capacity);

    if (!optimized_block->instructions)
    {
        goto end;
    }

    bfc_ir_instr_t prev_instr  = (*ir_block)->instructions[0];
    int64_t        instr_delta = 0;
    size_t         i           = 0;
    while (i < (*ir_block)->length)
    {
        if ((*ir_block)->instructions[i].op == IR_ADD || (*ir_block)->instructions[i].op == IR_MOVE)
        {
            do
            {
                instr_delta += (*ir_block)->instructions[i].val.imm;
                prev_instr = (*ir_block)->instructions[i++];
            }
            while (i < (*ir_block)->length && (*ir_block)->instructions[i].op == prev_instr.op);

            if (instr_delta != 0)
            {
                optimized_block->instructions[optimized_block->length++]
                    = bfc_ir_make_imm_instr(prev_instr.op, instr_delta);
            }

            instr_delta = 0;
        }
        else
        {
            if ((*ir_block)->instructions[i].op == IR_LOOP)
            {
                bfc_ir_instr_t* loop = &(*ir_block)->instructions[i];

                err = bfc_ir_optimize_rep(&loop->val.body);

                if (err.code != ERR_OK)
                {
                    goto end;
                }

                const bfc_ir_block_t* body = loop->val.body;

                if (body->length == 1 && body->instructions[0].op == IR_ADD
                    && (body->instructions[0].val.imm == 1 || body->instructions[0].val.imm == -1))
                {
                    bfc_ir_destroy(&loop->val.body);

                    optimized_block->instructions[optimized_block->length++]
                        = bfc_ir_make_imm_instr(IR_SET, 0);

                    ++i;
                    continue;
                }
            }

            optimized_block->instructions[optimized_block->length++] = (*ir_block)->instructions[i];

            prev_instr = (*ir_block)->instructions[i++];
        }
    }

    free((*ir_block)->instructions);
    free(*ir_block);

    *ir_block       = optimized_block;
    optimized_block = nullptr;

    err = BFC_ERR_OK;

end:
    if (optimized_block)
    {
        free(optimized_block->instructions);
        free(optimized_block);
    }

    return err;
}

void bfc_ir_destroy(bfc_ir_block_t** proot_block)
{
    if (!proot_block || !*proot_block)
    {
        return;
    }

    for (size_t i = 0; i < (*proot_block)->length; ++i)
    {
        if ((*proot_block)->instructions[i].op == IR_LOOP
            && (*proot_block)->instructions[i].val.body)
        {
            bfc_ir_destroy(&(*proot_block)->instructions[i].val.body);
        }
    }

    free((*proot_block)->instructions);
    free(*proot_block);

    *proot_block = nullptr;
}
