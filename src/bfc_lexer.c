/*
 * Brainfuck lexer.
 *
 * The lexer scans the complete source buffer once, records one-based source
 * locations, and emits tokens only for Brainfuck instructions. By default, a
 * semicolon starts a line comment unless --fno-comments is active.
 */

#include "bfc_lexer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "bfc_memory.h"

/*
 * Allocate a stream in *token_stream. The source size is a safe upper bound
 * for token count because each input byte can produce at most one token.
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
