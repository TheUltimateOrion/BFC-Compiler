/*
 * Bracket validation and jump-table construction.
 *
 * An explicit stack records unmatched '[' token indices. Each matched pair is
 * stored bidirectionally in the resulting table; non-bracket entries retain
 * the -1 sentinel.
 */

#include "bfc_jumptable.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bfc_memory.h"

/*
 * Build a table with one entry per token. Matched brackets contain each
 * other's indices; all other tokens retain -1.
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

/* Release the flat jump table and clear the caller's pointer. */
void bfc_jump_table_destroy(int64_t** pjump_table)
{
    if (!pjump_table || !*pjump_table)
    {
        return;
    }

    free(*pjump_table);

    *pjump_table = nullptr;
}
