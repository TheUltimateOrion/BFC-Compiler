# bfc Command-Line Interface

## Synopsis

```text
bfc [options] <file.bf>
```

`bfc` accepts one Brainfuck source file and optional compiler flags.

## Options

### `-h`, `--help`

Print the available command-line options and exit successfully.

```bash
bfc --help
```

### `-S`, `--assembly`

Generate assembly and stop.

```bash
bfc -S hello.bf
```

When `-o` is not provided, the current implementation appends `.s` to the complete input path:

```text
hello.bf      -> hello.bf.s
tests/a.bf    -> tests/a.bf.s
```

Use `-o` to select a different assembly-file path:

```bash
bfc -S hello.bf -o hello.s
```

> The current implementation only writes output when `-S` is present. The
> assemble-and-link stage for producing a final executable is not implemented
> yet.

### `-o`, `--output <file>`

Set the output path.

```bash
bfc -S hello.bf -o build/hello.s
```

The value must be supplied as the next argument.

Supported:

```bash
bfc -o hello.s -S hello.bf
bfc --output hello.s -S hello.bf
```

Not currently supported:

```bash
bfc -ohello.s hello.bf
bfc --output=hello.s hello.bf
```

Specifying the output option more than once is an error.

### `-t`, `--target <triple>`

Select the code-generation target.

```bash
bfc -S --target aarch64-apple-darwin hello.bf
```

When no target is provided, `bfc` selects the host architecture and operating
system using compiler predefined macros.

The target value must be supplied as the next argument. The
`--target=<triple>` form is not currently supported.

### `--fno-comments`

Disable `bfc`'s semicolon-comment extension.

By default, a semicolon begins a comment that continues to the end of the
current line:

```brainfuck
+++++ ; this text is ignored
.
```

With `--fno-comments`, the semicolon is treated like any other non-Brainfuck
character and does not suppress later Brainfuck instructions on that line.

```bash
bfc -S --fno-comments source.bf
```

### `--`

Stop parsing options.

This allows an input path beginning with `-`:

```bash
bfc -S -- -program.bf
```

Everything after `--` is treated as a positional argument.

## Target triples

The target parser currently recognizes:

| Target triple | Architecture | Operating system |
|---|---|---|
| `aarch64-apple-darwin` | AArch64 | macOS |
| `x86_64-apple-darwin` | x86-64 | macOS |
| `aarch64-unknown-linux-gnu` | AArch64 | Linux |
| `x86_64-unknown-linux-gnu` | x86-64 | Linux |
| `aarch64-pc-windows-msvc` | AArch64 | Windows |
| `x86_64-pc-windows-msvc` | x86-64 | Windows |
| `i386-pc-windows-msvc` | i386 | Windows |

Recognizing a triple does not guarantee that a backend is implemented.

Currently implemented backend selections:

| Target triple | Backend status |
|---|---|
| `aarch64-apple-darwin` | Implemented |
| `x86_64-apple-darwin` | Implemented |
| Linux targets | Recognized, backend not yet selected |
| Windows targets | Recognized, backend not yet selected |

Requesting a recognized target without an available backend produces an error.

## Examples

### Emit assembly for the host target

```bash
bfc -S hello.bf
```

Output:

```text
hello.bf.s
```

### Emit assembly to a selected path

```bash
bfc -S hello.bf -o hello.s
```

### Emit macOS AArch64 assembly

```bash
bfc -S \
    --target aarch64-apple-darwin \
    hello.bf \
    -o hello-aarch64.s
```

### Emit macOS x86-64 assembly

```bash
bfc -S \
    --target x86_64-apple-darwin \
    hello.bf \
    -o hello-x86_64.s
```

### Compile a source path beginning with `-`

```bash
bfc -S -- -example.bf
```

## Argument rules

- Exactly one input path is accepted.
- The input path may appear before, between, or after options.
- Options requiring values consume the following argument.
- Repeating `-o` or `--target` is an error.
- An unknown option is an error.
- Omitting the input file is an error, except when help is requested.
- `--` disables option parsing for all remaining arguments.

## Exit status

`bfc` returns:

- `EXIT_SUCCESS` when compilation succeeds.
- `EXIT_SUCCESS` after printing help.
- `EXIT_FAILURE` when argument parsing, input loading, lexing, validation, IR
  construction, optimization, code generation, or output writing fails.

## Current limitations

- Combined short options are not supported.
- Attached option values are not supported.
- `--option=value` syntax is not supported.
- Only one input file is accepted.
- The final assembler/linker stage is not implemented.
- Linux and Windows target triples are recognized but do not yet have selected
  backends.
- The default assembly output keeps the original extension and appends `.s`.
