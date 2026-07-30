/**
 * @file bfc_codegen_internal.h
 * @brief Internal backend contract and assembly representation.
 *
 * @details
 * Shared only by generic code generation and target-specific backend implementations.
 */
#ifndef BFC_CODEGEN_INTERNAL_H
#define BFC_CODEGEN_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "bfc_codegen.h"
#include "bfc_target.h"

/**
 * @brief Immutable callback table implemented by one target backend.
 *
 * @internal
 *
 * @note Every callback must be non-null for an advertised backend.
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

/**
 * @brief Mutable assembly-generation state shared by generic codegen and backends.
 *
 * @internal
 *
 * @note `buffer` is always null-terminated; `length` excludes the terminator.
 */
struct bfc_asm
{
    const bfc_backend_t* backend;

    size_t label_id;

    char*  buffer;
    size_t length;
    size_t capacity;
};

/**
 * @brief Backend descriptor declarations.
 *
 * @internal
 */
extern const bfc_backend_t BFC_BACKEND_WINDOWS_X86_64;
extern const bfc_backend_t BFC_BACKEND_WINDOWS_I386;
extern const bfc_backend_t BFC_BACKEND_WINDOWS_AARCH64;

extern const bfc_backend_t BFC_BACKEND_MACOS_AARCH64;
extern const bfc_backend_t BFC_BACKEND_MACOS_X86_64;

extern const bfc_backend_t BFC_BACKEND_LINUX_AARCH64;
extern const bfc_backend_t BFC_BACKEND_LINUX_X86_64;

/**
 * @brief Appends raw text to the assembly buffer.
 *
 * @internal
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen_emit_text(bfc_asm_t* asm_prog, const char* text);

/**
 * @brief Recursively emits every instruction in an IR block.
 *
 * @internal
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen_emit_block(bfc_asm_t* asm_prog, const bfc_ir_block_t* ir_block);

/**
 * @brief Formats and appends one assembly fragment.
 *
 * @internal
 */
[[gnu::nonnull(1, 2), gnu::format(printf, 2, 3)]]
bfc_error_t bfc_codegen_emitf(bfc_asm_t* asm_prog, const char* format, ...);

#endif  // BFC_CODEGEN_INTERNAL_H
