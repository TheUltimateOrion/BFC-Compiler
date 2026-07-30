#ifndef BFC_ERROR_H
#define BFC_ERROR_H

#include "bfc_token.h"

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

[[gnu::nonnull(2), gnu::format(printf, 2, 3)]]
bfc_error_t bfc_make_errorf(bfc_err_code_t error_code, const char* format, ...);

[[gnu::nonnull(3), gnu::format(printf, 3, 4)]]
bfc_error_t
bfc_make_errorf_with_token(bfc_err_code_t error_code, bfc_token_t token, const char* format, ...);

bfc_error_t bfc_make_error(bfc_err_code_t const error_code, char const* msg);

bfc_error_t bfc_make_error_with_token(
    bfc_err_code_t const error_code,
    char const*          msg,
    bfc_token_t const    token
);

[[nodiscard, gnu::const, gnu::returns_nonnull]]
char const* bfc_get_error_code(bfc_err_code_t const error_code);

[[gnu::cold]]
void bfc_log_error(bfc_error_t err, const struct bfc_program_t* program);

#endif  // BFC_ERROR_H
