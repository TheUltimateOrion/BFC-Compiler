# bfc Backend Guide

[Project README](../README.md) · [Architecture](architecture.md) · [CLI reference](cli.md)

## Table of contents

- [Purpose](#purpose)
- [What a backend represents](#what-a-backend-represents)
- [Where a backend fits](#where-a-backend-fits)
- [Backend contract](#backend-contract)
- [Emission lifecycle](#emission-lifecycle)
- [Responsibility boundaries](#responsibility-boundaries)
- [Target design worksheet](#target-design-worksheet)
- [Implementation procedure](#implementation-procedure)
  - [1. Define the target](#1-define-the-target)
  - [2. Create the backend module](#2-create-the-backend-module)
  - [3. Implement program structure](#3-implement-program-structure)
  - [4. Implement tape storage](#4-implement-tape-storage)
  - [5. Implement IR operations](#5-implement-ir-operations)
  - [6. Implement loop branches](#6-implement-loop-branches)
  - [7. Register the backend](#7-register-the-backend)
  - [8. Validate generated assembly](#8-validate-generated-assembly)
  - [9. Add target-native tests](#9-add-target-native-tests)
- [Current backends](#current-backends)
- [Platform-specific requirements](#platform-specific-requirements)
- [Acceptance checklist](#acceptance-checklist)
- [Common implementation mistakes](#common-implementation-mistakes)

## Purpose

A backend converts generic Brainfuck IR into assembly for one exact architecture and operating-system combination.

This guide explains the complete work required to add a backend: defining the target, implementing the ABI and instruction lowering, registering the backend, and validating the generated program.

A backend is not architecture-only. Two targets using the same CPU architecture may still require different backends because their ABI, object format, symbol naming, relocation syntax, section directives, unwind rules, and external-call conventions differ.

## What a backend represents

A backend corresponds to one `bfc_target_t` pair:

```text
architecture + operating system
```

Examples:

```text
AArch64 + macOS
x86-64 + macOS
x86-64 + Linux
AArch64 + Windows
```

The target triple is parsed separately. A recognized target triple becomes usable only when:

1. a backend object exists,
2. the backend is compiled into `bfc`,
3. the backend is registered in the target-to-backend lookup, and
4. the emitted assembly is valid for the target toolchain.

## Where a backend fits

The generic compilation pipeline owns parsing, IR construction, optimization, and traversal:

```text
Brainfuck source
    -> tokens
    -> nested IR
    -> optimized IR
    -> generic codegen
    -> backend callbacks
    -> target assembly
```

The generic code generator visits each IR instruction and delegates target-specific emission to the selected backend.

The backend does not parse Brainfuck, build IR, manage loop recursion, allocate assembly buffers, or write the output file.

## Backend contract

Each backend provides one immutable `bfc_backend_t` object:

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

Every callback must be initialized. Generic codegen assumes the backend object is complete.

The backend object should have external linkage and remain immutable:

```c
const bfc_backend_t BFC_BACKEND_LINUX_X86_64 = {
    /* target and callbacks */
};
```

Individual emitter functions should remain `static` inside the backend source file.

## Emission lifecycle

Generic codegen calls backend functions in this order:

```text
emit_header()
emit_data_section()
emit_symbol()

emit operation callbacks while traversing IR
emit loop-test callbacks while traversing nested loops

emit_end()
```

The intended responsibilities are:

| Callback | Required result |
|---|---|
| `emit_header` | Select assembly dialect and emit file-level directives or external declarations |
| `emit_data_section` | Reserve the zero-initialized Brainfuck tape |
| `emit_symbol` | Emit the entry symbol, function prologue, and initial tape-pointer setup |
| `emit_op_add` | Apply a wrapping 8-bit change to the current cell |
| `emit_op_move` | Move the tape pointer by a signed byte offset |
| `emit_op_get` | Read one byte, convert EOF according to BFC semantics, and store it |
| `emit_op_put` | Pass the current byte to the platform output function |
| `emit_op_set` | Store a normalized byte value directly |
| `emit_loop_test_z` | Branch to the supplied label when the current cell is zero |
| `emit_loop_test_nz` | Branch to the supplied label when the current cell is nonzero |
| `emit_end` | Return success from `main`, restore preserved state, and emit final directives |

Loop labels themselves are emitted by generic codegen. Backend loop callbacks emit only the test and conditional branch.

## Responsibility boundaries

### Generic codegen owns

- assembly-buffer allocation and growth
- checked formatted emission
- backend lookup
- IR traversal
- recursive loop traversal
- unique loop-label generation
- label placement
- operation dispatch
- assembly-file writing
- `bfc_asm_t` destruction

### A backend owns

- assembly dialect
- object-format directives
- text, data, and BSS sections
- global and external symbol spelling
- ABI-compliant function entry and return
- callee-saved register preservation
- stack alignment
- tape-pointer and scratch-register selection
- target-specific immediate encoding
- lowering of `ADD`, `MOVE`, `GET`, `PUT`, and `SET`
- zero and nonzero loop branches
- external calls to input and output functions
- EOF conversion
- any target-required metadata or unwind directives

Do not place target-specific instructions or directives in generic codegen.

Do not duplicate `bfc_codegen_emit_text()` or `bfc_codegen_emitf()` inside backend modules.

## Target design worksheet

Complete this table before writing the backend. It prevents ABI and object-format details from being discovered piecemeal while coding.

| Property | Target decision |
|---|---|
| Target triple | Exact triple accepted by the CLI |
| Architecture enum | `BFC_ARCH_*` value |
| Operating-system enum | `BFC_OS_*` value |
| Object format | Mach-O, ELF, or PE/COFF |
| Assembly dialect | Apple/GNU, Intel, AT&T, MASM-compatible, or another required dialect |
| C symbol prefix | For example `_main` on Mach-O or `main` on ELF |
| Entry symbol | Exact emitted symbol |
| Input symbol | Exact `getchar` spelling |
| Output symbol | Exact `putchar` spelling |
| Tape symbol | Exact backend-private tape symbol |
| Tape section | Exact zero-initialized-data section and directives |
| Tape-pointer register | Must survive external calls or be restored |
| Scratch register | Must be legal for the selected ABI |
| First integer argument register | Required by `putchar` |
| Return-value register | Used by `getchar` and `main` |
| Stack alignment | Required alignment at each external call |
| Preserved registers | Registers the backend must save and restore |
| Small immediate range | Directly encodable movement or arithmetic range |
| Large immediate strategy | Scratch-register materialization sequence |
| Address materialization | Relocation sequence used for the tape symbol |
| Function return sequence | ABI-correct epilogue and return |
| Native test environment | Physical host, VM, container, or emulator |

This worksheet is part of backend design, not generated code. Keep the decisions reflected in the target section of this document.

## Implementation procedure

### 1. Define the target

Confirm that the architecture and operating-system enums exist in `bfc_target.h`.

Confirm that the target triple is recognized by `bfc_target_parse()` in `bfc_target.c`.

Target recognition and backend support are separate. It is valid for a triple to be recognized before its backend is implemented, but code generation must then return an unsupported-target error.

Do not add a second spelling for the same target unless the CLI intentionally supports aliases.

### 2. Create the backend module

Create one backend source file named for both the operating system and architecture:

```text
src/bfc_backend_<os>_<arch>.c
```

Examples:

```text
src/bfc_backend_linux_x86_64.c
src/bfc_backend_linux_aarch64.c
src/bfc_backend_windows_x86_64.c
```

Include:

```c
#include "bfc_codegen_internal.h"
```

The file should contain:

1. private `static` emitter functions,
2. any private immediate-lowering helpers,
3. any backend-local symbol or register constants, and
4. one externally visible immutable backend object.

Do not expose emitter functions in a public header.

### 3. Implement program structure

Implement these callbacks first:

```text
emit_header
emit_data_section
emit_symbol
emit_end
```

At the end of this phase, the backend should be able to emit an empty program that:

- assembles,
- links,
- enters `main`,
- initializes the tape pointer,
- returns success, and
- obeys the target ABI.

Validate an empty program before implementing Brainfuck operations. This isolates object-format, symbol, prologue, relocation, and stack-alignment problems.

### 4. Implement tape storage

Reserve exactly `BFC_TAPE_SIZE` bytes in a zero-initialized section.

The backend must:

- emit one tape symbol,
- guarantee zero initialization,
- obtain the tape address using valid target relocations,
- place the address in the chosen tape-pointer register, and
- preserve that pointer across `getchar()` and `putchar()` calls.

The tape pointer is a byte pointer. `IR_MOVE 1` advances exactly one Brainfuck cell.

### 5. Implement IR operations

Implement and validate operations in this order.

#### `IR_SET`

Normalize the immediate to one byte and store it directly.

Required cases:

```text
0
1
255
negative values after normalization
values larger than 255 after normalization
```

#### `IR_ADD`

Load or update the current byte so arithmetic wraps modulo 256.

The implementation must behave correctly for:

```text
+1
-1
+255
-255
large folded immediates
```

Do not allow a wider temporary result to change the required byte-wrapping semantics.

#### `IR_MOVE`

Move the tape pointer by a signed byte offset.

Support:

- zero movement,
- small positive movement,
- small negative movement,
- the largest directly encodable positive and negative values,
- values requiring a scratch register.

The backend does not perform tape-bounds checking unless the language semantics are changed globally.

#### `IR_PUT`

Load the current cell, zero-extend it to the ABI-required argument width, and call `putchar()`.

Passing a sign-extended byte is incorrect for cell values from 128 through 255.

#### `IR_GET`

Call `getchar()` and store the low byte of the result.

BFC currently maps EOF to zero. The backend must test the full return value before truncating it to one byte.

### 6. Implement loop branches

`emit_loop_test_z()` must branch to the supplied end label when the current byte is zero.

`emit_loop_test_nz()` must branch to the supplied start label when the current byte is nonzero.

Generic codegen owns:

- label allocation,
- start-label emission,
- body recursion,
- end-label emission.

Validate:

- an empty loop body,
- one loop,
- adjacent loops,
- deeply nested loops,
- labels beyond single-digit identifiers.

### 7. Register the backend

Declare the backend object in `bfc_codegen_internal.h` if backend objects are declared individually.

Add the target-to-backend mapping in the generic backend lookup used by `bfc_codegen()`.

The mapping must compare both:

```text
target.arch
target.os
```

Do not select a backend by architecture alone.

Ensure the new source file is included by the Makefile. If source files are discovered automatically, verify that the new object appears in the build. Otherwise, add it to the source list explicitly.

After registration:

```bash
bfc -S --target <triple> program.bf
```

must select the new backend instead of returning an unsupported-target error.

### 8. Validate generated assembly

Use a minimal progression so failures identify one subsystem at a time:

1. empty source
2. `+`
3. `-`
4. `>`
5. `<`
6. `.`
7. `,`
8. `[-]`
9. a single loop
10. nested loops
11. Hello World
12. large coalesced pointer movements
13. large coalesced cell adjustments

For every stage:

1. inspect the generated assembly when the output is unexpected,
2. assemble it with the target assembler,
3. link it with the target C runtime,
4. run it in a target-native environment, and
5. compare exit status and output with the expected result.

Assembly generation alone is not sufficient validation.

### 9. Add target-native tests

A backend is complete only when its generated programs execute successfully on the target.

Tests should cover:

| Area | Required coverage |
|---|---|
| Program structure | Empty program links and returns success |
| Cell arithmetic | Increment, decrement, overflow, and underflow |
| Pointer movement | Positive, negative, small, and large offsets |
| Output | Values below and above 127 |
| Input | Normal byte input and EOF-to-zero behavior |
| Loops | Zero-entry, repeated execution, and nesting |
| Optimized IR | Coalesced operations and `IR_SET` |
| ABI | Multiple input/output calls without tape-pointer corruption |
| Diagnostics | Unsupported targets still fail cleanly |
| Toolchain | Generated assembly assembles and links with the intended tools |

Cross-assembly on another host is useful, but it does not replace target-native execution.

## Current backends

### macOS AArch64

#### Target identity

```text
aarch64-apple-darwin
BFC_ARCH_AARCH64
BFC_OS_MACOS
Mach-O
Apple AArch64 assembly syntax
```

#### Symbols and sections

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

Tape-address materialization:

```asm
adrp x19, _bfc_tape@PAGE
add  x19, x19, _bfc_tape@PAGEOFF
```

#### Register plan

| Register | Role |
|---|---|
| `x19` | Tape pointer; callee-saved and preserved |
| `x16` | Large-immediate scratch |
| `w16` | Temporary cell value |
| `w0` | Integer argument and return value |
| `x29` | Frame pointer |
| `x30` | Link register |

#### Lowering summary

| IR operation | Strategy |
|---|---|
| `IR_ADD` | Load byte, add normalized immediate, store byte |
| `IR_MOVE` | Direct immediate up to the encodable limit; otherwise materialize in `x16` |
| `IR_GET` | Call `_getchar`, map EOF to zero, store low byte |
| `IR_PUT` | Load byte into `w0`, call `_putchar` |
| `IR_SET` | Store `wzr` for zero or an immediate byte |
| Loop tests | `cbz` and `cbnz` |

### macOS x86-64

#### Target identity

```text
x86_64-apple-darwin
BFC_ARCH_X86_64
BFC_OS_MACOS
Mach-O
Intel syntax
```

Dialect directive:

```asm
.intel_syntax noprefix
```

#### Symbols

```asm
_main
_getchar
_putchar
_bfc_tape
```

#### Register plan

| Register | Role |
|---|---|
| `rbx` | Tape pointer; callee-saved and preserved |
| `r11` | Large-immediate scratch |
| `eax` | Return value and input temporary |
| `edi` | First integer argument |
| `rbp` | Frame pointer |

#### Lowering summary

| IR operation | Strategy |
|---|---|
| `IR_ADD` | `add byte ptr [rbx], imm` |
| `IR_MOVE` | Direct immediate when possible; otherwise use `movabs r11` |
| `IR_GET` | Call `_getchar`, map EOF to zero, store `al` |
| `IR_PUT` | Zero-extend the byte into `edi`, call `_putchar` |
| `IR_SET` | Store an immediate byte |
| Loop tests | Compare the byte with zero, then use `je` or `jne` |

## Platform-specific requirements

### macOS

- Object format: Mach-O
- C symbols normally use a leading underscore
- Tape-address materialization uses Apple relocation syntax
- Backend must follow the Apple platform ABI
- AArch64 and x86-64 require separate instruction emitters

### Linux

- Object format: ELF
- C symbols normally have no leading underscore
- x86-64 normally follows the System V AMD64 ABI
- AArch64 normally follows AAPCS64
- ELF section and relocation syntax must be used
- Position-independent executable defaults may affect address materialization and linking

### Windows

- Object format: PE/COFF
- x86-64 and AArch64 follow Microsoft ABIs
- i386 has separate calling-convention and symbol-decoration concerns
- Stack shadow-space and unwind requirements may apply
- Assembly syntax depends on the selected toolchain
- Clang integrated assembler, GNU assembler, MASM, and ARMASM are not interchangeable
- Backend and toolchain support must be designed together

Do not create a new backend by copying an existing backend and changing only symbol names.

## Acceptance checklist

Use this checklist only after completing the implementation phases above.

### Target and registration

- [ ] Exact target triple is documented.
- [ ] Architecture and operating-system enums are correct.
- [ ] Backend object is immutable and externally visible.
- [ ] Every callback is initialized.
- [ ] Backend lookup matches both architecture and operating system.
- [ ] Backend source is included in the build.

### Object format and ABI

- [ ] Assembly dialect is explicitly selected.
- [ ] Text and zero-initialized-data sections are correct.
- [ ] Entry, tape, input, and output symbols are correct.
- [ ] Tape address uses valid target relocations.
- [ ] Function prologue and epilogue follow the ABI.
- [ ] Stack alignment is correct at every external call.
- [ ] Callee-saved registers are preserved.
- [ ] Function returns success correctly.

### Brainfuck semantics

- [ ] Tape contains exactly `BFC_TAPE_SIZE` zero-initialized bytes.
- [ ] Pointer movement is byte-based and signed.
- [ ] Cell arithmetic wraps to 8 bits.
- [ ] `IR_SET` normalizes values to one byte.
- [ ] `getchar()` EOF maps to zero.
- [ ] `putchar()` receives a zero-extended byte.
- [ ] Loop tests inspect exactly the current byte.

### Immediate and branch lowering

- [ ] Small positive immediates work.
- [ ] Small negative immediates work.
- [ ] Large positive immediates work.
- [ ] Large negative immediates work.
- [ ] Scratch-register use follows the ABI.
- [ ] Nested and adjacent loop labels assemble correctly.

### Validation

- [ ] Empty output assembles and links.
- [ ] Every individual Brainfuck operation executes correctly.
- [ ] Canonical programs produce expected output.
- [ ] Optimized IR produces the same behavior as unoptimized source.
- [ ] Repeated external calls do not corrupt the tape pointer.
- [ ] Target-native tests pass.
- [ ] Unsupported target combinations still produce a clear error.

## Common implementation mistakes

### Selecting by architecture only

An x86-64 macOS backend is not a valid x86-64 Linux or Windows backend. Always match the complete target.

### Truncating `getchar()` before checking EOF

`getchar()` returns an `int`. Test for EOF before storing the low byte.

### Sign-extending output bytes

Brainfuck cells are unsigned bytes. Zero-extend the cell before passing it to `putchar()`.

### Keeping the tape pointer in a caller-saved register

External calls may overwrite caller-saved registers. Use a preserved register or explicitly save and restore the pointer.

### Violating call-site stack alignment

A program may assemble and still crash when calling the C runtime if stack alignment or platform-specific call-frame rules are wrong.

### Assuming immediates have unlimited range

Every architecture limits directly encoded immediates. Test the boundary and provide a large-immediate fallback.

### Emitting target labels in generic codegen

Generic codegen owns abstract loop labels. Backend callbacks should only emit target-specific tests and branches to the supplied label.

### Validating only the assembly text

Readable assembly is not proof of correctness. Assemble, link, execute, and compare behavior on the target.
