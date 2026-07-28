#ifndef BFC_CODEGEN_H
#define BFC_CODEGEN_H

#include "bfc_error.h"
#include "bfc_ir.h"
#include "bfc_target.h"

typedef struct bfc_asm bfc_asm_t;

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen(bfc_asm_t** out_asm, const bfc_ir_block_t* ir_block, bfc_target_t target);

void bfc_asm_destroy(bfc_asm_t** asm_prog);

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_asm_write_file(const bfc_asm_t* asm_prog, const char* path);

#endif  // BFC_CODEGEN_H
