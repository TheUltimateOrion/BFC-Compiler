# bfc Backend Guide

## Purpose

A backend converts generic Brainfuck IR into assembly for one exact architecture and operating-system combination.

A backend is not architecture-only. ABI, object format, symbol naming, relocation syntax, section directives, and external-call conventions vary by platform.

## Backend contract

```c
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

    bfc_error_t (*emit_loop_test_z)(
        bfc_asm_t* asm_prog,
        const char* label
    );

    bfc_error_t (*emit_loop_test_nz)(
        bfc_asm_t* asm_prog,
        const char* label
    );
} bfc_backend_t;
```

Every callback must be initialized.

## Responsibility split

### Generic codegen owns

- assembly-buffer management
- formatted emission
- IR traversal
- loop-label creation
- recursive loop emission
- backend lookup
- assembly-file output

### A backend owns

- assembly dialect
- section directives
- symbol names
- ABI-compliant prologue and epilogue
- tape-pointer and scratch registers
- lowering of `ADD`, `MOVE`, `GET`, `PUT`, and `SET`
- zero/nonzero loop branches
- target-specific immediate encoding

Do not duplicate `bfc_codegen_emitf()` inside backend files.

## Current support

Implemented:

- `BFC_BACKEND_MACOS_AARCH64`
- `BFC_BACKEND_MACOS_X86_64`

Recognized but not implemented:

- Linux AArch64
- Linux x86-64
- Windows AArch64
- Windows x86-64
- Windows i386

A recognized triple is not usable until a matching backend is linked and selected.

## Shared Brainfuck semantics

Every backend must implement:

- 30,000-byte zero-initialized tape
- one 8-bit wrapping cell per position
- byte-based pointer movement
- modulo-256 `+` and `-`
- one-byte input and output
- zero-tested loops

## macOS AArch64

### Symbols and sections

Mach-O symbols:

```asm
_main
_getchar
_putchar
_bfc_tape
```

Tape section:

```asm
.section __DATA,__bss
```

Tape address:

```asm
adrp x19, _bfc_tape@PAGE
add  x19, x19, _bfc_tape@PAGEOFF
```

### Registers

| Register | Role |
|---|---|
| `x19` | tape pointer |
| `x16` | large-immediate scratch |
| `w16` | temporary cell value |
| `w0` | call argument/return |
| `x29` | frame pointer |
| `x30` | link register |

`x19` is callee-saved and preserved.

### Lowering

- `IR_ADD`: load byte, add normalized immediate, store byte
- `IR_MOVE`: direct immediate up to 4095, otherwise `movz`/`movk` into `x16`
- `IR_GET`: `_getchar`, map EOF to zero, store low byte
- `IR_PUT`: load byte into `w0`, call `_putchar`
- `IR_SET`: store `wzr` or an immediate byte
- loops: `cbz` / `cbnz`

## macOS x86-64

### Dialect and symbols

Intel syntax:

```asm
.intel_syntax noprefix
```

Mach-O symbols:

```asm
_main
_getchar
_putchar
_bfc_tape
```

### Registers

| Register | Role |
|---|---|
| `rbx` | tape pointer |
| `r11` | large-immediate scratch |
| `eax` | return/input temporary |
| `edi` | first integer argument |
| `rbp` | frame pointer |

`rbx` is callee-saved and preserved.

### Lowering

- `IR_ADD`: `add byte ptr [rbx], imm`
- `IR_MOVE`: direct immediate when possible, otherwise `movabs r11`
- `IR_GET`: `_getchar`, map EOF to zero, store `al`
- `IR_PUT`: zero-extend byte into `edi`, call `_putchar`
- `IR_SET`: immediate byte store
- loops: compare byte with zero, then `je` / `jne`

## Adding a backend

1. Choose the exact target.
2. Create one source file, for example:
   - `bfc_backend_linux_x86_64.c`
   - `bfc_backend_linux_aarch64.c`
3. Include `bfc_codegen_internal.h`.
4. Keep individual emitters `static`.
5. Define one immutable backend object with external linkage.
6. Declare that object in the internal header or generate declarations from an X macro.
7. Register the target/backend mapping.
8. Add or enable the matching target triple.
9. Assemble, link, and run target-native integration tests.

Example object:

```c
const bfc_backend_t BFC_BACKEND_LINUX_X86_64 = {
    .target = {
        .arch = BFC_ARCH_X86_64,
        .os   = BFC_OS_LINUX,
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
```

## Platform differences

### macOS

- Mach-O
- leading underscore on C symbols
- Apple relocation syntax
- Apple ABI

### Linux

- ELF
- normally no leading underscore
- ELF relocation syntax
- System V or AAPCS ABI

### Windows

- PE/COFF
- Microsoft ABI
- target-specific unwind and symbol requirements
- assembler dialect depends on Clang/GAS/MASM/ARMASM

Do not create a new backend by changing only symbol names.

## Completion checklist

- [ ] correct target identity
- [ ] all callbacks initialized
- [ ] tape zero-initialized
- [ ] tape pointer preserved across calls
- [ ] ABI stack alignment correct
- [ ] external symbols correct
- [ ] EOF behaviour defined
- [ ] `putchar()` receives a zero-extended byte
- [ ] cell arithmetic wraps to 8 bits
- [ ] small and large moves work
- [ ] nested loop labels assemble
- [ ] assembly assembles and links
- [ ] executable passes target-native tests
