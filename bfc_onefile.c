/**
 * @file bfc_onefile.c
 * @brief Single-file C23 implementation of the bfc Brainfuck compiler.
 *
 * This translation unit combines the public interfaces, internal compiler
 * contracts, frontend pipeline, target handling, generic code generator, and
 * currently implemented macOS and Linux backends. The modular source tree remains the
 * preferred layout for ongoing development.
 *
 * @details
 * The compiler pipeline is: CLI parsing, source loading, lexing, bracket
 * validation, IR construction, IR optimization, target selection, backend
 * dispatch, and assembly emission.
 */
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * bfc_common.h
 * ========================================================================== */
/**
 * Returns the number of elements in an actual array.
 *
 * This macro must not be used with a pointer because sizeof(pointer) does not
 * describe the size of the pointed-to allocation.
 */
#define BFC_ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

/* ==========================================================================
 * bfc_config.h
 * ========================================================================== */
/**
 * Global compiler configuration shared by the frontend and all backends.
 *
 * The current machine model assumes one Brainfuck cell occupies one C byte.
 * Backends also rely on that byte containing exactly eight bits.
 */
static_assert(CHAR_BIT == 8, "bfc requires 8-bit bytes");

/** Number of byte-sized cells in the generated Brainfuck tape. */
#define BFC_TAPE_SIZE ((size_t) 30000)

/** Initial capacities for dynamically growing compiler data structures. */
#define BFC_INITIAL_ASM_CAPACITY ((size_t) 4096)
#define BFC_INITIAL_IR_CAPACITY ((size_t) 10)
#define BFC_INITIAL_IR_STACK_CAPACITY ((size_t) 5)

/* ==========================================================================
 * bfc_token.h
 * ========================================================================== */
/**
 * Single source of truth for Brainfuck characters and their token kinds.
 * Consumers may redefine X to generate related tables or switch cases.
 */
#define TOKEN_MAP         \
    X(TT_INC, '+')        \
    X(TT_DEC, '-')        \
    X(TT_PTR_RIGHT, '>')  \
    X(TT_PTR_LEFT, '<')   \
    X(TT_LOOP_START, '[') \
    X(TT_LOOP_END, ']')   \
    X(TT_OUTPUT, '.')     \
    X(TT_INPUT, ',')

/** Token kinds emitted by the lexer. */
typedef enum
{
#define X(tok_type, ...) tok_type,
    TOKEN_MAP
#undef X
} bfc_token_type_t;

/**
 * One Brainfuck token and its one-based source location.
 * line and col are used when reporting source diagnostics.
 */
typedef struct
{
    bfc_token_type_t type;
    uint32_t         line;
    uint32_t         col;
} bfc_token_t;

/**
 * Owning token sequence produced by bfc_lex().
 * Release it with bfc_token_stream_destroy().
 */
typedef struct
{
    bfc_token_t* tokens;
    size_t       length;
} bfc_token_stream_t;

/** Construct a token value without allocating memory. */
[[nodiscard, gnu::const]]
bfc_token_t
bfc_make_token(bfc_token_type_t const tok_type, uint32_t const line, uint32_t const col);

/**
 * Release a token stream and set the caller's pointer to null.
 * Passing null or a pointer to null is permitted.
 */
void bfc_token_stream_destroy(bfc_token_stream_t** ptok_stream);

/* ==========================================================================
 * bfc_error.h
 * ========================================================================== */
/** ANSI escape sequences used by the command-line diagnostic formatter. */
#define COL_OFF "\033[m"
#define COL_INFO "\033[1;1m"
#define COL_ERROR "\033[1;31m"

/** Single source of truth for compiler error codes. */
#define ERROR_LIST            \
    X(ERR_OK)                 \
    X(ERR_ARGS)               \
    X(ERR_IO)                 \
    X(ERR_MISMATCHED_BRACKET) \
    X(ERR_MISSING_BRACKET)    \
    X(ERR_ALLOC)              \
    X(ERR_INTERNAL)

typedef enum
{
#define X(name) name,
    ERROR_LIST
#undef X
} bfc_err_code_t;

/**
 * Error value returned throughout the compiler.
 *
 * token is meaningful for source-related diagnostics and is zero-initialized
 * for errors that do not refer to a particular source location. The type is
 * nodiscard so every returned compiler error must be checked.
 */
typedef struct [[nodiscard("bfc_error_t result must be checked")]]
{
    bfc_err_code_t code;
    char           msg[512];
    bfc_token_t    token;
} bfc_error_t;

/** Frequently returned error constants. */
#define BFC_ERR_OK ((bfc_error_t) {.code = ERR_OK, .msg = {0}, .token = {0}})
#define BFC_ERR_ALLOC                                                                      \
    ((bfc_error_t) {.code = ERR_ALLOC, .msg = "Memory allocation failure!", .token = {0}})

/** Forward declaration avoids requiring the complete source-program structure. */
struct bfc_program_t;

/** Create a formatted error message without a source token. */
[[gnu::nonnull(2), gnu::format(printf, 2, 3)]]
bfc_error_t bfc_make_errorf(bfc_err_code_t error_code, const char* format, ...);

/** Create a formatted error message associated with a source token. */
[[gnu::nonnull(3), gnu::format(printf, 3, 4)]]
bfc_error_t
bfc_make_errorf_with_token(bfc_err_code_t error_code, bfc_token_t token, const char* format, ...);

/** Create an error by copying an existing message string. */
bfc_error_t bfc_make_error(bfc_err_code_t const error_code, char const* msg);

/** Create a token-associated error by copying an existing message string. */
bfc_error_t bfc_make_error_with_token(
    bfc_err_code_t const error_code,
    char const*          msg,
    bfc_token_t const    token
);

/** Return the symbolic name of an error code, such as "ERR_IO". */
[[nodiscard, gnu::const, gnu::returns_nonnull]]
char const* bfc_get_error_code(bfc_err_code_t const error_code);

/**
 * Print a user-facing diagnostic to stderr.
 * Source-related errors include the source line and a caret when available.
 */
[[gnu::cold]]
void bfc_log_error(bfc_error_t err, const struct bfc_program_t* program);

/* ==========================================================================
 * bfc_target.h
 * ========================================================================== */
/** Architectures understood by target parsing and backend selection. */
typedef enum
{
    BFC_ARCH_X86_64,
    BFC_ARCH_I386,
    BFC_ARCH_AARCH64,
    BFC_ARCH_ARM32,
} bfc_arch_t;

/** Operating systems that may contribute ABI and object-format differences. */
typedef enum
{
    BFC_OS_WINDOWS,
    BFC_OS_MACOS,
    BFC_OS_LINUX,
} bfc_os_t;

/** Complete code-generation target used to select one backend. */
typedef struct
{
    bfc_arch_t arch;
    bfc_os_t   os;
} bfc_target_t;

/**
 * Parse a supported target triple into target.
 * target is written only when the triple is recognized.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_target_parse(bfc_target_t* target, const char* triple);

/** Return the architecture and operating system of the compiler host. */
[[gnu::const]]
bfc_target_t bfc_target_host(void);

/* ==========================================================================
 * bfc_cli.h
 * ========================================================================== */
/**
 * Parsed command-line state.
 *
 * input, output, and target are borrowed pointers into argv and must not be
 * modified or freed. do_assemble means "emit assembly and stop" for -S.
 */
typedef struct
{
    bool do_assemble;
    bool ask_help;
    bool f_no_comments;

    const char* input;
    const char* output;
    const char* target;
} bfc_args_t;

/** Print command-line usage and the available options to stdout. */
void bfc_cmd_help(void);

/**
 * Parse argv into cmd_args.
 *
 * Exactly one input path is accepted unless help is requested. The special
 * argument "--" stops option parsing. No allocations are performed.
 */
[[gnu::nonnull(1)]]
bfc_error_t bfc_process_args(bfc_args_t* cmd_args, int argc, char* const argv[]);

/* ==========================================================================
 * bfc_io.h
 * ========================================================================== */
/**
 * Owning representation of one loaded Brainfuck source file.
 * path and buffer are allocated by bfc_program_create().
 */
typedef struct bfc_program_t
{
    char*  path;
    char*  buffer;
    size_t file_size;
    size_t line_count;
} bfc_program_t;

/**
 * Load file_path into a newly allocated program object.
 * On success, the caller owns *program and must destroy it.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_program_create(bfc_program_t** program, char const* file_path);

/**
 * Release a source program and set the caller's pointer to null.
 * Passing null or a pointer to null is permitted.
 */
void bfc_program_destroy(bfc_program_t** pprogram);

/** Return a borrowed pointer to the basename within program->path. */
[[nodiscard, gnu::pure, gnu::nonnull(1), gnu::returns_nonnull]]
char const* bfc_program_getname(bfc_program_t const* const program);

/**
 * Return a newly allocated copy of the requested one-based source line.
 * The caller must free the returned string. Null indicates an invalid line or
 * an allocation failure.
 */
[[nodiscard, gnu::malloc, gnu::nonnull(1)]]
char* bfc_program_getline(bfc_program_t const* const program, size_t const n);

/* ==========================================================================
 * bfc_lexer.h
 * ========================================================================== */
/**
 * Tokenize a loaded source program.
 *
 * Non-Brainfuck characters are ignored. Unless f_no_comments is set, a
 * semicolon suppresses the remainder of its source line. On success, the
 * caller owns *token_stream and must release it with
 * bfc_token_stream_destroy().
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_lex(
    bfc_token_stream_t**       token_stream,
    bfc_program_t const* const program,
    bfc_args_t const           cmd_args
);

/* ==========================================================================
 * bfc_jumptable.h
 * ========================================================================== */
/**
 * Validate matching loop brackets and build a token-index jump table.
 *
 * Each bracket entry stores the index of its matching bracket; non-bracket
 * entries contain -1. On success, the caller owns *jump_table.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_parse_jump_table(int64_t** jump_table, bfc_token_stream_t const* const tok_stream);

/** Release a jump table and set the caller's pointer to null. */
void bfc_jump_table_destroy(int64_t** pjump_table);

/* ==========================================================================
 * bfc_ir.h
 * ========================================================================== */
/** Operations represented by the persistent intermediate representation. */
typedef enum
{
    IR_ADD,  /* Add a signed immediate to the current cell. */
    IR_MOVE, /* Move the tape pointer by a signed byte offset. */
    IR_PUT,  /* Output the current cell. */
    IR_GET,  /* Read one input byte into the current cell. */
    IR_SET,  /* Set the current cell to an immediate byte value. */
    IR_LOOP  /* Execute a nested block while the current cell is nonzero. */
} bfc_ir_token_type_t;

typedef struct bfc_ir_block bfc_ir_block_t;

/**
 * One IR instruction.
 *
 * val.imm is used by IR_ADD, IR_MOVE, and IR_SET. val.body is used by IR_LOOP
 * and is owned recursively by the containing IR block.
 */
typedef struct
{
    bfc_ir_token_type_t op;

    union
    {
        int64_t         imm;
        bfc_ir_block_t* body;
    } val;
} bfc_ir_instr_t;

