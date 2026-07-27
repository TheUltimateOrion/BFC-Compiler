#ifndef __BFC_TOKEN_H
#define __BFC_TOKEN_H

#include <stddef.h>
#include <stdint.h>

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
#define X(tok_type, ...) tok_type,
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
bfc_token_t
bfc_make_token(bfc_token_type_t const tok_type, uint32_t const line, uint32_t const col);

#endif  // __BFC_TOKEN_H
