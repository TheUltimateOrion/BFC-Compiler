#ifndef __BFC_CLI_H
#define __BFC_CLI_H

#include <stdint.h>

#include "bfc_error.h"

typedef struct {
	union {
		struct {
			uint8_t do_assemble        : 1;
			uint8_t ask_help           : 1;
			uint8_t f_no_comments : 1;
		};
		uint8_t flags;
	};
	char *input;
	char *outputs[UINT8_MAX];
} bfc_args_t;

void bfc_cmd_help(void);

bfc_error_t bfc_process_args(bfc_args_t *const cmd_args, int argc, char **argv);

#endif // __BFC_CLI_H
