#include <inttypes.h>

#include "bfc_codegen_internal.h"
#include "bfc_config.h"

/**
 * @file bfc_backend_windows_x86_64.c
 * @brief Windows x86-64 PE/COFF assembly backend.
 *
 * @details
 * Implements the Microsoft x64 ABI in GNU AT&T syntax. R12 holds the Brainfuck
 * tape pointer across CRT calls, while R11 is used for large pointer offsets.
 */

/**
 * @brief Materializes a full-width unsigned immediate in scratch register `r11`.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
{
    return bfc_codegen_emitf(asm_prog, "    movabsq $0x%016" PRIx64 ", %%r11\n", value);
}

/** @brief Emits Windows x86-64 text-section directives. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, ".text\n.p2align 4\n");
}

/** @brief Declares the zero-initialized Brainfuck tape in PE/COFF BSS. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_data_section(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emitf(
        asm_prog,
        ".bss\n"
        ".lcomm .bfc_tape, %zu, 16\n",
        BFC_TAPE_SIZE
    );
}

/** @brief Emits the Windows x64 prologue, unwind metadata, and tape address. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_symbol(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
              ".globl main\n"
              ".def main; .scl 2; .type 32; .endef\n"
                  ".p2align 4\n"
                  "main:\n"
                  ".seh_proc main\n"
                  "    subq $40, %rsp\n"
                  "    .seh_stackalloc 40\n"
                  "    movq %r12, 32(%rsp)\n"
                  "    .seh_savereg %r12, 32\n"
                  "    .seh_endprologue\n"
                  "\n"
                  "    leaq .bfc_tape(%rip), %r12\n"
    );
}

/** @brief Emits the Windows x64 epilogue and zero process status. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    xorl %eax, %eax\n"
                  "    movq 32(%rsp), %r12\n"
                  "    .seh_startepilogue\n"
                  "    addq $40, %rsp\n"
                  "    .seh_endepilogue\n"
                  "    retq\n"
                  "    .seh_endproc\n"
    );
}

/** @brief Lowers wrapping byte-cell addition. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    if (normalized == 0)
    {
        return BFC_ERR_OK;
    }

    return bfc_codegen_emitf(asm_prog, "    addb $%u, (%%r12)\n", (unsigned) normalized);
}

/** @brief Moves the tape pointer using direct or register materialized immediates. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
{
    if (imm == 0)
    {
        return BFC_ERR_OK;
    }

    const uint64_t magnitude = imm < 0 ? UINT64_C(0) - (uint64_t) imm : (uint64_t) imm;

    if (magnitude <= INT32_MAX)
    {
        return bfc_codegen_emitf(
            asm_prog, imm < 0 ? "    subq $%" PRIu64 ", %%r12\n" : "    addq $%" PRIu64 ", %%r12\n",
            magnitude
        );
    }

    bfc_error_t err = windows_x86_64_emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    return bfc_codegen_emit_text(
        asm_prog, imm < 0 ? "    subq %r11, %r12\n" : "    addq %r11, %r12\n"
    );
}

/** @brief Calls `getchar`, maps EOF to zero, and stores one byte. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    callq getchar\n"
                  "    xorl %edx, %edx\n"
                  "    cmpl $-1, %eax\n"
                  "    cmove %edx, %eax\n"
                  "    movb %al, (%r12)\n"
    );
}

/** @brief Zero-extends the current cell and calls `putchar`. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    movzbl (%r12), %ecx\n"
                  "    callq putchar\n"
    );
}

/** @brief Stores a normalized byte value in the current cell. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t windows_x86_64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    return bfc_codegen_emitf(asm_prog, "    movb $%u, (%%r12)\n", (unsigned) normalized);
}

/** @brief Branches when the current cell is zero. @internal */
[[gnu::nonnull(1, 2)]]
static bfc_error_t windows_x86_64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    cmpb $0, (%%r12)\n    je %s\n", label);
}

/** @brief Branches when the current cell is nonzero. @internal */
[[gnu::nonnull(1, 2)]]
static bfc_error_t windows_x86_64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    cmpb $0, (%%r12)\n    jne %s\n", label);
}

/** @brief Immutable Windows x86-64 backend descriptor. */
const bfc_backend_t BFC_BACKEND_WINDOWS_X86_64 = {
    .target = {
        .arch = BFC_ARCH_X86_64,
        .os   = BFC_OS_WINDOWS,
    },

    .emit_header       = windows_x86_64_emit_header,
    .emit_data_section = windows_x86_64_emit_data_section,
    .emit_symbol       = windows_x86_64_emit_symbol,
    .emit_end          = windows_x86_64_emit_end,

    .emit_op_add       = windows_x86_64_emit_op_add,
    .emit_op_move      = windows_x86_64_emit_op_move,
    .emit_op_get       = windows_x86_64_emit_op_get,
    .emit_op_put       = windows_x86_64_emit_op_put,
    .emit_op_set       = windows_x86_64_emit_op_set,
    .emit_loop_test_z  = windows_x86_64_emit_loop_test_z,
    .emit_loop_test_nz = windows_x86_64_emit_loop_test_nz,
};
