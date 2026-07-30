#ifndef BFC_TOKEN_H
#define BFC_TOKEN_H

#include <stddef.h>
#include <stdint.h>

/*
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

/* Token kinds emitted by the lexer. */
typedef enum
{
#define X(tok_type, ...) tok_type,
    TOKEN_MAP
#undef X
} bfc_token_type_t;

/*
 * One Brainfuck token and its one-based source location.
 * line and col are used when reporting source diagnostics.
 */
typedef struct
{
    bfc_token_type_t type;
    uint32_t         line;
    uint32_t         col;
} bfc_token_t;

/*
 * Owning token sequence produced by bfc_lex().
 * Release it with bfc_token_stream_destroy().
 */
typedef struct
{
    bfc_token_t* tokens;
    size_t       length;
} bfc_token_stream_t;

/* Construct a token value without allocating memory. */
[[nodiscard, gnu::const]]
bfc_token_t
bfc_make_token(bfc_token_type_t const tok_type, uint32_t const line, uint32_t const col);

/*
 * Release a token stream and set the caller's pointer to null.
 * Passing null or a pointer to null is permitted.
 */
void bfc_token_stream_destroy(bfc_token_stream_t** ptok_stream);

#endif  // BFC_TOKEN_H
