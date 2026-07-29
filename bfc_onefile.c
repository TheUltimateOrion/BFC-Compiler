/*
 * bfc_onefile.c
 *
 * Single-translation-unit build of the bfc Brainfuck compiler.
 *
 * Compile example:
 *   clang -std=c23 -Wall -Wextra -Wpedantic bfc_onefile.c -lm -o bfc
 */

#define BFC_INTERNAL 1

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tokens */

#define TOKEN_MAP         \
    X(TT_INC, '+')        \
    X(TT_DEC, '-')        \
    X(TT_PTR_RIGHT, '>')  \
    X(TT_PTR_LEFT, '<')   \
    X(TT_LOOP_START, '[') \
    X(TT_LOOP_END, ']')   \
    X(TT_OUTPUT, '.')     \
    X(TT_INPUT, ',')

typedef enum
{
#define X(token_type, ...) token_type,
    TOKEN_MAP
#undef X
} bfc_token_type_t;

typedef struct
{
    bfc_token_type_t type;
    uint32_t         line;
    uint32_t         col;
} bfc_token_t;

typedef struct
{
    bfc_token_t* tokens;
    size_t       length;
} bfc_token_stream_t;

[[nodiscard, gnu::const]]
bfc_token_t bfc_make_token(bfc_token_type_t token_type, uint32_t line, uint32_t col);

/* Errors */

#define COL_OFF "\033[m"
#define COL_INFO "\033[1;1m"
#define COL_ERROR "\033[1;31m"

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

typedef struct [[nodiscard("bfc_error_t result must be checked")]]
{
    bfc_err_code_t code;
    char           msg[512];
    bfc_token_t    token;
} bfc_error_t;

#define BFC_ERR_OK ((bfc_error_t) {.code = ERR_OK, .msg = {0}, .token = {0}})

#define BFC_ERR_ALLOC                                                                      \
    ((bfc_error_t) {.code = ERR_ALLOC, .msg = "Memory allocation failure!", .token = {0}})

struct bfc_program_t;

bfc_error_t bfc_make_error(bfc_err_code_t error_code, const char* msg);

bfc_error_t
bfc_make_error_with_token(bfc_err_code_t error_code, const char* msg, bfc_token_t token);

[[nodiscard, gnu::const, gnu::returns_nonnull]]
const char* bfc_get_error_code(bfc_err_code_t error_code);

[[gnu::cold]]
void bfc_log_error(bfc_error_t err, const struct bfc_program_t* program);

/* Target */

typedef enum
{
    BFC_ARCH_X86_64,
    BFC_ARCH_I386,
    BFC_ARCH_AARCH64,
    BFC_ARCH_ARM32,
} bfc_arch_t;

typedef enum
{
    BFC_OS_WINDOWS,
    BFC_OS_MACOS,
    BFC_OS_LINUX,
} bfc_os_t;

typedef struct
{
    bfc_arch_t arch;
    bfc_os_t   os;
} bfc_target_t;

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_target_parse(bfc_target_t* target, const char* triple);

[[gnu::const]]
bfc_target_t bfc_target_host(void);

/* CLI */

typedef struct
{
    bool do_assemble;
    bool ask_help;
    bool f_no_comments;

    const char* input;
    const char* output;
    const char* target;
} bfc_args_t;

void bfc_cmd_help(void);

[[gnu::nonnull(1)]]
bfc_error_t bfc_process_args(bfc_args_t* cmd_args, int argc, char* const argv[]);

/* Input */

typedef struct bfc_program_t
{
    char*  path;
    char*  buffer;
    size_t file_size;
    size_t line_count;
} bfc_program_t;

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_program_create(bfc_program_t** program, const char* file_path);

void bfc_program_destroy(bfc_program_t** program);

[[nodiscard, gnu::pure, gnu::nonnull(1), gnu::returns_nonnull]]
const char* bfc_program_getname(const bfc_program_t* program);

[[nodiscard, gnu::malloc, gnu::nonnull(1)]]
char* bfc_program_getline(const bfc_program_t* program, size_t line);

