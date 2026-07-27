#ifndef BFC_CLI_H
#define BFC_CLI_H

#include <stdbool.h>

#include "bfc_error.h"

typedef struct
{
    bool do_assemble;
    bool ask_help;
    bool f_no_comments;

    const char* input;
    const char* output;
} bfc_args_t;

void bfc_cmd_help(void);

[[gnu::nonnull(1)]]
bfc_error_t bfc_process_args(bfc_args_t* cmd_args, int argc, char* const argv[]);

#endif  // BFC_CLI_H