/** Owning, dynamically sized sequence of IR instructions. */
struct bfc_ir_block
{
    bfc_ir_instr_t* instructions;
    size_t          length;
    size_t          capacity;
};

/** Construct an immediate-bearing IR instruction without allocating memory. */
[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_imm_instr(bfc_ir_token_type_t const ir_token_type, int64_t const imm);

/** Construct an IR instruction whose value union is zero-initialized. */
[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_zero_instr(bfc_ir_token_type_t const ir_token_type);

/**
 * Build a nested IR tree from the token stream.
 * On success, the caller owns *root_block and must destroy it recursively.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_ir_create(bfc_ir_block_t** root_block, bfc_token_stream_t const* const tok_stream);

/**
 * Replace *ir_block with an optimized equivalent block.
 * Current optimizations fold repeated ADD/MOVE operations and clear loops.
 */
[[gnu::nonnull(1)]]
bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t** ir_block);

/** Recursively release an IR tree and set the caller's pointer to null. */
void bfc_ir_destroy(bfc_ir_block_t** proot_block);

/* ==========================================================================
 * bfc_codegen.h
 * ========================================================================== */
/** Opaque generated-assembly object owned by the codegen module. */
typedef struct bfc_asm bfc_asm_t;

/**
 * Generate target-specific assembly for an optimized IR tree.
 * On success, the caller owns *out_asm and must destroy it.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen(bfc_asm_t** out_asm, const bfc_ir_block_t* ir_block, bfc_target_t target);

/** Release generated assembly and set the caller's pointer to null. */
void bfc_asm_destroy(bfc_asm_t** asm_prog);

/** Write the generated assembly buffer to path without taking ownership. */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_asm_write_file(const bfc_asm_t* asm_prog, const char* path);

/* ==========================================================================
 * bfc_memory.h
 * ========================================================================== */
/**
 * Allocate an array of count elements, each element_size bytes wide.
 *
 * The multiplication is checked before allocation. A null pointer is returned
 * when the byte count overflows or the allocation fails.
 */
[[gnu::malloc, gnu::alloc_size(1, 2)]]
void* bfc_malloc_array(size_t count, size_t element_size);

/**
 * Allocate and zero-initialize an array using checked size multiplication.
 * Returns null on arithmetic overflow or allocation failure.
 */
[[gnu::malloc, gnu::alloc_size(1, 2)]]
void* bfc_calloc_array(size_t count, size_t element_size);

/**
 * Resize an existing array using checked size multiplication.
 *
 * On failure, the original allocation remains valid and must still be freed by
 * the caller. Passing a null allocation has the same effect as malloc.
 */
[[gnu::alloc_size(2, 3)]]
void* bfc_realloc_array(void* allocation, size_t count, size_t element_size);

/**
 * Typed convenience wrappers. The pointer expression supplies only the element
 * type for malloc/calloc and is not evaluated there. The realloc form evaluates
 * the pointer once as the allocation argument.
 */
#define BFC_MALLOC_ARRAY(pointer, count) bfc_malloc_array((count), sizeof(*(pointer)))
#define BFC_CALLOC_ARRAY(pointer, count) bfc_calloc_array((count), sizeof(*(pointer)))
#define BFC_REALLOC_ARRAY(pointer, count) bfc_realloc_array((pointer), (count), sizeof(*(pointer)))

/* ==========================================================================
 * bfc_codegen_internal.h
 * ========================================================================== */
/**
 * @internal
 * @brief Target backend dispatch table.
 *
 * Every callback must be non-null. The generic code generator owns IR
 * traversal and loop labels; the backend owns assembly syntax, ABI details,
 * register use, and instruction lowering.
 */
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

    bfc_error_t (*emit_loop_test_z)(bfc_asm_t* asm_prog, const char* label);
    bfc_error_t (*emit_loop_test_nz)(bfc_asm_t* asm_prog, const char* label);
} bfc_backend_t;

/**
 * @internal
 * @brief Mutable assembly-generation state.
 *
 * buffer is always null-terminated. length excludes the terminator and must be
 * less than capacity. label_id supplies unique labels for nested loops.
 */
struct bfc_asm
{
    const bfc_backend_t* backend;

    size_t label_id;

    char*  buffer;
    size_t length;
    size_t capacity;
};

/** @internal Immutable backend descriptors implemented by platform-specific modules. */
extern const bfc_backend_t BFC_BACKEND_WINDOWS_X86_64;
extern const bfc_backend_t BFC_BACKEND_WINDOWS_I386;
extern const bfc_backend_t BFC_BACKEND_WINDOWS_AARCH64;

extern const bfc_backend_t BFC_BACKEND_MACOS_AARCH64;
extern const bfc_backend_t BFC_BACKEND_MACOS_X86_64;

extern const bfc_backend_t BFC_BACKEND_LINUX_AARCH64;
extern const bfc_backend_t BFC_BACKEND_LINUX_X86_64;

/** @internal Append raw text to the dynamically growing assembly buffer. */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen_emit_text(bfc_asm_t* asm_prog, const char* text);

/** @internal Recursively lower one IR block through the selected backend. */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen_emit_block(bfc_asm_t* asm_prog, const bfc_ir_block_t* ir_block);

/** @internal Format and append one assembly fragment using printf-compatible arguments. */
[[gnu::nonnull(1, 2), gnu::format(printf, 2, 3)]]
bfc_error_t bfc_codegen_emitf(bfc_asm_t* asm_prog, const char* format, ...);

/* ==========================================================================
 * bfc_memory.c
 * ========================================================================== */
/**
 * @defgroup memory_impl Checked memory allocation implementation
 * @internal
 * @{
 * Overflow-checked allocation helpers for typed arrays.
 *
 * Each function validates count * element_size before delegating to the C
 * allocator. A nullptr result therefore represents either arithmetic overflow
 * or an allocation failure.
 */

/**
 * @brief Allocates an uninitialized typed array after checked multiplication.
 *
 * @param count Number of elements.
 * @param element_size Size of each element.
 * @return Allocated storage, or null on overflow/allocation failure.
 */
void* bfc_malloc_array(size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return malloc(bytes);
}

/**
 * @brief Allocates a zero-initialized typed array after checked multiplication.
 *
 * @param count Number of elements.
 * @param element_size Size of each element.
 * @return Zeroed storage, or null on overflow/allocation failure.
 */
void* bfc_calloc_array(size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return calloc(1, bytes);
}

/**
 * @brief Resizes a typed array after checked multiplication.
 *
 * On failure, the original allocation remains valid. The caller must assign
 * the result through a temporary pointer when preserving that allocation.
 *
 * @param allocation Existing allocation, or null.
 * @param count New element count.
 * @param element_size Size of each element.
 * @return Resized storage, or null on overflow/allocation failure.
 */
void* bfc_realloc_array(void* allocation, size_t count, size_t element_size)
{
    size_t bytes;

    if (ckd_mul(&bytes, count, element_size))
    {
        return nullptr;
    }

    return realloc(allocation, bytes);
}

/* ==========================================================================
 * bfc_token.c
 * ========================================================================== */
/**
 * @defgroup token_impl Token implementation
 * @internal
 * @{
 * Token construction and token-stream lifetime management.
 *
 * Token streams own their token arrays. Destruction accepts a pointer to the
 * caller's pointer so the released handle can be reset to nullptr.
 */

/**
 * @brief Constructs a complete token value without exposing partial initialization.
 *
 * @param tok_type Token kind.
 * @param line One-based source line.
 * @param col One-based source column.
 * @return The initialized token.
 */
bfc_token_t bfc_make_token(bfc_token_type_t const tok_type, uint32_t const line, uint32_t const col)
{
    return (bfc_token_t) {.type = tok_type, .line = line, .col = col};
}

/**
 * @brief Releases a token stream and clears the caller's pointer.
 *
 * Both the token array and its containing stream are released. Null input is
 * accepted so the function is suitable for cleanup attributes.
 *
 * @param ptok_stream Pointer to the owned token-stream pointer.
 */
void bfc_token_stream_destroy(bfc_token_stream_t** ptok_stream)
{
    if (!ptok_stream || !*ptok_stream)
    {
        return;
    }

    free((*ptok_stream)->tokens);
    free(*ptok_stream);

    *ptok_stream = nullptr;
}

/* ==========================================================================
 * bfc_error.c
 * ========================================================================== */
/**
 * @defgroup error_impl Error and diagnostic implementation
 * @internal
 * @{
 * Error construction and diagnostics.
 *
 * Errors may carry a source token. Bracket diagnostics use that token to print
 * the source location, the corresponding source line, and a caret marker.
 */

/**
 * @brief Constructs an error and formats its message into owned storage.
 *
 * @param error_code Error classification.
 * @param format `printf`-style diagnostic format.
 * @param ... Values consumed by `format`.
 * @return The formatted error.
 */
bfc_error_t bfc_make_errorf(bfc_err_code_t error_code, const char* format, ...)
{
    bfc_error_t err = {
        .code = error_code,
    };

    va_list args;
    va_start(args, format);

    vsnprintf(err.msg, sizeof(err.msg), format, args);

    va_end(args);

    return err;
}

/**
 * @brief Constructs a formatted error associated with a source token.
 *
 * @param error_code Error classification.
 * @param token Source location associated with the error.
 * @param format `printf`-style diagnostic format.
 * @param ... Values consumed by `format`.
 * @return The formatted token-associated error.
 */
bfc_error_t
bfc_make_errorf_with_token(bfc_err_code_t error_code, bfc_token_t token, const char* format, ...)
{
    bfc_error_t err = {
        .code  = error_code,
        .token = token,
    };

    va_list args;
    va_start(args, format);

    vsnprintf(err.msg, sizeof(err.msg), format, args);

    va_end(args);

    return err;
}

/**
 * @brief Constructs an error by copying a diagnostic message.
 *
 * @param error_code Error classification.
 * @param msg Message to copy, or null for an empty message.
 * @return The initialized error.
 */
bfc_error_t bfc_make_error(bfc_err_code_t const error_code, char const* msg)
{
    bfc_error_t err = {0};

    err.code = error_code;

    if (msg)
    {
        snprintf(err.msg, sizeof(err.msg), "%s", msg);
    }

    return err;
}

/**
 * @brief Constructs an error by copying a message and source token.
 *
 * @param error_code Error classification.
 * @param msg Message to copy, or null for an empty message.
 * @param token Source location associated with the error.
 * @return The initialized token-associated error.
 */
bfc_error_t
bfc_make_error_with_token(bfc_err_code_t const error_code, char const* msg, bfc_token_t const token)
{
    bfc_error_t err = {0};

    err.code  = error_code;
    err.token = token;

    if (msg)
    {
        snprintf(err.msg, sizeof(err.msg), "%s", msg);
    }

    return err;
}

/**
 * @brief Converts an error enum to its symbolic spelling.
 *
 * `ERROR_LIST` keeps this switch synchronized with the enum definition.
 *
 * @param error_code Error code to name.
 * @return The symbolic spelling, or `Unknown error` for an invalid value.
 */
