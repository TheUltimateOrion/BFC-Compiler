#ifndef BFC_IR_H
#define BFC_IR_H

#include <stddef.h>
#include <stdint.h>

#include "bfc_error.h"
#include "bfc_token.h"

/* Operations represented by the persistent intermediate representation. */
typedef enum
{
    IR_ADD,   /* Add a signed immediate to the current cell. */
    IR_MOVE,  /* Move the tape pointer by a signed byte offset. */
    IR_PUT,   /* Output the current cell. */
    IR_GET,   /* Read one input byte into the current cell. */
    IR_SET,   /* Set the current cell to an immediate byte value. */
    IR_LOOP   /* Execute a nested block while the current cell is nonzero. */
} bfc_ir_token_type_t;

typedef struct bfc_ir_block bfc_ir_block_t;

/*
 * One IR instruction.
 *
 * val.imm is used by IR_ADD, IR_MOVE, and IR_SET. val.body is used by IR_LOOP
 * and is owned recursively by the containing IR block.
 */
typedef struct
{
    bfc_ir_token_type_t op;

    union
    {
        int64_t         imm;
        bfc_ir_block_t* body;
    } val;
} bfc_ir_instr_t;

/* Owning, dynamically sized sequence of IR instructions. */
struct bfc_ir_block
{
    bfc_ir_instr_t* instructions;
    size_t          length;
    size_t          capacity;
};

/* Construct an immediate-bearing IR instruction without allocating memory. */
[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_imm_instr(bfc_ir_token_type_t const ir_token_type, int64_t const imm);

/* Construct an IR instruction whose value union is zero-initialized. */
[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_zero_instr(bfc_ir_token_type_t const ir_token_type);

/*
 * Build a nested IR tree from the token stream.
 * On success, the caller owns *root_block and must destroy it recursively.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_ir_create(bfc_ir_block_t** root_block, bfc_token_stream_t const* const tok_stream);

/*
 * Replace *ir_block with an optimized equivalent block.
 * Current optimizations fold repeated ADD/MOVE operations and clear loops.
 */
[[gnu::nonnull(1)]]
bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t** ir_block);

/* Recursively release an IR tree and set the caller's pointer to null. */
void bfc_ir_destroy(bfc_ir_block_t** proot_block);

#endif  // BFC_IR_H
