#ifndef BFC_CODEGEN_H
#define BFC_CODEGEN_H

#include "bfc_error.h"
#include "bfc_ir.h"
#include "bfc_target.h"

/* Opaque generated-assembly object owned by the codegen module. */
typedef struct bfc_asm bfc_asm_t;

/*
 * Generate target-specific assembly for an optimized IR tree.
 * On success, the caller owns *out_asm and must destroy it.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen(bfc_asm_t** out_asm, const bfc_ir_block_t* ir_block, bfc_target_t target);

/* Release generated assembly and set the caller's pointer to null. */
void bfc_asm_destroy(bfc_asm_t** asm_prog);

/* Write the generated assembly buffer to path without taking ownership. */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_asm_write_file(const bfc_asm_t* asm_prog, const char* path);

#endif  // BFC_CODEGEN_H