char const* bfc_get_error_code(bfc_err_code_t const error_code)
{
    switch (error_code)
    {
#define X(name)       \
    case name: {      \
        return #name; \
    }                 \
    break;
        ERROR_LIST
#undef X

        default: {
            return "Unknown error";
        }
        break;
    }
}

/**
 * @brief Logs a compiler diagnostic to standard error.
 *
 * Bracket errors include the source line and caret location. Other errors use
 * a compact compiler-style message.
 *
 * @param err Error to report.
 * @param program Source program used to resolve bracket locations.
 */
void bfc_log_error(bfc_error_t const err, const struct bfc_program_t* const program)
{
    if (err.code == ERR_MISSING_BRACKET || err.code == ERR_MISMATCHED_BRACKET)
    {
        fprintf(
            stderr,
            COL_INFO "%s[%" PRIu32 ", %" PRIu32 "]: " COL_ERROR "%s" COL_OFF COL_INFO
                     ": %s\n" COL_OFF,
            bfc_program_getname(program), err.token.line, err.token.col,
            bfc_get_error_code(err.code), err.msg
        );

        char* line_buf = bfc_program_getline(program, (size_t) err.token.line);

        if (line_buf)
        {
            int line_num_width = (err.token.line > 0) ? (int) log10(err.token.line) + 1 : 1;

            fprintf(stderr, "   %zu | %s\n", (size_t) err.token.line, line_buf);

            fprintf(stderr, "   %*s | %*c\n", line_num_width, "", (int) err.token.col, '^');

            free(line_buf);
        }

        return;
    }

    fprintf(
        stderr, COL_INFO "bfc: " COL_ERROR "%s" COL_OFF COL_INFO ": %s\n" COL_OFF,
        bfc_get_error_code(err.code), err.msg
    );
}

/* ==========================================================================
 * bfc_target.c
 * ========================================================================== */
/**
 * @defgroup target_impl Target parsing and host detection implementation
 * @internal
 * @{
 * Target-triple parsing and host-target detection.
 *
 * The target table describes recognized triples. Backend availability is
 * checked separately by code generation, so a recognized triple may still be
 * rejected when its backend is not linked into bfc.
 */

/* One exact command-line triple mapped to bfc's compact target identity. */
typedef struct
{
    const char*  name;
    bfc_target_t target;
} bfc_target_entry_t;

/*
 * Keep aliases or additional triples here. Adding an entry only makes parsing
 * succeed; a matching backend must also be registered in bfc_codegen.c.
 */
static const bfc_target_entry_t BFC_TARGETS[] = {
    {
        .name = "aarch64-apple-darwin",
        .target = {
            .arch = BFC_ARCH_AARCH64,
            .os   = BFC_OS_MACOS,
        },
    },
    {
        .name = "x86_64-apple-darwin",
        .target = {
            .arch = BFC_ARCH_X86_64,
            .os   = BFC_OS_MACOS,
        },
    },
    {
        .name = "aarch64-unknown-linux-gnu",
        .target = {
            .arch = BFC_ARCH_AARCH64,
            .os   = BFC_OS_LINUX,
        },
    },
    {
        .name = "x86_64-unknown-linux-gnu",
        .target = {
            .arch = BFC_ARCH_X86_64,
            .os   = BFC_OS_LINUX,
        },
    },
    {
        .name = "aarch64-pc-windows-msvc",
        .target = {
            .arch = BFC_ARCH_AARCH64,
            .os   = BFC_OS_WINDOWS,
        },
    },
    {
        .name = "x86_64-pc-windows-msvc",
        .target = {
            .arch = BFC_ARCH_X86_64,
            .os   = BFC_OS_WINDOWS,
        },
    },
    {
        .name = "i386-pc-windows-msvc",
        .target = {
            .arch = BFC_ARCH_I386,
            .os   = BFC_OS_WINDOWS,
        },
    },
};

/**
 * @brief Parses an exact supported target triple.
 *
 * @param target Destination target structure.
 * @param triple Target triple to parse.
 * @return `BFC_ERR_OK` when recognized; an argument error otherwise.
 */
bfc_error_t bfc_target_parse(bfc_target_t* target, const char* triple)
{
    for (size_t i = 0; i < BFC_ARRAY_LENGTH(BFC_TARGETS); ++i)
    {
        if (strcmp(triple, BFC_TARGETS[i].name) == 0)
        {
            *target = BFC_TARGETS[i].target;
            return BFC_ERR_OK;
        }
    }

    return bfc_make_error(ERR_ARGS, "Unknown or unsupported target triple");
}

/**
 * @brief Returns the architecture and operating system of the compiler host.
 *
 * Predefined compiler macros describe the machine running bfc, not an
 * arbitrary cross-compilation target.
 *
 * @return The host target used when `--target` is omitted.
 */
bfc_target_t bfc_target_host(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    const bfc_arch_t arch = BFC_ARCH_AARCH64;
#elif defined(__x86_64__) || defined(_M_X64)
    const bfc_arch_t arch = BFC_ARCH_X86_64;
#elif defined(__i386__) || defined(_M_IX86)
    const bfc_arch_t arch = BFC_ARCH_I386;
#elif defined(__arm__) || defined(_M_ARM)
    const bfc_arch_t arch = BFC_ARCH_ARM32;
#else
    #error Unsupported host architecture
#endif

#if defined(__APPLE__)
    const bfc_os_t os = BFC_OS_MACOS;
#elif defined(__linux__)
    const bfc_os_t os = BFC_OS_LINUX;
#elif defined(_WIN32)
    const bfc_os_t os = BFC_OS_WINDOWS;
#else
    #error Unsupported host operating system
#endif

    return (bfc_target_t) {
        .arch = arch,
        .os   = os,
    };
}

/* ==========================================================================
 * bfc_cli.c
 * ========================================================================== */
/**
 * @defgroup cli_impl Command-line implementation
 * @internal
 * @{
 * Table-driven command-line parser and help generator.
 *
 * Each option descriptor supplies its spellings, optional value name,
 * description, and state-update handler. The same table drives parsing and
 * formatted help output.
 */

/*
 * Handlers mutate parsed state. value is nullptr for flag options and points
 * into argv for options that consume a value.
 */
typedef bfc_error_t (*bfc_option_handler_t)(bfc_args_t* args, const char* value);

typedef struct
{
    const char*          short_name;
    const char*          long_name;
    const char*          value_name;
    const char*          description;
    bfc_option_handler_t handler;
} bfc_option_t;

[[gnu::nonnull(1)]]
/** @brief Sets the help-requested flag. @internal */
static bfc_error_t bfc_set_help(bfc_args_t* args, const char* value)
{
    (void) value;

    args->ask_help = true;
    return BFC_ERR_OK;
}

[[gnu::nonnull(1)]]
/** @brief Sets the assembly-only flag. @internal */
static bfc_error_t bfc_set_assemble(bfc_args_t* args, const char* value)
{
    (void) value;

    args->do_assemble = true;
    return BFC_ERR_OK;
}

[[gnu::nonnull(1)]]
/** @brief Disables semicolon comment handling. @internal */
static bfc_error_t bfc_set_no_comments(bfc_args_t* args, const char* value)
{
    (void) value;

    args->f_no_comments = true;
    return BFC_ERR_OK;
}

[[gnu::nonnull(1, 2)]]
/** @brief Stores the output path option. @internal */
static bfc_error_t bfc_set_output(bfc_args_t* args, const char* value)
{
    if (args->output)
    {
        return bfc_make_error(ERR_ARGS, "Output file specified more than once!");
    }

    args->output = value;
    return BFC_ERR_OK;
}

[[gnu::nonnull(1, 2)]]
/** @brief Stores the target-triple option. @internal */
static bfc_error_t bfc_set_target(bfc_args_t* args, const char* value)
{
    if (args->target)
    {
        return bfc_make_error(ERR_ARGS, "Target specified more than once");
    }

    args->target = value;
    return BFC_ERR_OK;
}

/*
 * Single source of truth for accepted spellings and generated help text.
 * Order here is the order displayed by bfc_cmd_help().
 */
static const bfc_option_t BFC_OPTIONS[] = {
    {
     .short_name  = "-h",
     .long_name   = "--help",
     .value_name  = nullptr,
     .description = "Display available options",
     .handler     = bfc_set_help,
     },
    {
     .short_name  = "-S",
     .long_name   = "--assembly",
     .value_name  = nullptr,
     .description = "Only run compilation steps",
     .handler     = bfc_set_assemble,
     },
    {
     .short_name  = nullptr,
     .long_name   = "--fno-comments",
     .value_name  = nullptr,
     .description = "Do not treat lines starting with ';' as comments",
     .handler     = bfc_set_no_comments,
     },
    {
     .short_name  = "-o",
     .long_name   = "--output",
     .value_name  = "file",
     .description = "Write output to <file>",
     .handler     = bfc_set_output,
     },
    {
     .short_name  = "-t",
     .long_name   = "--target",
     .value_name  = "triple",
     .description = "Compile for the given target <triple>",
     .handler     = bfc_set_target,
     }
};

/**
 * @brief Finds an option by its complete short or long spelling.
 * @param argument Argument spelling to search for.
 * @return Matching option, or null when no option matches.
 * @internal
 */
[[gnu::pure, gnu::nonnull(1)]]
static const bfc_option_t* bfc_find_option(const char* argument)
{
    for (size_t i = 0; i < BFC_ARRAY_LENGTH(BFC_OPTIONS); ++i)
    {
        const bfc_option_t* option = &BFC_OPTIONS[i];

        if (option->short_name && strcmp(argument, option->short_name) == 0)
        {
            return option;
        }

        if (option->long_name && strcmp(argument, option->long_name) == 0)
        {
            return option;
        }
    }

    return nullptr;
}

/**
 * @brief Prints command-line usage generated from `BFC_OPTIONS`.
 */
void bfc_cmd_help(void)
{
    printf("OVERVIEW: bfc Brainfuck compiler\n\n");
    printf("USAGE: bfc [options] <file.bf>\n\n");
    printf("OPTIONS:\n");

    for (size_t i = 0; i < BFC_ARRAY_LENGTH(BFC_OPTIONS); ++i)
    {
        const bfc_option_t* option = &BFC_OPTIONS[i];
        char                usage[64];

        if (option->short_name && option->long_name)
        {
            if (option->value_name)
            {
                snprintf(
                    usage, sizeof(usage), "%s <%s>, %s <%s>", option->short_name,
                    option->value_name, option->long_name, option->value_name
                );
            }
            else
            {
                snprintf(usage, sizeof(usage), "%s, %s", option->short_name, option->long_name);
            }
        }
        else
        {
            const char* name = option->short_name ? option->short_name : option->long_name;

            if (option->value_name)
            {
                snprintf(usage, sizeof(usage), "%s <%s>", name, option->value_name);
            }
            else
            {
                snprintf(usage, sizeof(usage), "%s", name);
            }
        }

        printf("  %-30s %s\n", usage, option->description);
    }
}

/**
 * @brief Parses command-line arguments into compiler state.
 *
 * Exactly one positional input path is accepted. `--` permanently disables
 * option parsing. Options with `value_name` consume the following argv
 * element; attached forms such as `--target=value` are not handled.
 *
 * @param cmd_args Destination command-line state.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return `BFC_ERR_OK` on success or an argument error.
 */
