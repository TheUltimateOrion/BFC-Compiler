#ifndef __BFC_IR_H
#define __BFC_IR_H

#include <sys/types.h>

#include "bfc_error.h"

typedef enum
{
    IR_ADD,
    IR_MOVE,
    IR_PUT,
    IR_GET,
    IR_SET,
    IR_LOOP
} bfc_ir_token_type_t;

struct bfc_ir_block_t;

typedef struct
{
    bfc_ir_token_type_t op;

    union
    {
        ssize_t                imm;
        struct bfc_ir_block_t* body;
    } val;
} bfc_ir_instr_t;

typedef struct
{
    bfc_ir_instr_t* instr;

    size_t length;
    size_t capacity;
} bfc_ir_block_t;

typedef struct
{
    bfc_ir_block_t** blocks;

    size_t length;
    size_t capacity;
} bfc_ir_stack_t;

[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_imm_instr(bfc_ir_token_type_t const ir_token_type, ssize_t const imm);

[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_zero_instr(bfc_ir_token_type_t const ir_token_type);

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_ir_create(bfc_ir_block_t** root_block, bfc_token_stream_t const* const tok_stream);

[[gnu::nonnull(1)]]
bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t** ir_block);

void bfc_ir_destroy(bfc_ir_block_t** proot_block);

#endif  // __BFC_IR_H
