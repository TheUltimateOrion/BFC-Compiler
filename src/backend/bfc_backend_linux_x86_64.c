#include <inttypes.h>

#include "bfc_codegen_internal.h"
#include "bfc_config.h"

/**
 * @file bfc_backend_linux_x86_64.c
 * @brief Linux x86-64 assembly backend.
 *
 * @details
 * Implements ELF symbols, the System V AMD64 ABI, and AT&T-syntax lowering
 * for all IR operations. RBX holds the Brainfuck tape pointer across libc
 * calls, while R11 is used as a scratch register for large pointer offsets.
 */

/**
 * @brief Materializes a full-width unsigned immediate in scratch register `r11`.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
{
    return bfc_codegen_emitf(asm_prog, "    movabsq $0x%016" PRIx64 ", %%r11\n", value);
}

/**
 * @brief Emits the ELF text-section directives.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, ".text\n.p2align 4\n");
}

/**
 * @brief Declares the zero-initialized Brainfuck tape in ELF BSS.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_data_section(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emitf(
        asm_prog,
        ".section .bss\n"
        ".balign 16\n"
        ".bfc_tape:\n"
        "    .zero %zu\n",
        BFC_TAPE_SIZE
    );
}

/**
 * @brief Emits `main`, its aligned stack frame, and the tape address.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_symbol(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
                  ".globl main\n"
                  ".type main, @function\n"
                  ".p2align 4\n"
                  "main:\n"
                  "    pushq %rbp\n"
                  "    movq  %rsp, %rbp\n"
                  "    pushq %rbx\n"
                  "    subq  $8, %rsp\n"
                  "\n"
                  "    leaq  .bfc_tape(%rip), %rbx\n"
    );
}

/**
 * @brief Emits the ABI-compliant epilogue and zero process status.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    xorl  %eax, %eax\n"
                  "    addq  $8, %rsp\n"
                  "    popq  %rbx\n"
                  "    popq  %rbp\n"
                  "    ret\n"
                  "    .size main, .-main\n"
    );
}

/**
 * @brief Lowers wrapping byte-cell addition.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    if (normalized == 0)
    {
        return BFC_ERR_OK;
    }

    return bfc_codegen_emitf(asm_prog, "    addb $%u, (%%rbx)\n", (unsigned) normalized);
}

/**
 * @brief Moves the tape pointer using direct or register materialized immediates.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
{
    if (imm == 0)
    {
        return BFC_ERR_OK;
    }

    const uint64_t magnitude = imm < 0 ? UINT64_C(0) - (uint64_t) imm : (uint64_t) imm;

    if (magnitude <= INT32_MAX)
    {
        return bfc_codegen_emitf(
            asm_prog, imm < 0 ? "    subq $%" PRIu64 ", %%rbx\n" : "    addq $%" PRIu64 ", %%rbx\n",
            magnitude
        );
    }

    bfc_error_t err = linux_x86_64_emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    return bfc_codegen_emit_text(
        asm_prog, imm < 0 ? "    subq %r11, %rbx\n" : "    addq %r11, %rbx\n"
    );
}

/**
 * @brief Calls `getchar`, maps EOF to zero, and stores one byte.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    call getchar@PLT\n"
                  "    xorl %edx, %edx\n"
                  "    cmpl $-1, %eax\n"
                  "    cmove %edx, %eax\n"
                  "    movb %al, (%rbx)\n"
    );
}

/**
 * @brief Zero-extends the current cell and calls `putchar` through the PLT.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    movzbl (%rbx), %edi\n"
                  "    call putchar@PLT\n"
    );
}

/**
 * @brief Stores a normalized byte value in the current cell.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_x86_64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    return bfc_codegen_emitf(asm_prog, "    movb $%u, (%%rbx)\n", (unsigned) normalized);
}

/**
 * @brief Uses `je` to branch when the current cell is zero.
 *
 * @internal
 */
[[gnu::nonnull(1, 2)]]
static bfc_error_t linux_x86_64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    cmpb $0, (%%rbx)\n    je %s\n", label);
}

/**
 * @brief Uses `jne` to branch when the current cell is nonzero.
 *
 * @internal
 */
[[gnu::nonnull(1, 2)]]
static bfc_error_t linux_x86_64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    cmpb $0, (%%rbx)\n    jne %s\n", label);
}

/**
 * @brief Immutable backend descriptor exported to generic code generation.
 */
const bfc_backend_t BFC_BACKEND_LINUX_X86_64 = {
    .target = {
        .arch = BFC_ARCH_X86_64,
        .os   = BFC_OS_LINUX,
    },

    .emit_header       = linux_x86_64_emit_header,
    .emit_data_section = linux_x86_64_emit_data_section,
    .emit_symbol       = linux_x86_64_emit_symbol,
    .emit_end          = linux_x86_64_emit_end,

    .emit_op_add       = linux_x86_64_emit_op_add,
    .emit_op_move      = linux_x86_64_emit_op_move,
    .emit_op_get       = linux_x86_64_emit_op_get,
    .emit_op_put       = linux_x86_64_emit_op_put,
    .emit_op_set       = linux_x86_64_emit_op_set,
    .emit_loop_test_z  = linux_x86_64_emit_loop_test_z,
    .emit_loop_test_nz = linux_x86_64_emit_loop_test_nz,
};
