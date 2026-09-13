#include <inttypes.h>

#include "bfc_codegen_internal.h"
#include "bfc_config.h"

/**
 * @file bfc_backend_linux_aarch64.c
 * @brief Linux AArch64 assembly backend.
 *
 * @details
 * Implements ELF symbols, the AAPCS64 ABI, and GNU AArch64-syntax lowering
 * for all IR operations. X19 holds the Brainfuck tape pointer across libc
 * calls, while X16/W16 is used for immediates and cell values.
 */

/**
 * @brief Materializes a 64-bit unsigned immediate in scratch register `x16`.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_aarch64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
{
    bfc_error_t err
        = bfc_codegen_emitf(asm_prog, "    movz x16, #%u\n", (unsigned) (value & UINT64_C(0xffff)));

    if (err.code != ERR_OK)
    {
        return err;
    }

    for (uint16_t shift = 16; shift < 64; shift += 16)
    {
        const uint16_t part = (uint16_t) ((value >> shift) & UINT64_C(0xffff));

        if (part == 0)
        {
            continue;
        }

        err = bfc_codegen_emitf(
            asm_prog, "    movk x16, #%u, lsl #%u\n", (unsigned) part, (unsigned) shift
        );

        if (err.code != ERR_OK)
        {
            return err;
        }
    }

    return BFC_ERR_OK;
}

/**
 * @brief Emits the ELF text-section directives.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_aarch64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, ".text\n.p2align 2\n");
}

/**
 * @brief Declares the zero-initialized Brainfuck tape in ELF BSS.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_aarch64_emit_data_section(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emitf(
        asm_prog,
        ".section .bss\n"
        ".balign 16\n"
        ".bfc_tape:\n"
        "    .skip %zu\n",
        BFC_TAPE_SIZE
    );
}

/**
 * @brief Emits `main`, its aligned stack frame, and the tape address.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_aarch64_emit_symbol(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
                  ".global main\n"
                  ".type main, %function\n"
                  ".p2align 2\n"
                  "main:\n"
                  "    stp x29, x30, [sp, #-32]!\n"
                  "    str x19, [sp, #16]\n"
                  "    mov x29, sp\n"
                  "\n"
                  "    adrp x19, .bfc_tape\n"
                  "    add  x19, x19, :lo12:.bfc_tape\n"
    );
}

/**
 * @brief Emits the ABI-compliant epilogue and zero process status.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_aarch64_emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    mov w0, #0\n"
                  "    ldr x19, [sp, #16]\n"
                  "    ldp x29, x30, [sp], #32\n"
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
static bfc_error_t linux_aarch64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    if (normalized == 0)
    {
        return BFC_ERR_OK;
    }

    return bfc_codegen_emitf(
        asm_prog,
        "    ldrb w16, [x19]\n"
        "    add  w16, w16, #%u\n"
        "    strb w16, [x19]\n",
        (unsigned) normalized
    );
}

/**
 * @brief Moves the tape pointer using direct or register materialized immediates.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_aarch64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
{
    if (imm == 0)
    {
        return BFC_ERR_OK;
    }

    const uint64_t magnitude = imm < 0 ? UINT64_C(0) - (uint64_t) imm : (uint64_t) imm;

    if (magnitude <= 4095)
    {
        return bfc_codegen_emitf(
            asm_prog,
            imm < 0 ? "    sub x19, x19, #%" PRIu64 "\n" : "    add x19, x19, #%" PRIu64 "\n",
            magnitude
        );
    }

    bfc_error_t err = linux_aarch64_emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    return bfc_codegen_emit_text(
        asm_prog, imm < 0 ? "    sub x19, x19, x16\n" : "    add x19, x19, x16\n"
    );
}

/**
 * @brief Calls `getchar`, maps EOF to zero, and stores one byte.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_aarch64_emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    bl   getchar\n"
                  "    cmn  w0, #1\n"
                  "    csel w0, wzr, w0, eq\n"
                  "    strb w0, [x19]\n"
    );
}

/**
 * @brief Loads the current cell and calls `putchar`.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_aarch64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, "    ldrb w0, [x19]\n    bl   putchar\n");
}

/**
 * @brief Stores a normalized byte value in the current cell.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t linux_aarch64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    if (normalized == 0)
    {
        return bfc_codegen_emit_text(asm_prog, "    strb wzr, [x19]\n");
    }

    return bfc_codegen_emitf(
        asm_prog,
        "    mov  w16, #%u\n"
        "    strb w16, [x19]\n",
        (unsigned) normalized
    );
}

/**
 * @brief Uses `cbz` to branch when the current cell is zero.
 *
 * @internal
 */
[[gnu::nonnull(1, 2)]]
static bfc_error_t linux_aarch64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    ldrb w16, [x19]\n    cbz  w16, %s\n", label);
}

/**
 * @brief Uses `cbnz` to branch when the current cell is nonzero.
 *
 * @internal
 */
[[gnu::nonnull(1, 2)]]
static bfc_error_t linux_aarch64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    ldrb w16, [x19]\n    cbnz w16, %s\n", label);
}

/**
 * @brief Immutable backend descriptor exported to generic code generation.
 */
const bfc_backend_t BFC_BACKEND_LINUX_AARCH64 = {
    .target = {
        .arch = BFC_ARCH_AARCH64,
        .os   = BFC_OS_LINUX,
    },

    .emit_header       = linux_aarch64_emit_header,
    .emit_data_section = linux_aarch64_emit_data_section,
    .emit_symbol       = linux_aarch64_emit_symbol,
    .emit_end          = linux_aarch64_emit_end,

    .emit_op_add       = linux_aarch64_emit_op_add,
    .emit_op_move      = linux_aarch64_emit_op_move,
    .emit_op_get       = linux_aarch64_emit_op_get,
    .emit_op_put       = linux_aarch64_emit_op_put,
    .emit_op_set       = linux_aarch64_emit_op_set,
    .emit_loop_test_z  = linux_aarch64_emit_loop_test_z,
    .emit_loop_test_nz = linux_aarch64_emit_loop_test_nz,
};
