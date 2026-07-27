#include "bfc_lexer.h"

#include <stdbool.h>
#include <stdlib.h>

bfc_token_t bfc_make_token(bfc_token_type_t const tok_type, uint32_t const line, uint32_t const col)
{ return (bfc_token_t) {.type = tok_type, .line = line, .col = col}; }

void bfc_token_stream_destroy(bfc_token_stream_t** ptok_stream)
{
    if (!ptok_stream || !*ptok_stream) { return; }

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
    bfc_error_t err                = BFC_ERR_ALLOC;

    *token_stream                  = nullptr;

    bfc_token_stream_t* tok_stream = calloc(1, sizeof(*tok_stream));
    if (!tok_stream) { goto end; }

    if (program->file_size == 0)
    {
        *token_stream = tok_stream;
        tok_stream    = nullptr;
        err           = BFC_ERR_OK;
        goto end;
    }

    tok_stream->tokens = malloc(program->file_size * sizeof(*tok_stream->tokens));
    if (!tok_stream->tokens) { goto end; }

    size_t   token_list_size = 0;
    size_t   buffer_index    = 0;

    uint32_t line            = 1;
    uint32_t col             = 1;

    bool     in_comment      = false;

#define EMIT_TOKEN(toktype)                                                                        \
    if (!in_comment) tok_stream->tokens[token_list_size++] = bfc_make_token((toktype), line, col);

    while (program->buffer[buffer_index] != '\0')
    {
        switch (program->buffer[buffer_index])
        {
            case ';': {
                if (cmd_args.f_no_comments) { break; }

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
        bfc_token_t* tmp = realloc(
            tok_stream->tokens, token_list_size * sizeof(*tok_stream->tokens)
        );

        if (tmp) { tok_stream->tokens = tmp; }
    }
    else
    {
        free(tok_stream->tokens);
        tok_stream->tokens = nullptr;
    }

    tok_stream->length = token_list_size;

    *token_stream      = tok_stream;

    tok_stream         = nullptr;

    err                = BFC_ERR_OK;

end:
    if (tok_stream)
    {
        free(tok_stream->tokens);
        free(tok_stream);
    }

    return err;
}
