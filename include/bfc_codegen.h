#ifndef __BFC_CODEGEN_H
#define __BFC_CODEGEN_H

#include <sys/types.h>

#include "bfc_error.h"
#include "bfc_ir.h"

typedef enum {
	ARCH_X86_64,
	ARCH_i386,
	ARCH_aarch64,
	ARCH_arm32,
} bfc_arch_t;

typedef enum {
	OS_WIN,
	OS_MAC,
	OS_LINUX,
} bfc_os_t;

struct bfc_asm_t;

typedef struct {
	void (*emit_header)(struct bfc_asm_t *asm_prog);
	void (*emit_data_section)(struct bfc_asm_t *asm_prog);
	void (*emit_symbol)(struct bfc_asm_t *asm_prog);
	void (*emit_end)(struct bfc_asm_t *asm_prog);
	
	void (*emit_op_add)(struct bfc_asm_t *asm_prog, ssize_t imm);
	void (*emit_op_move)(struct bfc_asm_t *asm_prog, ssize_t imm);
	void (*emit_op_get)(struct bfc_asm_t *asm_prog);
	void (*emit_op_put)(struct bfc_asm_t *asm_prog);
	void (*emit_op_set)(struct bfc_asm_t *asm_prog, ssize_t imm);
	void (*emit_loop_test_z)(struct bfc_asm_t *asm_prog, const char* label);
	void (*emit_loop_test_nz)(struct bfc_asm_t *asm_prog, const char* label);
} bfc_backend_t;

typedef struct {
	bfc_arch_t arch;
	bfc_os_t os;
	bfc_backend_t backend;
	size_t label_id;

	char *buffer;
	size_t length;
	size_t capacity;
} bfc_asm_t;

bfc_error_t bfc_codegen(bfc_asm_t **asm_prog, const bfc_ir_block_t *const ir_block);
bfc_error_t bfc_codegen_x86_64(bfc_asm_t **asm_prog, const bfc_ir_block_t *const ir_block);
bfc_error_t bfc_codegen_i386(bfc_asm_t **asm_prog, const bfc_ir_block_t *const ir_block);
bfc_error_t bfc_codegen_aarch64(bfc_asm_t **asm_prog, const bfc_ir_block_t *const ir_block);
bfc_error_t bfc_codegen_arm32(bfc_asm_t **asm_prog, const bfc_ir_block_t *const ir_block);

void bfc_codegen_emit_asm(bfc_asm_t **asm_prog, const char *asm_str);
void bfc_codegen_emit_label(bfc_asm_t **asm_prog, const char *label_str);
void bfc_codegen_emit_block(bfc_asm_t **asm_prog, const bfc_ir_block_t *const ir_block);

void bfc_asm_destroy(bfc_asm_t **pasm_prog);

#endif // __BFC_CODEGEN_H