bfc_error_t bfc_process_args(bfc_args_t* cmd_args, int argc, char* const argv[])
{
    *cmd_args = (bfc_args_t) {0};

    bool parse_options = true;

    for (int i = 1; i < argc; ++i)
    {
        const char* argument = argv[i];

        if (parse_options && strcmp(argument, "--") == 0)
        {
            parse_options = false;
            continue;
        }

        if (parse_options && argument[0] == '-')
        {
            const bfc_option_t* option = bfc_find_option(argument);

            if (!option)
            {
                return bfc_make_errorf(ERR_ARGS, "Unknown argument: '%s'", argument);
            }

            const char* value = nullptr;

            if (option->value_name)
            {
                if (i + 1 >= argc)
                {
                    char error_message[512];

                    snprintf(
                        error_message, sizeof(error_message),
                        "Argument to '%s' is missing; expected <%s>", argument, option->value_name
                    );

                    return bfc_make_error(ERR_ARGS, error_message);
                }

                value = argv[++i];
            }

            bfc_error_t err = option->handler(cmd_args, value);
            if (err.code != ERR_OK)
            {
                return err;
            }

            if (cmd_args->ask_help)
            {
                return BFC_ERR_OK;
            }

            continue;
        }

        if (cmd_args->input)
        {
            return bfc_make_error(ERR_ARGS, "Too many input file paths given!");
        }

        cmd_args->input = argument;
    }

    if (!cmd_args->input)
    {
        return bfc_make_error(ERR_ARGS, "No input file provided!");
    }

    return BFC_ERR_OK;
}

/* ==========================================================================
 * bfc_io.c
 * ========================================================================== */
/**
 * @defgroup io_impl Source-file input implementation
 * @internal
 * @{
 * Source-file loading and source-line access.
 *
 * bfc_program_t owns a copied path and a null-terminated source buffer.
 * bfc_program_getline() returns a separate allocation owned by its caller.
 */

/**
 * @brief Loads a complete Brainfuck source file into an owning program object.
 *
 * Ownership is transferred through `program` only after every allocation and
 * read succeeds. The source buffer is null-terminated.
 *
 * @param program Destination program pointer.
 * @param file_path Path to the source file.
 * @return `BFC_ERR_OK` on success or an I/O/allocation error.
 */
bfc_error_t bfc_program_create(bfc_program_t** program, char const* file_path)
{
    FILE* file_handle;

    if ((file_handle = fopen(file_path, "rb")))
    {
        bfc_program_t* prog = nullptr;
        prog                = BFC_CALLOC_ARRAY(prog, 1);
        if (!prog)
        {
            fclose(file_handle);

            return BFC_ERR_ALLOC;
        }

        int seek_status = fseek(file_handle, 0, SEEK_END);
        if (seek_status != 0)
        {
            free(prog);
            fclose(file_handle);

            return bfc_make_error(ERR_IO, "Unable to seek the end of file!");
        }

        long file_size = ftell(file_handle);
        if (file_size == -1L)
        {
            free(prog);
            fclose(file_handle);

            return bfc_make_error(ERR_IO, "Unable to perform ftell on file!");
        }

        seek_status = fseek(file_handle, 0, SEEK_SET);
        if (seek_status != 0)
        {
            free(prog);
            fclose(file_handle);

            return bfc_make_error(ERR_IO, "Unable to seek the start of file!");
        }

        /*
         * Validate conversion from ftell()'s signed long to size_t and reserve
         * one additional byte for the null terminator.
         */
        if (file_size < 0 || (uintmax_t) file_size > (uintmax_t) (SIZE_MAX - 1))
        {
            free(prog);
            fclose(file_handle);

            return bfc_make_error(ERR_IO, "Invalid file size!");
        }

        const size_t file_size_bytes = (size_t) file_size;

        prog->buffer = malloc(file_size_bytes + 1);
        if (!prog->buffer)
        {
            free(prog);
            fclose(file_handle);

            return BFC_ERR_ALLOC;
        }

        prog->file_size = file_size_bytes;

        prog->path = strdup(file_path);

        if (!prog->path)
        {
            free(prog->buffer);
            free(prog);
            fclose(file_handle);

            return BFC_ERR_ALLOC;
        }

        size_t end = fread(prog->buffer, sizeof(char), prog->file_size, file_handle);
        if (ferror(file_handle) != 0 || end != (size_t) prog->file_size)
        {
            free(prog->path);
            free(prog->buffer);
            free(prog);
            fclose(file_handle);

            return bfc_make_errorf(ERR_IO, "Unable to read from file '%s'!", file_path);
        }

        prog->buffer[end] = '\0';

        fclose(file_handle);

        /*
         * Count logical lines. A nonempty final line counts even when the file
         * does not end with a newline.
         */
        prog->line_count = 0;

        for (size_t i = 0; i < prog->file_size; ++i)
        {
            if (prog->buffer[i] == '\n')
            {
                ++prog->line_count;
            }
        }

        if (prog->file_size > 0 && prog->buffer[prog->file_size - 1] != '\n')
        {
            ++prog->line_count;
        }

        *program = prog;
        return BFC_ERR_OK;
    }

    return bfc_make_errorf(ERR_IO, "No such file or directory: '%s'", file_path);
}

/**
 * @brief Releases a source program and clears the caller's pointer.
 * @param pprogram Pointer to the owned program pointer.
 */
void bfc_program_destroy(bfc_program_t** pprogram)
{
    if (!pprogram || !*pprogram)
    {
        return;
    }

    free((*pprogram)->path);
    free((*pprogram)->buffer);
    free(*pprogram);

    *pprogram = nullptr;
}

/**
 * @brief Returns the borrowed basename of a source path.
 *
 * Both POSIX and Windows path separators are recognized. No allocation occurs.
 *
 * @param program Source program.
 * @return Borrowed basename within `program->path`.
 */
char const* bfc_program_getname(bfc_program_t const* program)
{
    char const* name = program->path;

    for (char const* p = program->path; *p != '\0'; ++p)
    {
        if (*p == '/' || *p == '\\')
        {
            name = p + 1;
        }
    }

    return name;
}

/**
 * @brief Returns an allocated copy of a one-based source line.
 *
 * @param program Source program.
 * @param n One-based line number.
 * @return Newly allocated line copy, or null for an invalid line/allocation failure.
 */
char* bfc_program_getline(bfc_program_t const* const program, size_t const n)
{
    if (n == 0 || n > program->line_count)
    {
        return nullptr;
    }

    size_t      current_line = 1;
    char const* start        = program->buffer;
    char const* end          = program->buffer;

    while (current_line < n)
    {
        end = strchr(start, '\n');
        if (end == nullptr)
        {
            return nullptr;
        }

        start = end + 1;
        ++current_line;
    }

    end = strchr(start, '\n');

    const size_t line_len = end ? (size_t) (end - start) : strlen(start);

    char* line_buf = malloc(line_len + 1);

    if (!line_buf)
    {
        return nullptr;
    }

    memcpy(line_buf, start, line_len);
    line_buf[line_len] = '\0';

    return line_buf;
}

/* ==========================================================================
 * bfc_lexer.c
 * ========================================================================== */
/**
 * @defgroup lexer_impl Lexer implementation
 * @internal
 * @{
 * Brainfuck lexer.
 *
 * The lexer scans the complete source buffer once, records one-based source
 * locations, and emits tokens only for Brainfuck instructions. By default, a
 * semicolon starts a line comment unless --fno-comments is active.
 */

/**
 * @brief Lexes Brainfuck instructions from a loaded source program.
 *
 * Non-Brainfuck characters are ignored. Semicolon comments are handled
 * according to `cmd_args.f_no_comments`.
 *
 * @param token_stream Destination owning token stream.
 * @param program Loaded source program.
 * @param cmd_args Command-line options affecting lexing.
 * @return `BFC_ERR_OK` on success or an allocation error.
 */
bfc_error_t bfc_lex(
    bfc_token_stream_t**       token_stream,
    bfc_program_t const* const program,
    bfc_args_t const           cmd_args
)
{
    bfc_error_t err = BFC_ERR_ALLOC;

    *token_stream = nullptr;

    bfc_token_stream_t* tok_stream = nullptr;
    tok_stream                     = BFC_CALLOC_ARRAY(tok_stream, 1);

    if (!tok_stream)
    {
        goto end;
    }

    if (program->file_size == 0)
    {
        *token_stream = tok_stream;
        tok_stream    = nullptr;
        err           = BFC_ERR_OK;
        goto end;
    }

    tok_stream->tokens = BFC_MALLOC_ARRAY(tok_stream->tokens, program->file_size);
    if (!tok_stream->tokens)
    {
        goto end;
    }

    size_t token_list_size = 0;
    size_t buffer_index    = 0;

    uint32_t line = 1;
    uint32_t col  = 1;

    bool in_comment = false;

/*
 * Comment suppression is centralized here so token cases remain generated
 * directly from TOKEN_MAP.
 */
#define EMIT_TOKEN(toktype)                                                                        \
    if (!in_comment) tok_stream->tokens[token_list_size++] = bfc_make_token((toktype), line, col);

    while (program->buffer[buffer_index] != '\0')
    {
        switch (program->buffer[buffer_index])
        {
            case ';': {
                if (cmd_args.f_no_comments)
                {
                    break;
                }

                in_comment = true;
            }
            break;

#define X(tok_type, tok_char) \
    case tok_char: {          \
        EMIT_TOKEN(tok_type); \
    }                         \
    break;
                TOKEN_MAP
#undef X

#if defined(_WIN32) || defined(_WIN64)
            case '\r': {
                ++buffer_index;
                continue;
            }
            break;
#endif
            case '\n': {
                ++line;
                col = 1;
                ++buffer_index;
                in_comment = false;
                continue;
            }
            break;

            default: break;
        }

        ++buffer_index;
        ++col;
    }

#undef EMIT_TOKEN

    /*
     * Shrinking is optional: if realloc fails, the original larger token array
     * remains valid and compilation can continue.
     */
    if (token_list_size > 0)
    {
        bfc_token_t* tmp = BFC_REALLOC_ARRAY(tok_stream->tokens, token_list_size);

        if (tmp)
        {
            tok_stream->tokens = tmp;
        }
    }
    else
    {
        free(tok_stream->tokens);
        tok_stream->tokens = nullptr;
    }

    tok_stream->length = token_list_size;

    *token_stream = tok_stream;

    tok_stream = nullptr;

    err = BFC_ERR_OK;

end:
    /* Free only the local, not-yet-transferred stream on failure. */
    if (tok_stream)
    {
        free(tok_stream->tokens);
        free(tok_stream);
    }

    return err;
}

/* ==========================================================================
 * bfc_jumptable.c
 * ========================================================================== */
/**
 * @defgroup jumptable_impl Bracket validation implementation
 * @internal
 * @{
 * Bracket validation and jump-table construction.
 *
 * An explicit stack records unmatched '[' token indices. Each matched pair is
 * stored bidirectionally in the resulting table; non-bracket entries retain
 * the -1 sentinel.
 */

