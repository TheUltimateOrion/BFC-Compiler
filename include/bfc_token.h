/**
 * @file bfc_token.h
 * @brief Brainfuck token definitions and token-stream ownership.
 *
 * @details
 * Defines token kinds, source locations, stream storage, construction, and destruction.
 */
#ifndef BFC_TOKEN_H
#define BFC_TOKEN_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Single source of truth for Brainfuck characters and token kinds.
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

/**
 * @brief Token kinds emitted by the lexer.
 */
typedef enum
{
#define X(tok_type, ...) tok_type,
    TOKEN_MAP
#undef X
} bfc_token_type_t;

/**
 * @brief One Brainfuck token and its one-based source location.
 */
typedef struct
{
    bfc_token_type_t type;
    uint32_t         line;
    uint32_t         col;
} bfc_token_t;

/**
 * @brief Owning token sequence produced by the lexer.
 */
typedef struct
{
    bfc_token_t* tokens;
    size_t       length;
} bfc_token_stream_t;

/**
 * @brief Constructs a token value without allocating memory.
 */
[[nodiscard, gnu::const]]
bfc_token_t
bfc_make_token(bfc_token_type_t const tok_type, uint32_t const line, uint32_t const col);

/**
 * @brief Releases a token stream and nulls the caller pointer.
 */
void bfc_token_stream_destroy(bfc_token_stream_t** ptok_stream);

#endif  // BFC_TOKEN_H
