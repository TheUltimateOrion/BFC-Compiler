/**
 * @file bfc_codegen.h
 * @brief Public assembly code-generation interface.
 *
 * @details
 * Exposes the opaque assembly object, target-aware code generation, file output, and destruction.
 */
#ifndef BFC_CODEGEN_H
#define BFC_CODEGEN_H

#include "bfc_error.h"
#include "bfc_ir.h"
#include "bfc_target.h"

/**
 * @brief Opaque generated-assembly object.
 */
typedef struct bfc_asm bfc_asm_t;

/**
 * @brief Generates target-specific assembly from an optimized IR tree.
 *
 * @param[out] out_asm Receives the allocated assembly object on success.
 * @param[in] ir_block Root IR block.
 * @param[in] target Requested code-generation target.
 *
 * @return `BFC_ERR_OK` on success; otherwise a target, allocation, or emission error.
 *
 * @note Release the result with `bfc_asm_destroy()`.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen(bfc_asm_t** out_asm, const bfc_ir_block_t* ir_block, bfc_target_t target);

/**
 * @brief Releases an assembly object and nulls the caller pointer.
 *
 * @param[in,out] asm_prog Address of the owned assembly pointer.
 */
void bfc_asm_destroy(bfc_asm_t** asm_prog);

/**
 * @brief Writes generated assembly bytes to a file.
 *
 * @param[in] asm_prog Completed assembly object.
 * @param[in] path Destination file path.
 *
 * @return `BFC_ERR_OK` on success; otherwise an I/O error.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_asm_write_file(const bfc_asm_t* asm_prog, const char* path);

#endif  // BFC_CODEGEN_H