/**
 * @brief Validates matching brackets and builds a token-index jump table.
 *
 * Matched brackets contain each other's indices; all other entries retain the
 * `-1` sentinel.
 *
 * @param jump_table Destination owning jump table.
 * @param tok_stream Token stream to validate.
 * @return `BFC_ERR_OK` on success or a bracket/allocation error.
 */
bfc_error_t bfc_parse_jump_table(int64_t** jump_table, bfc_token_stream_t const* const tok_stream)
{
    *jump_table = nullptr;

    bfc_error_t err;

    size_t n = tok_stream->length;
    if (n == 0)
    {
        return BFC_ERR_OK;
    }

    bfc_token_t const* toks = tok_stream->tokens;

    int64_t* jtable = nullptr;
    jtable          = BFC_MALLOC_ARRAY(jtable, n);

    if (!jtable)
    {
        return BFC_ERR_ALLOC;
    }

    /* Initialize the sentinel for non-bracket tokens. */
    for (size_t i = 0; i < n; ++i)
    {
        jtable[i] = -1;
    }

    size_t* stack = nullptr;
    stack         = BFC_MALLOC_ARRAY(stack, n);
    if (!stack)
    {
        free(jtable);

        return BFC_ERR_ALLOC;
    }

    size_t sp = 0;

    size_t i;
    size_t j;
    /*
     * Push opening-bracket indices. A closing bracket pops the latest opening
     * bracket, which naturally validates nesting.
     */
    for (i = 0; i < n; ++i)
    {
        if (toks[i].type == TT_LOOP_START)
        {
            stack[sp++] = i;
        }
        else if (toks[i].type == TT_LOOP_END)
        {
            if (sp == 0)
            {
                goto extra_closing_bracket;
            }
            j = stack[--sp];

            jtable[j] = (int64_t) i;
            jtable[i] = (int64_t) j;
        }
    }

    if (sp != 0)
    {
        goto missing_closing_bracket;
    }

    free(stack);

    *jump_table = jtable;
    return BFC_ERR_OK;

extra_closing_bracket:
    err = bfc_make_errorf_with_token(
        ERR_MISMATCHED_BRACKET, toks[i], "Found an extra ']' at line %" PRIu32 ".", toks[i].line
    );

    free(stack);
    free(jtable);

    return err;

missing_closing_bracket:
    err = bfc_make_errorf_with_token(
        ERR_MISMATCHED_BRACKET, toks[stack[sp - 1]],
        "Missing a closing bracket ']' for opening bracket '[' at line %" PRIu32 ".",
        toks[stack[sp - 1]].line
    );

    free(stack);
    free(jtable);

    return err;
}

/**
 * @brief Releases a jump table and clears the caller's pointer.
 * @param pjump_table Pointer to the owning jump-table pointer.
 */
void bfc_jump_table_destroy(int64_t** pjump_table)
{
    if (!pjump_table || !*pjump_table)
    {
        return;
    }

    free(*pjump_table);

    *pjump_table = nullptr;
}

/* ==========================================================================
 * bfc_ir.c
 * ========================================================================== */
/**
 * @defgroup ir_impl Intermediate-representation implementation
 * @internal
 * @{
 * Intermediate-representation construction, optimization, and destruction.
 *
 * Loops are represented as recursively owned IR blocks. Construction uses an
 * explicit block stack; optimization produces a replacement block and
 * transfers nested-loop ownership into it.
 */

/*
 * Temporary construction stack. It is private because it is not part of the
 * persistent IR representation.
 */
typedef struct
{
    bfc_ir_block_t** blocks;

    size_t length;
    size_t capacity;
} bfc_ir_stack_t;

/**
 * @brief Constructs an immediate-bearing IR instruction.
 * @param ir_token_type IR operation kind.
 * @param imm Signed immediate operand.
 * @return Initialized instruction.
 */
bfc_ir_instr_t bfc_ir_make_imm_instr(bfc_ir_token_type_t const ir_token_type, int64_t const imm)
{
    return (bfc_ir_instr_t) {
        .op  = ir_token_type,
        .val = {imm},
    };
}

/**
 * @brief Constructs an IR instruction with a zero-initialized operand union.
 * @param ir_token_type IR operation kind.
 * @return Initialized instruction.
 */
bfc_ir_instr_t bfc_ir_make_zero_instr(bfc_ir_token_type_t const ir_token_type)
{
    return (bfc_ir_instr_t) {
        .op = ir_token_type,
    };
}

/**
 * @brief Builds a recursively nested IR tree from a validated token stream.
 *
 * A child block is allocated for each loop and owned by its `IR_LOOP`
 * instruction.
 *
 * @param root_block Destination owning root block.
 * @param tok_stream Validated token stream.
 * @return `BFC_ERR_OK` on success or an allocation error.
 */
bfc_error_t bfc_ir_create(bfc_ir_block_t** root_block, bfc_token_stream_t const* const tok_stream)
{
    bfc_error_t err = BFC_ERR_ALLOC;
    *root_block     = nullptr;

    bfc_ir_stack_t stack = (bfc_ir_stack_t) {
        .capacity = BFC_INITIAL_IR_STACK_CAPACITY,
        .length   = 0,
    };

    stack.blocks = BFC_CALLOC_ARRAY(stack.blocks, stack.capacity);
    if (!stack.blocks)
    {
        goto end;
    }

    stack.blocks[stack.length] = BFC_CALLOC_ARRAY(stack.blocks[stack.length], 1);
    if (!stack.blocks[stack.length])
    {
        goto end;
    }

    bfc_ir_block_t* current_block = stack.blocks[stack.length++];
    current_block->instructions   = nullptr;
    current_block->capacity       = BFC_INITIAL_IR_CAPACITY;
    current_block->length         = 0;

    current_block->instructions
        = BFC_MALLOC_ARRAY(current_block->instructions, current_block->capacity);
    if (!current_block->instructions)
    {
        goto end;
    }

    size_t i = 0;
    while (i < tok_stream->length)
    {
        /* Grow the construction stack before another nested loop is pushed. */
        if (stack.length >= stack.capacity)
        {
            size_t new_capacity;

            if (ckd_mul(&new_capacity, stack.capacity, 2))
            {
                goto end;
            }

            bfc_ir_block_t** tmp = BFC_REALLOC_ARRAY(stack.blocks, new_capacity);

            if (!tmp)
            {
                goto end;
            }

            stack.blocks   = tmp;
            stack.capacity = new_capacity;
        }

        /* Grow the active block before appending its next instruction. */
        if (current_block->length >= current_block->capacity)
        {
            size_t new_capacity;

            if (ckd_mul(&new_capacity, current_block->capacity, 2))
            {
                goto end;
            }

            bfc_ir_instr_t* tmp = BFC_REALLOC_ARRAY(current_block->instructions, new_capacity);

            if (!tmp)
            {
                goto end;
            }

            current_block->instructions = tmp;
            current_block->capacity     = new_capacity;
        }

        switch (tok_stream->tokens[i].type)
        {
            case TT_INC: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_imm_instr(IR_ADD, 1);
            }
            break;

            case TT_DEC: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_imm_instr(IR_ADD, -1);
            }
            break;

            case TT_PTR_LEFT: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_imm_instr(IR_MOVE, -1);
            }
            break;

            case TT_PTR_RIGHT: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_imm_instr(IR_MOVE, 1);
            }
            break;

            case TT_INPUT: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_zero_instr(IR_GET);
            }
            break;

            case TT_OUTPUT: {
                current_block->instructions[current_block->length++]
                    = bfc_ir_make_zero_instr(IR_PUT);
            }
            break;

            case TT_LOOP_START: {
                /*
                 * Store the child block in the current instruction before
                 * switching current_block to that child.
                 */
                bfc_ir_block_t* loop_body = nullptr;
                loop_body                 = BFC_CALLOC_ARRAY(loop_body, 1);

                bfc_ir_instr_t loop_instr = (bfc_ir_instr_t) {
                    .op  = IR_LOOP,
                    .val = {.body = loop_body},
                };

                if (!loop_instr.val.body)
                {
                    goto end;
                }

                current_block->instructions[current_block->length++] = loop_instr;

                stack.blocks[stack.length] = loop_instr.val.body;

                current_block           = stack.blocks[stack.length++];
                current_block->capacity = BFC_INITIAL_IR_CAPACITY;
                current_block->length   = 0;

                current_block->instructions
                    = BFC_MALLOC_ARRAY(current_block->instructions, current_block->capacity);

                if (!current_block->instructions)
                {
                    goto end;
                }
            }
            break;

            case TT_LOOP_END: {
                --stack.length;
                current_block = stack.blocks[stack.length - 1];
            }
            break;
        }

        ++i;
    }

    err = BFC_ERR_OK;

end:
    /*
     * On success, transfer the root block to the caller. On failure, recursively
     * destroy every block reachable from the root.
     */
    if (stack.blocks)
    {
        if (err.code == ERR_OK)
        {
            *root_block = stack.blocks[0];
        }
        else if (stack.blocks[0])
        {
            bfc_ir_destroy(&stack.blocks[0]);
        }

        free(stack.blocks);
    }

    return err;
}

/**
 * @brief Replaces an IR block with an optimized equivalent.
 *
 * Adjacent ADD or MOVE instructions are folded into one signed immediate.
 * Nested loops are optimized recursively, and [+]/[-] clear loops become
 * IR_SET 0 under the compiler's 8-bit wrapping-cell model.
 *
 * @param ir_block Pointer to the block being replaced.
 * @return `BFC_ERR_OK` on success or an allocation error.
 */
bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t** ir_block)
{
    if ((*ir_block)->length == 0)
    {
        return BFC_ERR_OK;
    }

    bfc_error_t err = BFC_ERR_ALLOC;

    bfc_ir_block_t* optimized_block = nullptr;
    optimized_block                 = BFC_CALLOC_ARRAY(optimized_block, 1);

    if (!optimized_block)
    {
        goto end;
    }

    optimized_block->capacity = (*ir_block)->capacity;

    optimized_block->instructions
        = BFC_MALLOC_ARRAY(optimized_block->instructions, optimized_block->capacity);

    if (!optimized_block->instructions)
    {
        goto end;
    }

    bfc_ir_instr_t prev_instr  = (*ir_block)->instructions[0];
    int64_t        instr_delta = 0;
    size_t         i           = 0;
    while (i < (*ir_block)->length)
    {
        if ((*ir_block)->instructions[i].op == IR_ADD || (*ir_block)->instructions[i].op == IR_MOVE)
        {
            do
            {
                instr_delta += (*ir_block)->instructions[i].val.imm;
                prev_instr = (*ir_block)->instructions[i++];
            }
            while (i < (*ir_block)->length && (*ir_block)->instructions[i].op == prev_instr.op);

            if (instr_delta != 0)
            {
                optimized_block->instructions[optimized_block->length++]
                    = bfc_ir_make_imm_instr(prev_instr.op, instr_delta);
            }

            instr_delta = 0;
        }
        else
        {
            if ((*ir_block)->instructions[i].op == IR_LOOP)
            {
                bfc_ir_instr_t* loop = &(*ir_block)->instructions[i];

                err = bfc_ir_optimize_rep(&loop->val.body);

                if (err.code != ERR_OK)
                {
                    goto end;
                }

                const bfc_ir_block_t* body = loop->val.body;

                if (body->length == 1 && body->instructions[0].op == IR_ADD
                    && (body->instructions[0].val.imm == 1 || body->instructions[0].val.imm == -1))
                {
                    bfc_ir_destroy(&loop->val.body);

                    optimized_block->instructions[optimized_block->length++]
                        = bfc_ir_make_imm_instr(IR_SET, 0);

                    ++i;
                    continue;
                }
            }

            optimized_block->instructions[optimized_block->length++] = (*ir_block)->instructions[i];

            prev_instr = (*ir_block)->instructions[i++];
        }
    }

    /*
     * Nested loop pointers copied into optimized_block retain ownership.
     * Release only the old block container and its instruction array.
     */
    free((*ir_block)->instructions);
    free(*ir_block);

    *ir_block       = optimized_block;
    optimized_block = nullptr;

    err = BFC_ERR_OK;

end:
    if (optimized_block)
    {
        free(optimized_block->instructions);
        free(optimized_block);
    }

    return err;
}

