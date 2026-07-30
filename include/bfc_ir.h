/**
 * @file bfc_ir.h
 * @brief Brainfuck intermediate representation interface.
 *
 * @details
 * Defines nested IR blocks, instructions, construction, optimization, and recursive destruction.
 */
#ifndef BFC_IR_H
#define BFC_IR_H

#include <stddef.h>
#include <stdint.h>

#include "bfc_error.h"
#include "bfc_token.h"

/**
 * @brief Operations understood by the optimizer and code generator.
 */
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

/**
 * @brief One IR instruction.
 *
 * @note `val.imm` is used by immediate operations; `val.body` owns a nested loop block.
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

/**
 * @brief Owning, dynamically sized sequence of IR instructions.
 */
struct bfc_ir_block
{
    bfc_ir_instr_t* instructions;

    size_t length;
    size_t capacity;
};

/**
 * @brief Constructs an immediate-valued IR instruction.
 */
[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_imm_instr(bfc_ir_token_type_t const ir_token_type, int64_t const imm);

/**
 * @brief Constructs an IR instruction with a zero-initialized operand union.
 */
[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_zero_instr(bfc_ir_token_type_t const ir_token_type);

/**
 * @brief Builds a nested IR tree from the token stream.
 *
 * @param[out] root_block Receives the allocated root block.
 * @param[in] tok_stream Validated token stream.
 *
 * @return `BFC_ERR_OK` on success; otherwise an allocation error.
 *
 * @note Release the tree with `bfc_ir_destroy()`.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_ir_create(bfc_ir_block_t** root_block, bfc_token_stream_t const* const tok_stream);

/**
 * @brief Optimizes repeated operations and recognized clear loops in place.
 *
 * @param[in,out] ir_block Address of the owned IR block pointer.
 *
 * @return `BFC_ERR_OK` on success; otherwise an allocation error.
 */
[[gnu::nonnull(1)]]
bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t** ir_block);

/**
 * @brief Recursively destroys an IR block and all nested loop bodies.
 */
void bfc_ir_destroy(bfc_ir_block_t** proot_block);

#endif  // BFC_IR_H
