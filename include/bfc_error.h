#ifndef __BFC_ERROR_H
#define __BFC_ERROR_H

#include "bfc_token.h"

#define COL_OFF     "\033[m"
#define COL_INFO    "\033[1;1m"
#define COL_ERROR   "\033[1;31m"

struct bfc_program_t;

typedef enum {
	ERR_OK = 0,
	ERR_ARGS,
	ERR_IO,
	ERR_MISMATCHED_BRACKET,
	ERR_MISSING_BRACKET,
	ERR_ALLOC,
	ERR_INTERNAL,
} bfc_err_code_t;

typedef struct {
	bfc_err_code_t code;
	char msg[512];
	bfc_token_t token;
} bfc_error_t;

#define BFC_ERR_OK ((bfc_error_t) {   \
	.code = ERR_OK,               \
	.msg = {0},                   \
	.token = {0}                  \
})

#define BFC_ERR_ALLOC ((bfc_error_t) {      \
	.code = ERR_ALLOC,                     \
	.msg = "Memory allocation failure!",   \
	.token = {0}                           \
})


bfc_error_t bfc_make_error(const bfc_err_code_t error_code, const char *msg);
bfc_error_t bfc_make_error_with_token(const bfc_err_code_t error_code, const char *msg, const bfc_token_t token);
const char *bfc_get_error_code(const bfc_err_code_t error_code);
void bfc_log_error(const bfc_error_t err, const struct bfc_program_t *const program);


#endif // __BFC_ERROR_H