/**
 * @brief Recursively releases an IR tree and clears the caller's pointer.
 *
 * Null input is accepted for cleanup-attribute compatibility.
 *
 * @param proot_block Pointer to the owning root-block pointer.
 */
void bfc_ir_destroy(bfc_ir_block_t** proot_block)
{
    if (!proot_block || !*proot_block)
    {
        return;
    }

    for (size_t i = 0; i < (*proot_block)->length; ++i)
    {
        if ((*proot_block)->instructions[i].op == IR_LOOP
            && (*proot_block)->instructions[i].val.body)
        {
            bfc_ir_destroy(&(*proot_block)->instructions[i].val.body);
        }
    }

    free((*proot_block)->instructions);
    free(*proot_block);

    *proot_block = nullptr;
}

/* ==========================================================================
 * bfc_codegen.c
 * ========================================================================== */
/**
 * @defgroup codegen_impl Generic code-generation implementation
 * @internal
 * @{
 * Generic assembly generation.
 *
 * This module owns backend selection, recursive IR traversal, assembly-buffer
 * management, formatted emission, and assembly-file output. Architecture- and
 * OS-specific instruction syntax belongs in backend modules.
 */

/**
 * @brief Selects the backend for an exact architecture and operating system.
 *
 * @param target Target identity to resolve.
 * @return Immutable backend descriptor, or null when unavailable.
 * @internal
 */
[[gnu::pure]]
static const bfc_backend_t* bfc_backend_select(bfc_target_t target)
{
    if (target.arch == BFC_ARCH_AARCH64 && target.os == BFC_OS_MACOS)
    {
        return &BFC_BACKEND_MACOS_AARCH64;
    }

    if (target.arch == BFC_ARCH_X86_64 && target.os == BFC_OS_MACOS)
    {
        return &BFC_BACKEND_MACOS_X86_64;
    }

    if (target.arch == BFC_ARCH_AARCH64 && target.os == BFC_OS_LINUX)
    {
        return &BFC_BACKEND_LINUX_AARCH64;
    }

    if (target.arch == BFC_ARCH_X86_64 && target.os == BFC_OS_LINUX)
    {
        return &BFC_BACKEND_LINUX_X86_64;
    }

    return nullptr;
}

/**
 * @brief Emits one generic loop and delegates tests to the selected backend.
 *
 * `label_id` guarantees unique labels across nested and sibling loops.
 *
 * @internal
 */
[[gnu::nonnull(1, 2)]]
static bfc_error_t bfc_codegen_emit_loop(bfc_asm_t* asm_prog, const bfc_ir_block_t* body)
{
    const size_t id = asm_prog->label_id++;

    char start_label[64];
    char end_label[64];
    char label_line[80];

    snprintf(start_label, sizeof(start_label), ".L_bfc_loop_%zu", id);

    snprintf(end_label, sizeof(end_label), ".L_bfc_loop_%zu_end", id);

    snprintf(label_line, sizeof(label_line), "%s:\n", start_label);

    bfc_error_t err = bfc_codegen_emit_text(asm_prog, label_line);

    if (err.code == ERR_OK)
    {
        err = asm_prog->backend->emit_loop_test_z(asm_prog, end_label);
    }

    if (err.code == ERR_OK)
    {
        err = bfc_codegen_emit_block(asm_prog, body);
    }

    if (err.code == ERR_OK)
    {
        err = asm_prog->backend->emit_loop_test_nz(asm_prog, start_label);
    }

    if (err.code == ERR_OK)
    {
        snprintf(label_line, sizeof(label_line), "%s:\n", end_label);

        err = bfc_codegen_emit_text(asm_prog, label_line);
    }

    return err;
}

/**
 * @brief Appends text to the dynamically growing assembly buffer.
 *
 * The buffer remains null-terminated and size arithmetic is checked before
 * geometric growth.
 *
 * @param asm_prog Assembly object receiving the text.
 * @param text Null-terminated assembly fragment.
 * @return `BFC_ERR_OK` or an allocation/size error.
 * @internal
 *
 * Invariants:
 *   - length excludes the trailing null byte
 *   - buffer[length] is always '\0'
 */
bfc_error_t bfc_codegen_emit_text(bfc_asm_t* asm_prog, const char* text)
{
    const size_t text_length = strlen(text);
    size_t       required;

    if (ckd_add(&required, asm_prog->length, text_length) || ckd_add(&required, required, 1))
    {
        return bfc_make_error(ERR_ALLOC, "Assembly buffer size overflow");
    }

    if (required > asm_prog->capacity)
    {
        size_t new_capacity = asm_prog->capacity;

        while (new_capacity < required)
        {
            size_t doubled_capacity;

            if (ckd_mul(&doubled_capacity, new_capacity, 2))
            {
                return bfc_make_error(ERR_ALLOC, "Assembly buffer capacity overflow");
            }

            new_capacity = doubled_capacity;
        }

        char* new_buffer = realloc(asm_prog->buffer, new_capacity);

        if (!new_buffer)
        {
            return BFC_ERR_ALLOC;
        }

        asm_prog->buffer   = new_buffer;
        asm_prog->capacity = new_capacity;
    }

    memcpy(asm_prog->buffer + asm_prog->length, text, text_length + 1);

    asm_prog->length += text_length;

    return BFC_ERR_OK;
}

/**
 * @brief Traverses an IR block and dispatches operations to the backend.
 *
 * `IR_LOOP` recurses through the generic loop emitter.
 *
 * @param asm_prog Assembly object and selected backend.
 * @param ir_block Optimized IR block to lower.
 * @return `BFC_ERR_OK` or the first emission error.
 * @internal
 */
bfc_error_t bfc_codegen_emit_block(bfc_asm_t* asm_prog, const bfc_ir_block_t* ir_block)
{
    for (size_t i = 0; i < ir_block->length; ++i)
    {
        const bfc_ir_instr_t* instr = &ir_block->instructions[i];
        bfc_error_t           err;

        switch (instr->op)
        {
            case IR_ADD: err = asm_prog->backend->emit_op_add(asm_prog, instr->val.imm); break;

            case IR_MOVE: err = asm_prog->backend->emit_op_move(asm_prog, instr->val.imm); break;

            case IR_GET: err = asm_prog->backend->emit_op_get(asm_prog); break;

            case IR_PUT: err = asm_prog->backend->emit_op_put(asm_prog); break;

            case IR_SET: err = asm_prog->backend->emit_op_set(asm_prog, instr->val.imm); break;

            case IR_LOOP: err = bfc_codegen_emit_loop(asm_prog, instr->val.body); break;

            default: return bfc_make_error(ERR_INTERNAL, "Unknown IR instruction");
        }

        if (err.code != ERR_OK)
        {
            return err;
        }
    }

    return BFC_ERR_OK;
}

/**
 * @brief Formats and appends one bounded assembly fragment.
 *
 * @param asm_prog Assembly object receiving the fragment.
 * @param format `printf`-style format string.
 * @param ... Values consumed by `format`.
 * @return `BFC_ERR_OK` or a formatting/emission error.
 * @internal
 */
[[gnu::nonnull(1, 2), gnu::format(printf, 2, 3)]]
bfc_error_t bfc_codegen_emitf(bfc_asm_t* asm_prog, const char* format, ...)
{
    char buffer[256];

    va_list args;
    va_start(args, format);

    const int length = vsnprintf(buffer, sizeof(buffer), format, args);

    va_end(args);

    if (length < 0 || (size_t) length >= sizeof(buffer))
    {
        return bfc_make_error(ERR_INTERNAL, "Formatted assembly text is too long");
    }

    return bfc_codegen_emit_text(asm_prog, buffer);
}

/**
 * @brief Generates target-specific assembly for an optimized IR tree.
 *
 * Ownership is transferred to `out_asm` only after every emission stage
 * succeeds. Any intermediate failure destroys the partially built object.
 *
 * @param out_asm Destination owning assembly pointer.
 * @param ir_block Optimized IR tree to lower.
 * @param target Exact architecture and operating-system target.
 * @return `BFC_ERR_OK` or a target/emission/allocation error.
 */
bfc_error_t bfc_codegen(bfc_asm_t** out_asm, const bfc_ir_block_t* ir_block, bfc_target_t target)
{
    *out_asm = nullptr;

    const bfc_backend_t* backend = bfc_backend_select(target);

    if (!backend)
    {
        return bfc_make_error(ERR_ARGS, "No backend is available for the requested target");
    }

    bfc_asm_t* asm_prog = nullptr;
    asm_prog            = BFC_CALLOC_ARRAY(asm_prog, 1);

    if (!asm_prog)
    {
        return BFC_ERR_ALLOC;
    }

    asm_prog->capacity = BFC_INITIAL_ASM_CAPACITY;
    asm_prog->backend  = backend;
    asm_prog->buffer   = malloc(asm_prog->capacity);

    if (!asm_prog->buffer)
    {
        free(asm_prog);
        return BFC_ERR_ALLOC;
    }

    asm_prog->buffer[0] = '\0';

    bfc_error_t err = backend->emit_header(asm_prog);

    if (err.code == ERR_OK)
    {
        err = backend->emit_data_section(asm_prog);
    }

    if (err.code == ERR_OK)
    {
        err = backend->emit_symbol(asm_prog);
    }

    if (err.code == ERR_OK)
    {
        err = bfc_codegen_emit_block(asm_prog, ir_block);
    }

    if (err.code == ERR_OK)
    {
        err = backend->emit_end(asm_prog);
    }

    if (err.code != ERR_OK)
    {
        bfc_asm_destroy(&asm_prog);
        return err;
    }

    *out_asm = asm_prog;
    return BFC_ERR_OK;
}

/**
 * @brief Releases generated assembly and clears the caller's pointer.
 * @param pasm_prog Pointer to the owning assembly pointer.
 */
