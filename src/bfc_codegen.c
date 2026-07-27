#include "bfc_codegen.h"

#include <stdlib.h>

bfc_error_t bfc_codegen(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block)
{
    *asm_prog = calloc(1, sizeof(**asm_prog));
    if (!(*asm_prog)) { return BFC_ERR_ALLOC; }

    (*asm_prog)->capacity = 4096;

    (*asm_prog)->buffer   = malloc((*asm_prog)->capacity * sizeof(*(*asm_prog)->buffer));
    if (!(*asm_prog)->buffer) { return BFC_ERR_ALLOC; }

    (*asm_prog)->buffer[0] = '\0';

#if defined(__x86_64__) || defined(_M_X64)
    return bfc_codegen_x86_64(asm_prog, ir_block);
#elif defined(__i386__) || defined(_M_IX86)
    return bfc_codegen_i386(asm_prog, ir_block);
#elif defined(__aarch64__) || defined(_M_ARM64)
    return bfc_codegen_aarch64(asm_prog, ir_block);
#elif defined(__arm__) || defined(_M_ARM)
    return bfc_codegen_arm32(asm_prog, ir_block);
#else
    return bfc_make_error(ERR_INTERNAL, "Unknown architectur!");
#endif
}

bfc_error_t bfc_codegen_x86_64(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block)
{
    (*asm_prog)->arch = ARCH_X86_64;
    (void) ir_block;  // suppress unused parameter warning

    return bfc_make_error(ERR_INTERNAL, "x86_64 generation not supported yet!");
}

bfc_error_t bfc_codegen_i386(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block)
{
    (*asm_prog)->arch = ARCH_i386;
    (void) ir_block;  // suppress unused parameter warning

    return bfc_make_error(ERR_INTERNAL, "i386 generation not supported yet!");
}

bfc_error_t bfc_codegen_aarch64(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block)
{
    (*asm_prog)->arch = ARCH_aarch64;
    (void) ir_block;  // suppress unused parameter warning

    return BFC_ERR_OK;
}

bfc_error_t bfc_codegen_arm32(bfc_asm_t** asm_prog, bfc_ir_block_t const* const ir_block)
{
    (*asm_prog)->arch = ARCH_arm32;
    (void) ir_block;  // suppress unused parameter warning

    return bfc_make_error(ERR_INTERNAL, "arm32 generation not supported yet!");
}

void bfc_asm_destroy(bfc_asm_t** pasm_prog)
{
    if (!pasm_prog || !*pasm_prog) { return; }

    free((*pasm_prog)->buffer);
    free(*pasm_prog);

    *pasm_prog = nullptr;
}
