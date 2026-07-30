/*
 * macOS AArch64 assembly backend.
 *
 * The backend emits Mach-O assembly using Apple's relocation syntax. X19 holds
 * the Brainfuck tape pointer across libc calls, while X16/W16 is used as a
 * scratch register for immediates and cell values.
 */

#include "bfc_codegen_internal.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include "bfc_config.h"

/*
 * Materialize a 64-bit constant in X16 as one MOVZ followed by only the MOVK
 * instructions required for nonzero 16-bit chunks.
 */
[[gnu::nonnull(1)]]
static bfc_error_t emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
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

[[gnu::nonnull(1)]]
static bfc_error_t emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
                  ".p2align 2\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_data_section(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emitf(
        asm_prog,
        ".section __DATA,__bss\n"
        ".p2align 4\n"
        "_bfc_tape:\n"
        "    .space %zu\n",
        BFC_TAPE_SIZE
    );
}

/*
 * Establish an ABI-compliant main function and preserve X19, the callee-saved
 * register used as the tape pointer.
 */
[[gnu::nonnull(1)]]
static bfc_error_t emit_symbol(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
                  ".globl _main\n"
                  ".p2align 2\n"
                  "_main:\n"
                  "    stp x29, x30, [sp, #-32]!\n"
                  "    str x19, [sp, #16]\n"
                  "    mov x29, sp\n"
                  "\n"
                  "    adrp x19, _bfc_tape@PAGE\n"
                  "    add  x19, x19, _bfc_tape@PAGEOFF\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    mov w0, #0\n"
                  "    ldr x19, [sp, #16]\n"
                  "    ldp x29, x30, [sp], #32\n"
                  "    ret\n"
    );
}

/*
 * Normalize the IR operand to one byte so positive and negative additions use
 * Brainfuck's modulo-256 cell semantics.
 */
[[gnu::nonnull(1)]]
static bfc_error_t emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
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

/*
 * Use AArch64's 12-bit add/sub immediate form for common small movements.
 * Larger magnitudes are materialized in X16.
 */
[[gnu::nonnull(1)]]
static bfc_error_t emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
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

    bfc_error_t err = emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    if (imm < 0)
    {
        return bfc_codegen_emit_text(asm_prog, "    sub x19, x19, x16\n");
    }

    return bfc_codegen_emit_text(asm_prog, "    add x19, x19, x16\n");
}

/* Convert getchar()'s EOF result (-1) to the compiler's chosen zero byte. */
[[gnu::nonnull(1)]]
static bfc_error_t emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    bl   _getchar\n"
                  "    cmn  w0, #1\n"
                  "    csel w0, wzr, w0, eq\n"
                  "    strb w0, [x19]\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    ldrb w0, [x19]\n"
                  "    bl   _putchar\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
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

[[gnu::nonnull(1, 2)]]
static bfc_error_t emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    ldrb w16, [x19]\n"
        "    cbz  w16, %s\n",
        label
    );
}

[[gnu::nonnull(1, 2)]]
static bfc_error_t emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    ldrb w16, [x19]\n"
        "    cbnz w16, %s\n",
        label
    );
}

/*
 * Immutable macOS/AArch64 callback table exported to generic code generation.
 */
const bfc_backend_t BFC_BACKEND_MACOS_AARCH64 = {
    .target = {.arch = BFC_ARCH_AARCH64, .os = BFC_OS_MACOS},

    .emit_header       = emit_header,
    .emit_data_section = emit_data_section,
    .emit_symbol       = emit_symbol,
    .emit_end          = emit_end,

    .emit_op_add       = emit_op_add,
    .emit_op_move      = emit_op_move,
    .emit_op_get       = emit_op_get,
    .emit_op_put       = emit_op_put,
    .emit_op_set       = emit_op_set,
    .emit_loop_test_z  = emit_loop_test_z,
    .emit_loop_test_nz = emit_loop_test_nz,
};