void bfc_asm_destroy(bfc_asm_t** pasm_prog)
{
    if (!pasm_prog || !*pasm_prog)
    {
        return;
    }

    free((*pasm_prog)->buffer);
    free(*pasm_prog);

    *pasm_prog = nullptr;
}

/**
 * @brief Writes generated assembly to a file.
 *
 * Exactly `asm_prog->length` bytes are written; the internal null terminator
 * is not part of the output file.
 *
 * @param asm_prog Assembly object to write.
 * @param path Output path.
 * @return `BFC_ERR_OK` or an I/O error.
 */
bfc_error_t bfc_asm_write_file(const bfc_asm_t* asm_prog, const char* path)
{
    FILE* file = fopen(path, "wb");

    if (!file)
    {
        return bfc_make_error(ERR_IO, "Could not open assembly output file");
    }

    const size_t written = fwrite(asm_prog->buffer, 1, asm_prog->length, file);

    if (written != asm_prog->length)
    {
        fclose(file);

        return bfc_make_error(ERR_IO, "Could not write complete assembly output");
    }

    if (fclose(file) != 0)
    {
        return bfc_make_error(ERR_IO, "Could not close assembly output file");
    }

    return BFC_ERR_OK;
}

/* ==========================================================================
 * bfc_backend_macos_aarch64.c
 * ========================================================================== */
/**
 * @defgroup macos_aarch64_backend macOS AArch64 backend
 * @internal
 * @{
 * macOS AArch64 assembly backend.
 *
 * The backend emits Mach-O assembly using Apple's relocation syntax. X19 holds
 * the Brainfuck tape pointer across libc calls, while X16/W16 is used as a
 * scratch register for immediates and cell values.
 */

/**
 * @brief Materializes a 64-bit constant in scratch register `x16`.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
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

/** @brief Emits the initial Mach-O text-section directives. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
                  ".p2align 2\n"
    );
}

/** @brief Declares the zero-initialized tape in Mach-O BSS. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_data_section(bfc_asm_t* asm_prog)
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

/**
 * @brief Emits the macOS AArch64 `main` prologue and tape address.
 *
 * Preserves X19, the callee-saved register used as the tape pointer.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_symbol(bfc_asm_t* asm_prog)
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

/** @brief Restores macOS AArch64 state and returns zero. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    mov w0, #0\n"
                  "    ldr x19, [sp, #16]\n"
                  "    ldp x29, x30, [sp], #32\n"
                  "    ret\n"
    );
}

/**
 * @brief Lowers wrapping byte-cell addition.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
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

/**
 * @brief Moves the tape pointer using direct or materialized immediates.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
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

    bfc_error_t err = macos_aarch64_emit_load_u64(asm_prog, magnitude);

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

/**
 * @brief Calls `_getchar`, maps EOF to zero, and stores one byte.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    bl   _getchar\n"
                  "    cmn  w0, #1\n"
                  "    csel w0, wzr, w0, eq\n"
                  "    strb w0, [x19]\n"
    );
}

/** @brief Loads the current cell and calls `_putchar`. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    ldrb w0, [x19]\n"
                  "    bl   _putchar\n"
    );
}

/** @brief Stores a normalized byte value in the current cell. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_aarch64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
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

/** @brief Uses `cbz` to branch when the current cell is zero. @internal */
[[gnu::nonnull(1, 2)]]
static bfc_error_t macos_aarch64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    ldrb w16, [x19]\n"
        "    cbz  w16, %s\n",
        label
    );
}

/** @brief Uses `cbnz` to branch when the current cell is nonzero. @internal */
[[gnu::nonnull(1, 2)]]
static bfc_error_t macos_aarch64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    ldrb w16, [x19]\n"
        "    cbnz w16, %s\n",
        label
    );
}

/**
 * @brief Immutable macOS AArch64 backend descriptor.
 */
const bfc_backend_t BFC_BACKEND_MACOS_AARCH64 = {
    .target = {.arch = BFC_ARCH_AARCH64, .os = BFC_OS_MACOS},

    .emit_header       = macos_aarch64_emit_header,
    .emit_data_section = macos_aarch64_emit_data_section,
    .emit_symbol       = macos_aarch64_emit_symbol,
    .emit_end          = macos_aarch64_emit_end,

    .emit_op_add       = macos_aarch64_emit_op_add,
    .emit_op_move      = macos_aarch64_emit_op_move,
    .emit_op_get       = macos_aarch64_emit_op_get,
    .emit_op_put       = macos_aarch64_emit_op_put,
    .emit_op_set       = macos_aarch64_emit_op_set,
    .emit_loop_test_z  = macos_aarch64_emit_loop_test_z,
    .emit_loop_test_nz = macos_aarch64_emit_loop_test_nz,
};

/* ==========================================================================
 * bfc_backend_macos_x86_64.c
 * ========================================================================== */
/**
 * @defgroup macos_x86_64_backend macOS x86-64 backend
 * @internal
 * @{
 * macOS x86-64 assembly backend.
 *
 * The backend emits Mach-O assembly in Clang/GNU Intel syntax. RBX holds the
 * Brainfuck tape pointer across libc calls, while R11 is reserved as a scratch
 * register for large pointer-movement immediates.
 */

/**
 * @brief Materializes a full-width unsigned immediate in scratch register `r11`.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
{
    return bfc_codegen_emitf(asm_prog, "    movabs r11, 0x%016" PRIx64 "\n", value);
}

/** @brief Emits Intel syntax and Mach-O text-section directives. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".intel_syntax noprefix\n"
                  ".text\n"
                  ".p2align 4\n"
    );
}

/** @brief Declares the zero-initialized tape in Mach-O BSS. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_data_section(bfc_asm_t* asm_prog)
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

/**
 * @brief Emits the macOS x86-64 `main` prologue and tape address.
 *
 * Preserves RBX and aligns the stack before libc calls.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_symbol(bfc_asm_t* asm_prog)
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

/** @brief Restores macOS x86-64 state and returns zero. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_end(bfc_asm_t* asm_prog)
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

/**
 * @brief Lowers wrapping byte-cell addition.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    if (normalized == 0)
    {
        return BFC_ERR_OK;
    }

    return bfc_codegen_emitf(asm_prog, "    add byte ptr [rbx], %u\n", (unsigned) normalized);
}

/**
 * @brief Moves the tape pointer using direct or materialized immediates.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
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

    bfc_error_t err = macos_x86_64_emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    return bfc_codegen_emit_text(asm_prog, imm < 0 ? "    sub rbx, r11\n" : "    add rbx, r11\n");
}

/**
 * @brief Calls `_getchar`, maps EOF to zero, and stores one byte.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    call _getchar\n"
                  "    xor  edx, edx\n"
                  "    cmp  eax, -1\n"
                  "    cmove eax, edx\n"
                  "    mov  byte ptr [rbx], al\n"
    );
}

/** @brief Zero-extends the current cell and calls `_putchar`. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    movzx edi, byte ptr [rbx]\n"
                  "    call  _putchar\n"
    );
}

/** @brief Stores a normalized byte value in the current cell. @internal */
[[gnu::nonnull(1)]]
static bfc_error_t macos_x86_64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    return bfc_codegen_emitf(asm_prog, "    mov byte ptr [rbx], %u\n", (unsigned) normalized);
}

/** @brief Uses `je` to branch when the current cell is zero. @internal */
[[gnu::nonnull(1, 2)]]
static bfc_error_t macos_x86_64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    cmp byte ptr [rbx], 0\n"
        "    je  %s\n",
        label
    );
}

/** @brief Uses `jne` to branch when the current cell is nonzero. @internal */
[[gnu::nonnull(1, 2)]]
static bfc_error_t macos_x86_64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    cmp byte ptr [rbx], 0\n"
        "    jne %s\n",
        label
    );
}

/**
 * @brief Immutable macOS x86-64 backend descriptor.
 */
const bfc_backend_t BFC_BACKEND_MACOS_X86_64 = {
    .target = {
        .arch = BFC_ARCH_X86_64,
        .os   = BFC_OS_MACOS,
    },

    .emit_header       = macos_x86_64_emit_header,
    .emit_data_section = macos_x86_64_emit_data_section,
    .emit_symbol       = macos_x86_64_emit_symbol,
    .emit_end          = macos_x86_64_emit_end,

    .emit_op_add       = macos_x86_64_emit_op_add,
    .emit_op_move      = macos_x86_64_emit_op_move,
    .emit_op_get       = macos_x86_64_emit_op_get,
    .emit_op_put       = macos_x86_64_emit_op_put,
    .emit_op_set       = macos_x86_64_emit_op_set,
    .emit_loop_test_z  = macos_x86_64_emit_loop_test_z,
    .emit_loop_test_nz = macos_x86_64_emit_loop_test_nz,
};

/* ==========================================================================
 * bfc_backend_linux_aarch64.c
 * ========================================================================== */
/**
 * @file bfc_backend_linux_aarch64.c
 * @brief Linux AArch64 assembly backend embedded in the single-file build.
 *
 * @details
 * Implements ELF symbols, the AAPCS64 ABI, and GNU AArch64-syntax lowering
 * for all IR operations.
 */
/** @internal Materializes a 64-bit immediate in scratch register `x16`. */
static bfc_error_t linux_aarch64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
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

        if (part != 0)
        {
            err = bfc_codegen_emitf(
                asm_prog, "    movk x16, #%u, lsl #%u\n", (unsigned) part, (unsigned) shift
            );

            if (err.code != ERR_OK)
            {
                return err;
            }
        }
    }

    return BFC_ERR_OK;
}

/** @internal Emits ELF text-section directives. */
static bfc_error_t linux_aarch64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, ".text\n.p2align 2\n");
}

/** @internal Declares the zero-initialized Brainfuck tape in ELF BSS. */
static bfc_error_t linux_aarch64_emit_data_section(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emitf(
        asm_prog,
        ".section .bss\n"
        ".balign 16\n"
        ".bfc_tape:\n"
        "    .skip %zu\n",
        BFC_TAPE_SIZE
    );
}

/** @internal Emits `main`, its stack frame, and the tape address. */
static bfc_error_t linux_aarch64_emit_symbol(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
                  ".global main\n"
                  ".type main, %function\n"
                  ".p2align 2\n"
                  "main:\n"
                  "    stp x29, x30, [sp, #-32]!\n"
                  "    str x19, [sp, #16]\n"
                  "    mov x29, sp\n"
                  "\n"
                  "    adrp x19, .bfc_tape\n"
                  "    add  x19, x19, :lo12:.bfc_tape\n"
    );
}

/** @internal Emits the ABI epilogue and zero process status. */
static bfc_error_t linux_aarch64_emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    mov w0, #0\n"
                  "    ldr x19, [sp, #16]\n"
                  "    ldp x29, x30, [sp], #32\n"
                  "    ret\n"
                  "    .size main, .-main\n"
    );
}

/** @internal Lowers wrapping byte-cell addition. */
static bfc_error_t linux_aarch64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
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