/* Lexer */

[[gnu::nonnull(1, 2)]]
bfc_error_t
bfc_lex(bfc_token_stream_t** token_stream, const bfc_program_t* program, bfc_args_t cmd_args);

void bfc_token_stream_destroy(bfc_token_stream_t** token_stream);

/* Jump table */

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_parse_jump_table(int64_t** jump_table, const bfc_token_stream_t* token_stream);

void bfc_jump_table_destroy(int64_t** jump_table);

/* IR */

typedef enum
{
    IR_ADD,
    IR_MOVE,
    IR_PUT,
    IR_GET,
    IR_SET,
    IR_LOOP,
} bfc_ir_token_type_t;

typedef struct bfc_ir_block bfc_ir_block_t;

typedef struct
{
    bfc_ir_token_type_t op;

    union
    {
        int64_t         imm;
        bfc_ir_block_t* body;
    } val;
} bfc_ir_instr_t;

struct bfc_ir_block
{
    bfc_ir_instr_t* instr;
    size_t          length;
    size_t          capacity;
};

typedef struct
{
    bfc_ir_block_t** blocks;
    size_t           length;
    size_t           capacity;
} bfc_ir_stack_t;

[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_imm_instr(bfc_ir_token_type_t token_type, int64_t imm);

[[nodiscard, gnu::const]]
bfc_ir_instr_t bfc_ir_make_zero_instr(bfc_ir_token_type_t token_type);

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_ir_create(bfc_ir_block_t** root_block, const bfc_token_stream_t* token_stream);

[[gnu::nonnull(1)]]
bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t** ir_block);

void bfc_ir_destroy(bfc_ir_block_t** root_block);

/* Code generation */

typedef struct bfc_asm bfc_asm_t;

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen(bfc_asm_t** out_asm, const bfc_ir_block_t* ir_block, bfc_target_t target);

void bfc_asm_destroy(bfc_asm_t** asm_prog);

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_asm_write_file(const bfc_asm_t* asm_prog, const char* path);

/* Internal code-generation API */

#ifdef BFC_INTERNAL

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

typedef struct
{
    const char*  name;
    bfc_target_t target;
} bfc_target_entry_t;

struct bfc_asm
{
    const bfc_backend_t* backend;
    size_t               label_id;

    char*  buffer;
    size_t length;
    size_t capacity;
};

extern const bfc_backend_t BFC_BACKEND_WINDOWS_X86_64;
extern const bfc_backend_t BFC_BACKEND_WINDOWS_I386;
extern const bfc_backend_t BFC_BACKEND_WINDOWS_AARCH64;

extern const bfc_backend_t BFC_BACKEND_MACOS_AARCH64;
extern const bfc_backend_t BFC_BACKEND_MACOS_X86_64;

extern const bfc_backend_t BFC_BACKEND_LINUX_AARCH64;
extern const bfc_backend_t BFC_BACKEND_LINUX_X86_64;

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen_emit_text(bfc_asm_t* asm_prog, const char* text);

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_codegen_emit_block(bfc_asm_t* asm_prog, const bfc_ir_block_t* ir_block);

[[gnu::nonnull(1, 2), gnu::format(printf, 2, 3)]]
bfc_error_t bfc_codegen_emitf(bfc_asm_t* asm_prog, const char* format, ...);

#endif  // BFC_INTERNAL

/* =========================================================================
 * Error handling
 * ========================================================================= */

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

