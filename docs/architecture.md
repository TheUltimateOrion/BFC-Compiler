# bfc Architecture

[Project README](../README.md) · [Architecture](architecture.md) · [Backend guide](backends.md) · [CLI reference](cli.md)

## Table of contents

- [Purpose](#purpose)
- [Compilation pipeline](#compilation-pipeline)
- [Entry point](#entry-point)
- [Module responsibilities](#module-responsibilities)
  - [`bfc_cli`](#bfc_cli)
  - [`bfc_io`](#bfc_io)
  - [`bfc_token` and `bfc_lexer`](#bfc_token-and-bfc_lexer)
  - [`bfc_jumptable`](#bfc_jumptable)
  - [`bfc_ir`](#bfc_ir)
  - [`bfc_target`](#bfc_target)
  - [`bfc_codegen`](#bfc_codegen)
  - [Backend modules](#backend-modules)
  - [`bfc_error`](#bfc_error)
- [Ownership model](#ownership-model)
- [Assembly buffer](#assembly-buffer)
- [Loop generation](#loop-generation)
- [Important invariants](#important-invariants)

## Purpose

`bfc` is a C23 Brainfuck compiler. The current pipeline reads a Brainfuck source file, validates and tokenizes it, builds and optimizes a nested IR, selects a target-specific backend, and emits assembly text.

Current implemented backends:

- `aarch64-apple-darwin`
- `x86_64-apple-darwin`

The target parser also recognizes Linux and Windows triples, but those targets remain unavailable until their backend objects are implemented and registered.

## Compilation pipeline

```text
Brainfuck source
    -> CLI parsing
    -> source-file loading
    -> lexing
    -> bracket validation
    -> IR construction
    -> IR optimization
    -> target resolution
    -> backend selection
    -> assembly generation
    -> assembly-file output
```

A future final stage can assemble and link the emitted assembly:

```text
assembly -> object file -> executable
```

In the current implementation, `-S` writes the generated assembly file. The assemble-and-link stage is not yet part of the compiler.

## Entry point

`src/bfc.c` coordinates the pipeline:

1. `bfc_process_args()`
2. `bfc_program_create()`
3. `bfc_lex()`
4. `bfc_parse_jump_table()`
5. `bfc_ir_create()`
6. `bfc_ir_optimize_rep()`
7. `bfc_target_parse()` or `bfc_target_host()`
8. `bfc_codegen()`
9. `bfc_asm_write_file()` when `-S` is active

Cleanup attributes release allocated compiler objects when `main()` exits.

## Module responsibilities

### `bfc_cli`

Owns command-line parsing and help output.

Current options:

- `-h`, `--help`
- `-S`, `--assembly`
- `--fno-comments`
- `-o`, `--output`
- `-t`, `--target`

`bfc_args_t` stores borrowed pointers into `argv`; it does not own the input, output, or target strings.

### `bfc_io`

Owns source-file loading and line lookup.

`bfc_program_t` contains the copied path, source buffer, file size, and line count.

- Creator: `bfc_program_create()`
- Destructor: `bfc_program_destroy()`

### `bfc_token` and `bfc_lexer`

The token layer defines Brainfuck tokens and token streams.

The lexer:

- ignores non-Brainfuck characters
- optionally treats text after `;` as comments
- tracks one-based line and column positions
- emits one token per Brainfuck instruction

- Creator: `bfc_lex()`
- Destructor: `bfc_token_stream_destroy()`

### `bfc_jumptable`

Validates matching `[` and `]` tokens.

The current IR builder does not consume the resulting table; the module is presently a separate validation pass. It can be removed later if bracket validation moves into `bfc_ir_create()`.

### `bfc_ir`

Defines the persistent intermediate representation.

Current operations:

- `IR_ADD`
- `IR_MOVE`
- `IR_PUT`
- `IR_GET`
- `IR_SET`
- `IR_LOOP`

Loops contain nested `bfc_ir_block_t` objects.

- `bfc_ir_create()` builds the tree.
- `bfc_ir_optimize_rep()` folds adjacent `ADD`/`MOVE` instructions and converts simple clear loops such as `[-]` and `[+]` to `IR_SET 0`.
- `bfc_ir_destroy()` recursively frees the tree.

### `bfc_target`

Defines target architecture and operating-system types.

- `bfc_target_parse()` converts a target triple to `bfc_target_t`.
- `bfc_target_host()` detects the default host target.

Explicit `--target` selection should drive cross-target code generation; host detection is only the default.

### `bfc_codegen`

Owns generic assembly generation:

- backend lookup
- assembly-buffer allocation and growth
- generic loop-label generation
- IR traversal
- callback dispatch
- assembly-file output
- `bfc_asm_t` destruction

Target-specific syntax must remain in backend modules.

### Backend modules

Each backend defines one immutable `bfc_backend_t` object and private emitter functions.

Current backend files:

- `bfc_backend_macos_aarch64.c`
- `bfc_backend_macos_x86_64.c`

Backends own assembly syntax, symbol naming, ABI rules, register use, immediate lowering, sections, and branch syntax.

### `bfc_error`

Defines error codes, error constructors, and diagnostics.

`bfc_error_t` is `nodiscard`, so returned errors must be checked. Token-bearing errors can print file, line, column, source text, and a caret.

## Ownership model

| Object | Created by | Destroyed by |
|---|---|---|
| `bfc_program_t` | `bfc_program_create()` | `bfc_program_destroy()` |
| `bfc_token_stream_t` | `bfc_lex()` | `bfc_token_stream_destroy()` |
| jump table | `bfc_parse_jump_table()` | `bfc_jump_table_destroy()` |
| `bfc_ir_block_t` | `bfc_ir_create()` | `bfc_ir_destroy()` |
| `bfc_asm_t` | `bfc_codegen()` | `bfc_asm_destroy()` |

## Assembly buffer

`bfc_asm_t` stores:

```c
char*  buffer;
size_t length;
size_t capacity;
```

The buffer remains null-terminated; `length` excludes the terminator.

`bfc_codegen_emit_text()`:

1. computes the required size with checked arithmetic
2. grows capacity geometrically
3. reallocates the byte buffer
4. appends text
5. updates `length`

`bfc_codegen_emitf()` is the shared formatted-emission helper.

## Loop generation

Generic codegen owns loop structure:

1. allocate a unique label ID
2. emit the start label
3. ask the backend to branch to the end if the current cell is zero
4. recursively emit the body
5. ask the backend to branch to the start if the current cell is nonzero
6. emit the end label

This keeps loop structure architecture-independent.

## Important invariants

- Brainfuck cells are 8-bit bytes.
- The tape contains 30,000 cells.
- `CHAR_BIT` must equal 8.
- Loop bodies are recursively owned.
- Every backend callback must be non-null.
- Every successful creator has a matching destroy function.
- Target-specific syntax belongs only in backend files.
- Generic codegen must not assume Mach-O, ELF, COFF, AArch64, or x86-64 syntax.
