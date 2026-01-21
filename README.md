# BFC-Compiler

BFC-Compiler is a blazingly fast Brainfuck compiler written in C, designed with a clean, no-nonsense command line experience inspired by clang. It compiles `.bf` programs to optimized output with an emphasis on speed, simplicity, and portability.

The entire compiler is implemented to be as minimal as possible with zero external dependencies.

## Highlights

- Blazingly fast compilation for Brainfuck programs.
- Clean CLI inspired by clang conventions.
- Brainfuck-specific optimizations to reduce instruction count and improve runtime performance.
- Fully self-contained: one-file implementation, standard library only, no third-party dependencies.
- Simple build: `make` produces a single `bfc` binary.

## Planned Usage

The CLI is intended to feel familiar if you have used clang:

```bash
# Compile hello.bf to a native executable (default behavior)
./bfc hello.bf -o hello

# Emit assembly only
./bfc -S hello.bf -o hello.s

# Emit object file only
./bfc -c hello.bf -o hello.o

# Keep intermediates when producing an executable
./bfc hello.bf -o hello --save-temps
```

## TODO

- [x] Clang-Style CLI 

  - [x] -o <file> output selection

  - [x] -S emit assembly

  - [x] --help

- [x] Language features:

  - [x] Line comments starting with ';' 

- [ ] Code generation:

  - [ ] Assembly generation from Brainfuck instructions

  - [ ] Assemble + link pipeline (produce executable)

  - [ ] Target selection (x86_64, arm64)

- [ ] Optimizations (Brainfuck-specific):

  - [ ] Coalesce repeated ops (+++++, -----)

  - [ ] Coalesce pointer moves (>>>>, <<<<)

  - [ ] Peephole optimizations for common patterns

  - [ ] Recognize clear loops ([-] / [+]) and optimize to direct store
  
- [ ] Diagnostics:

  - [x] Mismatched bracket errors

  - [x] Clear, user-facing messages for invalid inputs and file errors

- [ ] Tests:

  - [ ] Golden tests for canonical Brainfuck programs

  - [ ] Regression tests for optimizations

  - [ ] Negative tests for invalid syntax/bracket structure

- [ ] Documentation:

  - [ ] CLI reference and examples
