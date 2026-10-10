#include <inttypes.h>

#include "bfc_codegen_internal.h"
#include "bfc_config.h"

/**
 * @file bfc_backend_windows_aarch64.c
 * @brief Windows AArch64 PE/COFF assembly backend.
 *
 * @details
 * Implements the Microsoft ARM64 ABI in GNU AArch64 syntax. X19 holds the
 * Brainfuck tape pointer across CRT calls, while X16/W16 is used as scratch.
 */

/** @brief Materializes a 64-bit unsigned immediate in scratch register `x16`. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
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

/** @brief Emits Windows AArch64 text-section directives. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, ".text\n.p2align 2\n");
}

/** @brief Declares the zero-initialized Brainfuck tape in PE/COFF BSS. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_data_section(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emitf(
        asm_prog,
        ".bss\n"
        ".lcomm .bfc_tape, %zu, 16\n",
        BFC_TAPE_SIZE
    );
}

/** @brief Emits the Windows ARM64 prologue, unwind metadata, and tape address. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_symbol(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
              ".globl main\n"
              ".def main; .scl 2; .type 32; .endef\n"
                  ".p2align 2\n"
                  "main:\n"
                  ".seh_proc main\n"
                  "    sub sp, sp, #48\n"
                  "    .seh_stackalloc 48\n"
                  "    stp x19, x30, [sp, #32]\n"
                  "    .seh_save_regp_x x19, 32\n"
                  "    .seh_endprologue\n"
                  "\n"
                  "    adrp x19, .bfc_tape\n"
                  "    add  x19, x19, :lo12:.bfc_tape\n"
    );
}

/** @brief Emits the Windows ARM64 epilogue and zero process status. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    mov w0, wzr\n"
                  "    .seh_startepilogue\n"
                  "    ldp x19, x30, [sp, #32]\n"
                  "    .seh_save_regp_x x19, 32\n"
                  "    add sp, sp, #48\n"
                  "    .seh_stackalloc 48\n"
                  "    .seh_endepilogue\n"
                  "    ret\n"
                "    .seh_endfunclet\n"
                  "    .seh_endproc\n"
    );
}

/** @brief Lowers wrapping byte-cell addition. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
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

/** @brief Moves the tape pointer using direct or register materialized immediates. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
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

    bfc_error_t err = windows_aarch64_emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    return bfc_codegen_emit_text(
        asm_prog, imm < 0 ? "    sub x19, x19, x16\n" : "    add x19, x19, x16\n"
    );
}

/** @brief Calls `getchar`, maps EOF to zero, and stores one byte. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    bl   getchar\n"
                  "    cmn  w0, #1\n"
                  "    csel w0, wzr, w0, eq\n"
                  "    strb w0, [x19]\n"
    );
}

/** @brief Loads the current cell and calls `putchar`. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, "    ldrb w0, [x19]\n    bl   putchar\n");
}

/** @brief Stores a normalized byte value in the current cell. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_aarch64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
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

/** @brief Branches when the current cell is zero. @internal */
[[gnu::nonnull(1, 2)]]
static bfc_error_t windows_aarch64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    ldrb w16, [x19]\n    cbz  w16, %s\n", label);
}

/** @brief Branches when the current cell is nonzero. @internal */
[[gnu::nonnull(1, 2)]]
static bfc_error_t windows_aarch64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    ldrb w16, [x19]\n    cbnz w16, %s\n", label);
}

/** @brief Immutable Windows AArch64 backend descriptor. */
const bfc_backend_t BFC_BACKEND_WINDOWS_AARCH64 = {
    .target = {
        .arch = BFC_ARCH_AARCH64,
        .os   = BFC_OS_WINDOWS,
    },

    .emit_header       = windows_aarch64_emit_header,
    .emit_data_section = windows_aarch64_emit_data_section,
    .emit_symbol       = windows_aarch64_emit_symbol,
    .emit_end          = windows_aarch64_emit_end,

    .emit_op_add       = windows_aarch64_emit_op_add,
    .emit_op_move      = windows_aarch64_emit_op_move,
    .emit_op_get       = windows_aarch64_emit_op_get,
    .emit_op_put       = windows_aarch64_emit_op_put,
    .emit_op_set       = windows_aarch64_emit_op_set,
    .emit_loop_test_z  = windows_aarch64_emit_loop_test_z,
    .emit_loop_test_nz = windows_aarch64_emit_loop_test_nz,
};
