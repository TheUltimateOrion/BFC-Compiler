#include "bfc_error.h"

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "bfc_io.h"

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