void bfc_log_error(bfc_error_t const err, const struct bfc_program_t* const program)
{
    if (err.code == ERR_MISSING_BRACKET || err.code == ERR_MISMATCHED_BRACKET)
    {
        fprintf(
            stderr, COL_INFO "%s[%u, %u]: " COL_ERROR "%s" COL_OFF COL_INFO ": %s\n" COL_OFF,
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

/* =========================================================================
 * Input and file I/O
 * ========================================================================= */

bfc_error_t bfc_program_create(bfc_program_t** program, char const* file_path)
{
    FILE* file_handle;

    if ((file_handle = fopen(file_path, "rb")))
    {
        bfc_program_t* prog = calloc(1, sizeof(*prog));
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

            char err_str[512];
            snprintf(err_str, sizeof(err_str), "Unable to read from file '%s'!", file_path);

            return bfc_make_error(ERR_IO, err_str);
        }

        prog->buffer[end] = '\0';

        fclose(file_handle);

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

    char err_str[512];
    snprintf(err_str, sizeof(err_str), "No such file or directory: '%s'", file_path);
    return bfc_make_error(ERR_IO, err_str);
}

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

/* =========================================================================
 * Command-line interface
 * ========================================================================= */

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
static bfc_error_t bfc_set_help(bfc_args_t* args, const char* value)
{
    (void) value;

    args->ask_help = true;
    return BFC_ERR_OK;
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_set_assemble(bfc_args_t* args, const char* value)
{
    (void) value;

    args->do_assemble = true;
    return BFC_ERR_OK;
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_set_no_comments(bfc_args_t* args, const char* value)
{
    (void) value;

    args->f_no_comments = true;
    return BFC_ERR_OK;
}

[[gnu::nonnull(1, 2)]]
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
static bfc_error_t bfc_set_target(bfc_args_t* args, const char* value)
{
    if (args->target)
    {
        return bfc_make_error(ERR_ARGS, "Target specified more than once");
    }

    args->target = value;
    return BFC_ERR_OK;
}

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

#define BFC_ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

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
                char error_message[512];

                snprintf(error_message, sizeof(error_message), "Unknown argument: '%s'", argument);

                return bfc_make_error(ERR_ARGS, error_message);
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

/* =========================================================================
 * Lexer and tokens
 * ========================================================================= */

bfc_token_t bfc_make_token(bfc_token_type_t const tok_type, uint32_t const line, uint32_t const col)
{
    return (bfc_token_t) {.type = tok_type, .line = line, .col = col};
}

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

bfc_error_t bfc_lex(
    bfc_token_stream_t**       token_stream,
    bfc_program_t const* const program,
    bfc_args_t const           cmd_args
)
{
    bfc_error_t err = BFC_ERR_ALLOC;

    *token_stream = nullptr;

    bfc_token_stream_t* tok_stream = calloc(1, sizeof(*tok_stream));
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

    tok_stream->tokens = malloc(program->file_size * sizeof(*tok_stream->tokens));
    if (!tok_stream->tokens)
    {
        goto end;
    }

    size_t token_list_size = 0;
    size_t buffer_index    = 0;

    uint32_t line = 1;
    uint32_t col  = 1;

    bool in_comment = false;

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

    if (token_list_size > 0)
    {
        bfc_token_t* tmp
            = realloc(tok_stream->tokens, token_list_size * sizeof(*tok_stream->tokens));

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
    if (tok_stream)
    {
        free(tok_stream->tokens);
        free(tok_stream);
    }

    return err;
}

/* =========================================================================
 * Jump table
 * ========================================================================= */

bfc_error_t bfc_parse_jump_table(int64_t** jump_table, bfc_token_stream_t const* const tok_stream)
{
    *jump_table = nullptr;

    bfc_error_t err;
    char        err_str[512];

    size_t n = tok_stream->length;
    if (n == 0)
    {
        return BFC_ERR_OK;
    }

    bfc_token_t const* toks = tok_stream->tokens;

    int64_t* jtable = malloc(n * sizeof(*jtable));
    if (!jtable)
    {
        return BFC_ERR_ALLOC;
    }

    for (size_t i = 0; i < n; ++i)
    {
        jtable[i] = -1;
    }

    size_t* stack = malloc(n * sizeof(*stack));
    if (!stack)
    {
        free(jtable);

        return BFC_ERR_ALLOC;
    }

    size_t sp = 0;

    size_t i;
    size_t j;
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
    snprintf(err_str, sizeof(err_str), "Found an extra ']' at line %u.", toks[i].line);

    err = bfc_make_error_with_token(ERR_MISMATCHED_BRACKET, err_str, toks[i]);

    free(stack);
    free(jtable);

    return err;

missing_closing_bracket:
    snprintf(
        err_str, sizeof(err_str),
        "Missing a closing bracket ']' for opening bracket '[' at line %u.",
        toks[stack[sp - 1]].line
    );

    err = bfc_make_error_with_token(ERR_MISSING_BRACKET, err_str, toks[stack[sp - 1]]);

    free(stack);
    free(jtable);

    return err;
}

void bfc_jump_table_destroy(int64_t** pjump_table)
{
    if (!pjump_table || !*pjump_table)
    {
        return;
    }

    free(*pjump_table);

    *pjump_table = nullptr;
}

/* =========================================================================
 * Intermediate representation
 * ========================================================================= */

bfc_ir_instr_t bfc_ir_make_imm_instr(bfc_ir_token_type_t const ir_token_type, int64_t const imm)
{
    return (bfc_ir_instr_t) {
        .op  = ir_token_type,
        .val = {imm},
    };
}

bfc_ir_instr_t bfc_ir_make_zero_instr(bfc_ir_token_type_t const ir_token_type)
{
    return (bfc_ir_instr_t) {
        .op = ir_token_type,
    };
}

bfc_error_t bfc_ir_create(bfc_ir_block_t** root_block, bfc_token_stream_t const* const tok_stream)
{
    bfc_error_t err = BFC_ERR_ALLOC;
    *root_block     = nullptr;

    bfc_ir_stack_t stack = (bfc_ir_stack_t) {
        .capacity = 5,
        .length   = 0,
    };

    stack.blocks = calloc(stack.capacity, sizeof(*stack.blocks));
    if (!stack.blocks)
    {
        goto end;
    }

    stack.blocks[stack.length] = calloc(1, sizeof(*stack.blocks[stack.length]));
    if (!stack.blocks[stack.length])
    {
        goto end;
    }

    bfc_ir_block_t* current_block = stack.blocks[stack.length++];
    current_block->instr          = nullptr;
    current_block->capacity       = 10;
    current_block->length         = 0;

    current_block->instr = malloc(current_block->capacity * sizeof(*current_block->instr));
    if (!current_block->instr)
    {
        goto end;
    }

    size_t i = 0;
    while (i < tok_stream->length)
    {
        if (stack.length >= stack.capacity)
        {
            size_t new_capacity = stack.capacity * 2;

            bfc_ir_block_t** tmp = realloc(stack.blocks, new_capacity * sizeof(*stack.blocks));

            if (!tmp)
            {
                goto end;
            }

            stack.blocks   = tmp;
            stack.capacity = new_capacity;
        }

        if (current_block->length >= current_block->capacity)
        {
            size_t new_capacity = current_block->capacity * 2;

            bfc_ir_instr_t* tmp
                = realloc(current_block->instr, new_capacity * sizeof(*current_block->instr));

            if (!tmp)
            {
                goto end;
            }

            current_block->instr    = tmp;
            current_block->capacity = new_capacity;
        }

        switch (tok_stream->tokens[i].type)
        {
            case TT_INC: {
                current_block->instr[current_block->length++] = bfc_ir_make_imm_instr(IR_ADD, 1);
            }
            break;

            case TT_DEC: {
                current_block->instr[current_block->length++] = bfc_ir_make_imm_instr(IR_ADD, -1);
            }
            break;

            case TT_PTR_LEFT: {
                current_block->instr[current_block->length++] = bfc_ir_make_imm_instr(IR_MOVE, -1);
            }
            break;

            case TT_PTR_RIGHT: {
                current_block->instr[current_block->length++] = bfc_ir_make_imm_instr(IR_MOVE, 1);
            }
            break;

            case TT_INPUT: {
                current_block->instr[current_block->length++] = bfc_ir_make_zero_instr(IR_GET);
            }
            break;

            case TT_OUTPUT: {
                current_block->instr[current_block->length++] = bfc_ir_make_zero_instr(IR_PUT);
            }
            break;

            case TT_LOOP_START: {
                bfc_ir_instr_t loop_instr = (bfc_ir_instr_t) {
                    .op  = IR_LOOP,
                    .val = {.body = calloc(1, sizeof(bfc_ir_block_t))},
                };

                if (!loop_instr.val.body)
                {
                    goto end;
                }

                current_block->instr[current_block->length++] = loop_instr;

                stack.blocks[stack.length] = loop_instr.val.body;

                current_block           = stack.blocks[stack.length++];
                current_block->capacity = 10;
                current_block->length   = 0;

                current_block->instr
                    = malloc(current_block->capacity * sizeof(*current_block->instr));

                if (!current_block->instr)
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

bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t** ir_block)
{
    if ((*ir_block)->length == 0)
    {
        return BFC_ERR_OK;
    }

    bfc_error_t err = BFC_ERR_ALLOC;

    bfc_ir_block_t* optimized_block = calloc(1, sizeof(*optimized_block));

    if (!optimized_block)
    {
        goto end;
    }

    optimized_block->capacity = (*ir_block)->capacity;

    optimized_block->instr = malloc(optimized_block->capacity * sizeof(*optimized_block->instr));

    if (!optimized_block->instr)
    {
        goto end;
    }

    bfc_ir_instr_t prev_instr  = (*ir_block)->instr[0];
    int64_t        instr_delta = 0;
    size_t         i           = 0;
    while (i < (*ir_block)->length)
    {
        if ((*ir_block)->instr[i].op == IR_ADD || (*ir_block)->instr[i].op == IR_MOVE)
        {
            do
            {
                instr_delta += (*ir_block)->instr[i].val.imm;
                prev_instr = (*ir_block)->instr[i++];
            }
            while (i < (*ir_block)->length && (*ir_block)->instr[i].op == prev_instr.op);

            if (instr_delta != 0)
            {
                optimized_block->instr[optimized_block->length++]
                    = bfc_ir_make_imm_instr(prev_instr.op, instr_delta);
            }

            instr_delta = 0;
        }
        else
        {
            if ((*ir_block)->instr[i].op == IR_LOOP)
            {
                err = bfc_ir_optimize_rep(&(*ir_block)->instr[i].val.body);

                if (err.code != ERR_OK)
                {
                    goto end;
                }
            }

            optimized_block->instr[optimized_block->length++] = (*ir_block)->instr[i];
            prev_instr                                        = (*ir_block)->instr[i++];
        }
    }

    free((*ir_block)->instr);
    free(*ir_block);

    *ir_block       = optimized_block;
    optimized_block = nullptr;

    err = BFC_ERR_OK;

end:
    if (optimized_block)
    {
        free(optimized_block->instr);
        free(optimized_block);
    }

    return err;
}

void bfc_ir_destroy(bfc_ir_block_t** proot_block)
{
    if (!proot_block || !*proot_block)
    {
        return;
    }

    for (size_t i = 0; i < (*proot_block)->length; ++i)
    {
        if ((*proot_block)->instr[i].op == IR_LOOP && (*proot_block)->instr[i].val.body)
        {
            bfc_ir_destroy(&(*proot_block)->instr[i].val.body);
        }
    }

    free((*proot_block)->instr);
    free(*proot_block);

    *proot_block = nullptr;
}

/* =========================================================================
 * Target detection
 * ========================================================================= */

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

/* =========================================================================
 * Generic code generation
 * ========================================================================= */

static constexpr bfc_target_entry_t BFC_TARGETS[] = {
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

bfc_error_t bfc_target_parse(bfc_target_t* target, const char* triple)
{
    for (size_t i = 0; i < sizeof(BFC_TARGETS) / sizeof(BFC_TARGETS[0]); ++i)
    {
        if (strcmp(triple, BFC_TARGETS[i].name) == 0)
        {
            *target = BFC_TARGETS[i].target;
            return BFC_ERR_OK;
        }
    }

    return bfc_make_error(ERR_ARGS, "Unknown or unsupported target triple");
}

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

    return nullptr;
}

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

bfc_error_t bfc_codegen_emit_text(bfc_asm_t* asm_prog, const char* text)
{
    const size_t text_length = strlen(text);
    const size_t required    = asm_prog->length + text_length + 1;

    if (required > asm_prog->capacity)
    {
        size_t new_capacity = asm_prog->capacity;

        while (new_capacity < required)
        {
            new_capacity *= 2;
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

bfc_error_t bfc_codegen_emit_block(bfc_asm_t* asm_prog, const bfc_ir_block_t* ir_block)
{
    for (size_t i = 0; i < ir_block->length; ++i)
    {
        const bfc_ir_instr_t* instr = &ir_block->instr[i];
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

bfc_error_t bfc_codegen(bfc_asm_t** out_asm, const bfc_ir_block_t* ir_block, bfc_target_t target)
{
    *out_asm = nullptr;

    const bfc_backend_t* backend = bfc_backend_select(target);

    if (!backend)
    {
        return bfc_make_error(ERR_ARGS, "No backend is available for the requested target");
    }

    bfc_asm_t* asm_prog = calloc(1, sizeof(*asm_prog));

    if (!asm_prog)
    {
        return BFC_ERR_ALLOC;
    }

    asm_prog->capacity = 4096;
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

/* =========================================================================
 * macOS x86-64 backend
 * ========================================================================= */

static const size_t bfc_macos_x86_64_BFC_TAPE_SIZE = 30000;

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_x86_64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
{
    return bfc_codegen_emitf(asm_prog, "    movabs r11, 0x%016" PRIx64 "\n", value);
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_x86_64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".intel_syntax noprefix\n"
                  ".text\n"
                  ".p2align 4\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_x86_64_emit_data_section(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emitf(
        asm_prog,
        ".section __DATA,__bss\n"
        ".p2align 4\n"
        "_bfc_tape:\n"
        "    .space %zu\n",
        bfc_macos_x86_64_BFC_TAPE_SIZE
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_x86_64_emit_symbol(bfc_asm_t* asm_prog)
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
static bfc_error_t bfc_macos_x86_64_emit_end(bfc_asm_t* asm_prog)
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
static bfc_error_t bfc_macos_x86_64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    if (normalized == 0)
    {
        return BFC_ERR_OK;
    }

    return bfc_codegen_emitf(asm_prog, "    add byte ptr [rbx], %u\n", (unsigned) normalized);
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_x86_64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
{
    if (imm == 0)
    {
        return BFC_ERR_OK;
    }

    const uint64_t magnitude = imm < 0 ? UINT64_C(0) - (uint64_t) imm : (uint64_t) imm;

    bfc_error_t err = bfc_macos_x86_64_emit_load_u64(asm_prog, magnitude);

    if (err.code != ERR_OK)
    {
        return err;
    }

    return bfc_codegen_emit_text(asm_prog, imm < 0 ? "    sub rbx, r11\n" : "    add rbx, r11\n");
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_x86_64_emit_op_get(bfc_asm_t* asm_prog)
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
static bfc_error_t bfc_macos_x86_64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    movzx edi, byte ptr [rbx]\n"
                  "    call  _putchar\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_x86_64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
{
    const uint8_t normalized = (uint8_t) imm;

    return bfc_codegen_emitf(asm_prog, "    mov byte ptr [rbx], %u\n", (unsigned) normalized);
}

[[gnu::nonnull(1, 2)]]
static bfc_error_t bfc_macos_x86_64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    cmp byte ptr [rbx], 0\n"
        "    je  %s\n",
        label
    );
}

[[gnu::nonnull(1, 2)]]
static bfc_error_t bfc_macos_x86_64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
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

    .emit_header       = bfc_macos_x86_64_emit_header,
    .emit_data_section = bfc_macos_x86_64_emit_data_section,
    .emit_symbol       = bfc_macos_x86_64_emit_symbol,
    .emit_end          = bfc_macos_x86_64_emit_end,

    .emit_op_add       = bfc_macos_x86_64_emit_op_add,
    .emit_op_move      = bfc_macos_x86_64_emit_op_move,
    .emit_op_get       = bfc_macos_x86_64_emit_op_get,
    .emit_op_put       = bfc_macos_x86_64_emit_op_put,
    .emit_op_set       = bfc_macos_x86_64_emit_op_set,
    .emit_loop_test_z  = bfc_macos_x86_64_emit_loop_test_z,
    .emit_loop_test_nz = bfc_macos_x86_64_emit_loop_test_nz,
};

/* =========================================================================
 * macOS AArch64 backend
 * ========================================================================= */

static const size_t bfc_macos_aarch64_BFC_TAPE_SIZE = 30000;

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_load_u64(bfc_asm_t* asm_prog, uint64_t value)
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

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_header(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, ".text\n"
                  ".p2align 2\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_data_section(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emitf(
        asm_prog,
        ".section __DATA,__bss\n"
        ".p2align 4\n"
        "_bfc_tape:\n"
        "    .space %zu\n",
        bfc_macos_aarch64_BFC_TAPE_SIZE
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_symbol(bfc_asm_t* asm_prog)
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

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_end(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "\n"
                  "    mov w0, #0\n"
                  "    ldr x19, [sp, #16]\n"
                  "    ldp x29, x30, [sp], #32\n"
                  "    ret\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_op_add(bfc_asm_t* asm_prog, int64_t imm)
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

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_op_move(bfc_asm_t* asm_prog, int64_t imm)
{
    if (imm == 0)
    {
        return BFC_ERR_OK;
    }

    const uint64_t magnitude = imm < 0 ? UINT64_C(0) - (uint64_t) imm : (uint64_t) imm;

    bfc_error_t err = bfc_macos_aarch64_emit_load_u64(asm_prog, magnitude);

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

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_op_get(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    bl   _getchar\n"
                  "    cmn  w0, #1\n"
                  "    csel w0, wzr, w0, eq\n"
                  "    strb w0, [x19]\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_op_put(bfc_asm_t* asm_prog)
{
    return bfc_codegen_emit_text(
        asm_prog, "    ldrb w0, [x19]\n"
                  "    bl   _putchar\n"
    );
}

[[gnu::nonnull(1)]]
static bfc_error_t bfc_macos_aarch64_emit_op_set(bfc_asm_t* asm_prog, int64_t imm)
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

[[gnu::nonnull(1, 2)]]
static bfc_error_t bfc_macos_aarch64_emit_loop_test_z(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    ldrb w16, [x19]\n"
        "    cbz  w16, %s\n",
        label
    );
}

[[gnu::nonnull(1, 2)]]
static bfc_error_t bfc_macos_aarch64_emit_loop_test_nz(bfc_asm_t* asm_prog, const char* label)
{
    return bfc_codegen_emitf(
        asm_prog,
        "    ldrb w16, [x19]\n"
        "    cbnz w16, %s\n",
        label
    );
}

const bfc_backend_t BFC_BACKEND_MACOS_AARCH64 = {
    .target = {.arch = BFC_ARCH_AARCH64, .os = BFC_OS_MACOS},

    .emit_header       = bfc_macos_aarch64_emit_header,
    .emit_data_section = bfc_macos_aarch64_emit_data_section,
    .emit_symbol       = bfc_macos_aarch64_emit_symbol,
    .emit_end          = bfc_macos_aarch64_emit_end,

    .emit_op_add       = bfc_macos_aarch64_emit_op_add,
    .emit_op_move      = bfc_macos_aarch64_emit_op_move,
    .emit_op_get       = bfc_macos_aarch64_emit_op_get,
    .emit_op_put       = bfc_macos_aarch64_emit_op_put,
    .emit_op_set       = bfc_macos_aarch64_emit_op_set,
    .emit_loop_test_z  = bfc_macos_aarch64_emit_loop_test_z,
    .emit_loop_test_nz = bfc_macos_aarch64_emit_loop_test_nz,
};

/* =========================================================================
 * Compiler driver
 * ========================================================================= */

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

int main(int argc, char** argv)
{
    int ret = EXIT_FAILURE;

    bfc_args_t cmd_args = {0};

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
