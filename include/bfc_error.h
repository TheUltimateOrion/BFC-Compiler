#ifndef BFC_ERROR_H
#define BFC_ERROR_H

#include "bfc_token.h"

/* ANSI escape sequences used by the command-line diagnostic formatter. */
#define COL_OFF "\033[m"
#define COL_INFO "\033[1;1m"
#define COL_ERROR "\033[1;31m"

/* Single source of truth for compiler error codes. */
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

/*
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

/* Frequently returned error constants. */
#define BFC_ERR_OK ((bfc_error_t) {.code = ERR_OK, .msg = {0}, .token = {0}})
#define BFC_ERR_ALLOC                                                                      \
    ((bfc_error_t) {.code = ERR_ALLOC, .msg = "Memory allocation failure!", .token = {0}})

/* Forward declaration avoids requiring the complete source-program structure. */
struct bfc_program_t;

/* Create a formatted error message without a source token. */
[[gnu::nonnull(2), gnu::format(printf, 2, 3)]]
bfc_error_t bfc_make_errorf(bfc_err_code_t error_code, const char* format, ...);

/* Create a formatted error message associated with a source token. */
[[gnu::nonnull(3), gnu::format(printf, 3, 4)]]
bfc_error_t
bfc_make_errorf_with_token(bfc_err_code_t error_code, bfc_token_t token, const char* format, ...);

/* Create an error by copying an existing message string. */
bfc_error_t bfc_make_error(bfc_err_code_t const error_code, char const* msg);

/* Create a token-associated error by copying an existing message string. */
bfc_error_t bfc_make_error_with_token(
    bfc_err_code_t const error_code,
    char const*          msg,
    bfc_token_t const    token
);

/* Return the symbolic name of an error code, such as "ERR_IO". */
[[nodiscard, gnu::const, gnu::returns_nonnull]]
char const* bfc_get_error_code(bfc_err_code_t const error_code);

/*
 * Print a user-facing diagnostic to stderr.
 * Source-related errors include the source line and a caret when available.
 */
[[gnu::cold]]
void bfc_log_error(bfc_error_t err, const struct bfc_program_t* program);

#endif  // BFC_ERROR_H
