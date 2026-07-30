# BFC Compiler

BFC is a Brainfuck compiler written in C23. It parses Brainfuck source code, builds and optimizes an intermediate representation, and emits target-specific assembly.

The project is intentionally small and self-contained. It uses only the C standard library and platform toolchains; no third-party runtime libraries are required.

## Current status

BFC currently supports assembly generation for:

- `aarch64-apple-darwin`
- `x86_64-apple-darwin`

The target parser also recognizes Linux and Windows target triples, but those backends are not yet implemented.

The compiler currently emits assembly with `-S`. The final assemble-and-link stage that produces a native executable is planned but not yet complete.

## Features

- C23 implementation
- Clang-style command-line interface
- Brainfuck tokenization with source locations
- Optional semicolon line comments
- Bracket validation and diagnostics
- Nested intermediate representation
- Repeated-operation optimization
- Repeated pointer-movement optimization
- Clear-loop optimization for `[-]` and `[+]`
- Target-triple selection
- macOS AArch64 backend
- macOS x86-64 backend
- AddressSanitizer-enabled debug builds
- Doxygen-compatible source documentation

## Requirements

To build BFC:

- A C23-capable compiler
  - Clang is currently the primary tested compiler
  - GCC may work if it supports the required C23 and GNU attribute features
- GNU Make or a compatible `make`
- macOS for executing and linking the currently implemented backends

Optional:

- Doxygen, for generating HTML documentation

## Building

The default configuration is `debug`:

```bash
make
```

Explicit debug build:

```bash
make debug
```

Release build:

```bash
make release
```

Generated binaries are placed under:

```text
build/debug/bfc
build/release/bfc
```

Clean build output:

```bash
make clean
```

## Usage

```text
bfc [options] <file.bf>
```

### Emit assembly for the host target

```bash
./build/debug/bfc -S hello.bf
```

Default output:

```text
hello.bf.s
```

### Choose the output path

```bash
./build/debug/bfc -S hello.bf -o hello.s
```

### Select a target

```bash
./build/debug/bfc \
    --target aarch64-apple-darwin \
    -S \
    hello.bf \
    -o hello-aarch64.s
```

```bash
./build/debug/bfc \
    --target x86_64-apple-darwin \
    -S \
    hello.bf \
    -o hello-x86_64.s
```

### Display help

```bash
./build/debug/bfc --help
```

### Display the version

```bash
./build/debug/bfc --version
```

## Command-line options

| Option | Description |
|---|---|
| `-h`, `--help` | Display help |
| `-v`, `--version` | Display the compiler version |
| `-S`, `--assembly` | Emit assembly and stop |
| `-o`, `--output <file>` | Set the output path |
| `-t`, `--target <triple>` | Select the target triple |
| `--fno-comments` | Disable semicolon line-comment handling |
| `--` | Stop parsing command-line options |

For complete CLI documentation, see [`docs/cli.md`](docs/cli.md).

## Supported target triples

| Target triple | Status |
|---|---|
| `aarch64-apple-darwin` | Implemented |
| `x86_64-apple-darwin` | Implemented |
| `aarch64-unknown-linux-gnu` | Recognized; backend not implemented |
| `x86_64-unknown-linux-gnu` | Recognized; backend not implemented |
| `aarch64-pc-windows-msvc` | Recognized; backend not implemented |
| `x86_64-pc-windows-msvc` | Recognized; backend not implemented |
| `i386-pc-windows-msvc` | Recognized; backend not implemented |

If no target is supplied, BFC selects the host architecture and operating system.

## Brainfuck semantics

BFC currently uses:

- 30,000 cells
- 8-bit wrapping cells
- One byte per cell
- EOF input converted to zero
- Nested loop blocks in the IR

Semicolon comments are enabled by default:

```brainfuck
+++++ ; this text is ignored
.
```

Use `--fno-comments` to disable that extension.

## Optimizations

Implemented optimizations include:

- Coalescing adjacent cell operations
- Coalescing adjacent pointer movements
- Clear-loop recognition for `[-]` and `[+]`

## Documentation

Project documentation is available under `docs/`:

- [`docs/architecture.md`](docs/architecture.md)
- [`docs/backends.md`](docs/backends.md)
- [`docs/cli.md`](docs/cli.md)

Generate Doxygen HTML documentation with:

```bash
make docs
```

Generated documentation is written to:

```text
docs/doxygen/html/
```

The generated HTML is not committed to the repository.

## Project structure

```text
.
├── docs/
├── include/
├── src/
├── tests/
├── Doxyfile
├── LICENSE
├── Makefile
├── README.md
└── VERSION
```

## Roadmap

- Add the assemble-and-link stage
- Add Linux x86-64 code generation
- Add Linux AArch64 code generation
- Add Windows code-generation backends
- Expand integration and regression tests
- Add additional Brainfuck-specific optimizations
- Add target-aware external toolchain selection

## License

BFC is licensed under the GNU General Public License v3.0.

See [`LICENSE`](LICENSE) for the complete license text.
