#ifndef BFC_CODEGEN_H
#define BFC_CODEGEN_H

#include "bfc_error.h"
#include "bfc_ir.h"
#include <stdint.h>

typedef enum
{
    ARCH_X86_64,
    ARCH_i386,
    ARCH_aarch64,
    ARCH_arm32,
} bfc_arch_t;

typedef enum
{
    OS_WIN,
    OS_MAC,
    OS_LINUX,
} bfc_os_t;

typedef struct bfc_asm bfc_asm_t;

typedef struct
{
    void (*emit_header)(bfc_asm_t* asm_prog);
    void (*emit_data_section)(bfc_asm_t* asm_prog);
    void (*emit_symbol)(bfc_asm_t* asm_prog);
    void (*emit_end)(bfc_asm_t* asm_prog);

    void (*emit_op_add)(bfc_asm_t* asm_prog, int64_t imm);
    void (*emit_op_move)(bfc_asm_t* asm_prog, int64_t imm);
    void (*emit_op_get)(bfc_asm_t* asm_prog);
    void (*emit_op_put)(bfc_asm_t* asm_prog);
    void (*emit_op_set)(bfc_asm_t* asm_prog, int64_t imm);
    void (*emit_loop_test_z)(bfc_asm_t* asm_prog, char const* label);
    void (*emit_loop_test_nz)(bfc_asm_t* asm_prog, char const* label);
} bfc_backend_t;

struct bfc_asm
{
    bfc_arch_t    arch;
    bfc_os_t      os;
    bfc_backend_t backend;
    size_t        label_id;

    char*  buffer;
    size_t length;
    size_t capacity;
};

bfc_error_t bfc_codegen(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block);
bfc_error_t bfc_codegen_x86_64(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block);
bfc_error_t bfc_codegen_i386(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block);
bfc_error_t bfc_codegen_aarch64(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block);
bfc_error_t bfc_codegen_arm32(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block);

void bfc_codegen_emit_asm(bfc_asm_t** asm_prog, char const* asm_str);
void bfc_codegen_emit_label(bfc_asm_t** asm_prog, char const* label_str);
void bfc_codegen_emit_block(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block);

void bfc_asm_destroy(bfc_asm_t** pasm_prog);

#endif  // BFC_CODEGEN_H