/** @internal Moves the tape pointer using immediate or register lowering. */
static bfc_error_t linux_aarch64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
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

    bfc_error_t err = linux_aarch64_emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    return bfc_codegen_emit_text(
        asm_prog, imm < 0 ? "    sub x19, x19, x16\n" : "    add x19, x19, x16\n"
    );
}

/** @internal Calls `getchar`, maps EOF to zero, and stores one byte. */
static bfc_error_t linux_aarch64_emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    bl   getchar\n"
                  "    cmn  w0, #1\n"
                  "    csel w0, wzr, w0, eq\n"
                  "    strb w0, [x19]\n"
    );
}

/** @internal Loads the current cell and calls `putchar`. */
static bfc_error_t linux_aarch64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, "    ldrb w0, [x19]\n    bl   putchar\n");
}

/** @internal Stores a normalized byte value in the current cell. */
static bfc_error_t linux_aarch64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
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

/** @internal Uses `cbz` to branch when the current cell is zero. */
static bfc_error_t linux_aarch64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    ldrb w16, [x19]\n    cbz  w16, %s\n", label);
}

/** @internal Uses `cbnz` to branch when the current cell is nonzero. */
static bfc_error_t linux_aarch64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    ldrb w16, [x19]\n    cbnz w16, %s\n", label);
}

/** @brief Immutable Linux AArch64 backend descriptor. */
const bfc_backend_t BFC_BACKEND_LINUX_AARCH64 = {
    .target = {
        .arch = BFC_ARCH_AARCH64,
        .os   = BFC_OS_LINUX,
    },
    .emit_header       = linux_aarch64_emit_header,
    .emit_data_section = linux_aarch64_emit_data_section,
    .emit_symbol       = linux_aarch64_emit_symbol,
    .emit_end          = linux_aarch64_emit_end,
    .emit_op_add       = linux_aarch64_emit_op_add,
    .emit_op_move      = linux_aarch64_emit_op_move,
    .emit_op_get       = linux_aarch64_emit_op_get,
    .emit_op_put       = linux_aarch64_emit_op_put,
    .emit_op_set       = linux_aarch64_emit_op_set,
    .emit_loop_test_z  = linux_aarch64_emit_loop_test_z,
    .emit_loop_test_nz = linux_aarch64_emit_loop_test_nz,
};

/* ==========================================================================
 * bfc_backend_linux_x86_64.c
 * ========================================================================== */
/**
 * @file bfc_backend_linux_x86_64.c
 * @brief Linux x86-64 assembly backend embedded in the single-file build.
 *
 * @details
 * Implements ELF symbols, the System V AMD64 ABI, and AT&T-syntax lowering
 * for all IR operations.
 */
/** @internal Materializes a full-width immediate in scratch register `r11`. */
static bfc_error_t linux_x86_64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
{
    return bfc_codegen_emitf(asm_prog, "    movabsq $0x%016" PRIx64 ", %%r11\n", value);
}

/** @internal Emits ELF text-section directives. */
static bfc_error_t linux_x86_64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, ".text\n.p2align 4\n");
}

/** @internal Declares the zero-initialized Brainfuck tape in ELF BSS. */
static bfc_error_t linux_x86_64_emit_data_section(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emitf(
        asm_prog,
        ".section .bss\n"
        ".balign 16\n"
        ".bfc_tape:\n"
        "    .zero %zu\n",
        BFC_TAPE_SIZE
    );
}

/** @internal Emits `main`, its aligned stack frame, and the tape address. */
static bfc_error_t linux_x86_64_emit_symbol(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
                  ".globl main\n"
                  ".type main, @function\n"
                  ".p2align 4\n"
                  "main:\n"
                  "    pushq %rbp\n"
                  "    movq  %rsp, %rbp\n"
                  "    pushq %rbx\n"
                  "    subq  $8, %rsp\n"
                  "\n"
                  "    leaq  .bfc_tape(%rip), %rbx\n"
    );
}

/** @internal Emits the ABI epilogue and zero process status. */
static bfc_error_t linux_x86_64_emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    xorl  %eax, %eax\n"
                  "    addq  $8, %rsp\n"
                  "    popq  %rbx\n"
                  "    popq  %rbp\n"
                  "    ret\n"
                  "    .size main, .-main\n"
    );
}

/** @internal Lowers wrapping byte-cell addition. */
static bfc_error_t linux_x86_64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    if (normalized == 0)
    {
        return BFC_ERR_OK;
    }

    return bfc_codegen_emitf(asm_prog, "    addb $%u, (%%rbx)\n", (unsigned) normalized);
}

/** @internal Moves the tape pointer using immediate or register lowering. */
static bfc_error_t linux_x86_64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
{
    if (imm == 0)
    {
        return BFC_ERR_OK;
    }

    const uint64_t magnitude = imm < 0 ? UINT64_C(0) - (uint64_t) imm : (uint64_t) imm;

    if (magnitude <= INT32_MAX)
    {
        return bfc_codegen_emitf(
            asm_prog, imm < 0 ? "    subq $%" PRIu64 ", %%rbx\n" : "    addq $%" PRIu64 ", %%rbx\n",
            magnitude
        );
    }

    bfc_error_t err = linux_x86_64_emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    return bfc_codegen_emit_text(
        asm_prog, imm < 0 ? "    subq %r11, %rbx\n" : "    addq %r11, %rbx\n"
    );
}

/** @internal Calls `getchar`, maps EOF to zero, and stores one byte. */
static bfc_error_t linux_x86_64_emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    call getchar@PLT\n"
                  "    xorl %edx, %edx\n"
                  "    cmpl $-1, %eax\n"
                  "    cmove %edx, %eax\n"
                  "    movb %al, (%rbx)\n"
    );
}

/** @internal Zero-extends the current cell and calls `putchar` through PLT. */
static bfc_error_t linux_x86_64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(asm_prog, "    movzbl (%rbx), %edi\n    call putchar@PLT\n");
}

/** @internal Stores a normalized byte value in the current cell. */
static bfc_error_t linux_x86_64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    return bfc_codegen_emitf(asm_prog, "    movb $%u, (%%rbx)\n", (unsigned) normalized);
}

/** @internal Uses `je` to branch when the current cell is zero. */
static bfc_error_t linux_x86_64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    cmpb $0, (%%rbx)\n    je %s\n", label);
}

/** @internal Uses `jne` to branch when the current cell is nonzero. */
static bfc_error_t linux_x86_64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(asm_prog, "    cmpb $0, (%%rbx)\n    jne %s\n", label);
}

/** @brief Immutable Linux x86-64 backend descriptor. */
const bfc_backend_t BFC_BACKEND_LINUX_X86_64 = {
    .target = {
        .arch = BFC_ARCH_X86_64,
        .os   = BFC_OS_LINUX,
    },
    .emit_header       = linux_x86_64_emit_header,
    .emit_data_section = linux_x86_64_emit_data_section,
    .emit_symbol       = linux_x86_64_emit_symbol,
    .emit_end          = linux_x86_64_emit_end,
    .emit_op_add       = linux_x86_64_emit_op_add,
    .emit_op_move      = linux_x86_64_emit_op_move,
    .emit_op_get       = linux_x86_64_emit_op_get,
    .emit_op_put       = linux_x86_64_emit_op_put,
    .emit_op_set       = linux_x86_64_emit_op_set,
    .emit_loop_test_z  = linux_x86_64_emit_loop_test_z,
    .emit_loop_test_nz = linux_x86_64_emit_loop_test_nz,
};
/* ==========================================================================
 * bfc.c
 * ========================================================================== */
/**
 * @defgroup driver_impl Compiler driver implementation
 * @internal
 * @{
 * bfc compiler driver.
 *
 * main() coordinates the complete compilation pipeline and uses cleanup
 * attributes to release successfully created objects on every exit path.
 */

/*
 * Propagate stage failures through one cleanup path. The cleanup attributes
 * release every object that was successfully created before the error.
 */
#define CHECK_ERROR(error_)                 \
    do                                      \
    {                                       \
        if ((error_).code != ERR_OK)        \
        {                                   \
            bfc_log_error(error_, program); \
            goto end;                       \
        }                                   \
    }                                       \
    while (0)

/**
 * @brief Runs the complete bfc compilation pipeline.
 *
 * Parses command-line arguments, loads and validates Brainfuck input, builds
 * and optimizes IR, selects the requested or host backend, and optionally
 * writes generated assembly when `-S` is supplied.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return `EXIT_SUCCESS` on successful compilation or `EXIT_FAILURE` after a
 * diagnostic has been logged.
 */
int main(int argc, char** argv)
{
    int ret = EXIT_FAILURE;

    bfc_args_t cmd_args = {0};

    /*
     * Each pointer starts null, making every registered cleanup function safe
     * even when the corresponding creation stage is never reached.
     */
    [[gnu::cleanup(bfc_program_destroy)]]
    bfc_program_t* program = nullptr;

    [[gnu::cleanup(bfc_token_stream_destroy)]]
    bfc_token_stream_t* tok_stream = nullptr;

    [[gnu::cleanup(bfc_jump_table_destroy)]]
    int64_t* jump_table = nullptr;

    [[gnu::cleanup(bfc_ir_destroy)]]
    bfc_ir_block_t* root_block = nullptr;

    [[gnu::cleanup(bfc_asm_destroy)]]
    bfc_asm_t* asm_prog = nullptr;

    bfc_error_t err;

    /* Front end: options, source loading, lexing, validation, and IR. */
    err = bfc_process_args(&cmd_args, argc, argv);
    CHECK_ERROR(err);

    if (cmd_args.ask_help)
    {
        bfc_cmd_help();
        ret = EXIT_SUCCESS;
        goto end;
    }

    err = bfc_program_create(&program, cmd_args.input);
    CHECK_ERROR(err);

    err = bfc_lex(&tok_stream, program, cmd_args);
    CHECK_ERROR(err);

    err = bfc_parse_jump_table(&jump_table, tok_stream);
    CHECK_ERROR(err);

    err = bfc_ir_create(&root_block, tok_stream);
    CHECK_ERROR(err);

    err = bfc_ir_optimize_rep(&root_block);
    CHECK_ERROR(err);

    /* Explicit --target overrides host detection and enables cross-codegen. */
    bfc_target_t target;

    if (cmd_args.target)
    {
        err = bfc_target_parse(&target, cmd_args.target);

        CHECK_ERROR(err);
    }
    else
    {
        target = bfc_target_host();
    }

    /* Back end: lower optimized IR into target-specific assembly text. */
    err = bfc_codegen(&asm_prog, root_block, target);

    CHECK_ERROR(err);

    if (cmd_args.do_assemble)
    {
        char output_path[4096];

        const int length = snprintf(
            output_path, sizeof(output_path), cmd_args.output ? "%s" : "%s.s",
            cmd_args.output ? cmd_args.output : cmd_args.input
        );

        if (length < 0 || (size_t) length >= sizeof(output_path))
        {
            err = bfc_make_error(ERR_ARGS, "Output path is too long");
            CHECK_ERROR(err);
        }

        err = bfc_asm_write_file(asm_prog, output_path);
        CHECK_ERROR(err);
    }

    ret = EXIT_SUCCESS;

end:
    return ret;
}
