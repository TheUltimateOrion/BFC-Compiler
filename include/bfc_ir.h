#ifndef BFC_IR_H
#define BFC_IR_H

#include <stddef.h>
#include <stdint.h>

#include "bfc_error.h"
#include "bfc_token.h"

typedef enum
{
    IR_ADD,
    IR_MOVE,
    IR_PUT,
    IR_GET,
    IR_SET,
    IR_LOOP
} bfc_ir_token_type_t;

typedef struct bfc_ir_block bfc_ir_block_t;

typedef struct
{
    bfc_ir_token_type_t op;

    union
    {
        int64_t         imm;
        bfc_ir_block_t* body;
    } val;
} bfc_ir_instr_t;

struct bfc_ir_block
{
    bfc_ir_instr_t* instr;

    size_t length;
    size_t capacity;
};

typedef struct
{
    bfc_ir_block_t** blocks;

    size_t length;
    size_t capacity;
} bfc_ir_stack_t;

[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_imm_instr(bfc_ir_token_type_t const ir_token_type, int64_t const imm);

[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_zero_instr(bfc_ir_token_type_t const ir_token_type);

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_ir_create(bfc_ir_block_t** root_block, bfc_token_stream_t const* const tok_stream);

[[gnu::nonnull(1)]]
bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t** ir_block);

void bfc_ir_destroy(bfc_ir_block_t** proot_block);

#endif  // BFC_IR_H
