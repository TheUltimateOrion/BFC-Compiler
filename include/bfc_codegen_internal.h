#ifndef BFC_CODEGEN_INTERNAL_H
#define BFC_CODEGEN_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "bfc_codegen.h"
#include "bfc_target.h"

/*
 * Target backend dispatch table.
 *
 * Every callback must be non-null. The generic code generator owns IR
 * traversal and loop labels; the backend owns assembly syntax, ABI details,
 * register use, and instruction lowering.
 */
typedef struct
{
    bfc_target_t target;

    bfc_error_t (*emit_header)(bfc_asm_t* asm_prog);
    bfc_error_t (*emit_data_section)(bfc_asm_t* asm_prog);
    bfc_error_t (*emit_symbol)(bfc_asm_t* asm_prog);
    bfc_error_t (*emit_end)(bfc_asm_t* asm_prog);

    bfc_error_t (*emit_op_add)(bfc_asm_t* asm_prog, int64_t imm);
    bfc_error_t (*emit_op_move)(bfc_asm_t* asm_prog, int64_t imm);
    bfc_error_t (*emit_op_get)(bfc_asm_t* asm_prog);
    bfc_error_t (*emit_op_put)(bfc_asm_t* asm_prog);
    bfc_error_t (*emit_op_set)(bfc_asm_t* asm_prog, int64_t imm);

    bfc_error_t (*emit_loop_test_z)(bfc_asm_t* asm_prog, const char* label);
    bfc_error_t (*emit_loop_test_nz)(bfc_asm_t* asm_prog, const char* label);
} bfc_backend_t;

/*
 * Mutable assembly-generation state.
 *
 * buffer is always null-terminated. length excludes the terminator and must be
 * less than capacity. label_id supplies unique labels for nested loops.
 */
struct bfc_asm
{
    const bfc_backend_t* backend;

    size_t label_id;

    char*  buffer;
    size_t length;
    size_t capacity;
};

/* Immutable backend descriptors implemented by platform-specific modules. */
extern const bfc_backend_t BFC_BACKEND_WINDOWS_X86_64;
extern const bfc_backend_t BFC_BACKEND_WINDOWS_I386;
extern const bfc_backend_t BFC_BACKEND_WINDOWS_AARCH64;

extern const bfc_backend_t BFC_BACKEND_MACOS_AARCH64;
extern const bfc_backend_t BFC_BACKEND_MACOS_X86_64;

extern const bfc_backend_t BFC_BACKEND_LINUX_AARCH64;
extern const bfc_backend_t BFC_BACKEND_LINUX_X86_64;

/* Append raw text to the dynamically growing assembly buffer. */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen_emit_text(bfc_asm_t* asm_prog, const char* text);

/* Recursively lower one IR block through the selected backend. */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen_emit_block(bfc_asm_t* asm_prog, const bfc_ir_block_t* ir_block);

/* Format and append one assembly fragment using printf-compatible arguments. */
[[gnu::nonnull(1, 2), gnu::format(printf, 2, 3)]]
bfc_error_t bfc_codegen_emitf(bfc_asm_t* asm_prog, const char* format, ...);

#endif  // BFC_CODEGEN_INTERNAL_H
