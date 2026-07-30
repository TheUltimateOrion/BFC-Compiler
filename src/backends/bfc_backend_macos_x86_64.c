#include "bfc_codegen_internal.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include "bfc_config.h"

[[gnu::nonnull(1)]]
static bfc_error_t emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
{
    return bfc_codegen_emitf(asm_prog, "    movabs r11, 0x%016" PRIx64 "\n", value);
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".intel_syntax noprefix\n"
                  ".text\n"
                  ".p2align 4\n"
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

[[gnu::nonnull(1)]]
static bfc_error_t emit_symbol(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
                  ".globl _main\n"
                  ".p2align 4\n"
                  "_main:\n"
                  "    push rbp\n"
                  "    mov  rbp, rsp\n"
                  "    push rbx\n"
                  "    sub  rsp, 8\n"
                  "\n"
                  "    lea  rbx, [rip + _bfc_tape]\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    xor  eax, eax\n"
                  "    add  rsp, 8\n"
                  "    pop  rbx\n"
                  "    pop  rbp\n"
                  "    ret\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    if (normalized == 0)
    {
        return BFC_ERR_OK;
    }

    return bfc_codegen_emitf(asm_prog, "    add byte ptr [rbx], %u\n", (unsigned) normalized);
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
{
    if (imm == 0)
    {
        return BFC_ERR_OK;
    }

    const uint64_t magnitude = imm < 0 ? UINT64_C(0) - (uint64_t) imm : (uint64_t) imm;

    if (magnitude <= INT32_MAX)
    {
        return bfc_codegen_emitf(
            asm_prog, imm < 0 ? "    sub rbx, %" PRIu64 "\n" : "    add rbx, %" PRIu64 "\n",
            magnitude
        );
    }

    bfc_error_t err = emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    return bfc_codegen_emit_text(asm_prog, imm < 0 ? "    sub rbx, r11\n" : "    add rbx, r11\n");
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    call _getchar\n"
                  "    xor  edx, edx\n"
                  "    cmp  eax, -1\n"
                  "    cmove eax, edx\n"
                  "    mov  byte ptr [rbx], al\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    movzx edi, byte ptr [rbx]\n"
                  "    call  _putchar\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    return bfc_codegen_emitf(asm_prog, "    mov byte ptr [rbx], %u\n", (unsigned) normalized);
}

[[gnu::nonnull(1, 2)]]
static bfc_error_t emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    cmp byte ptr [rbx], 0\n"
        "    je  %s\n",
        label
    );
}

[[gnu::nonnull(1, 2)]]
static bfc_error_t emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    cmp byte ptr [rbx], 0\n"
        "    jne %s\n",
        label
    );
}

const bfc_backend_t BFC_BACKEND_MACOS_X86_64 = {
    .target = {
        .arch = BFC_ARCH_X86_64,
        .os   = BFC_OS_MACOS,
    },

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
