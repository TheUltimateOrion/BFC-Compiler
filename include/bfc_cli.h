#ifndef BFC_CLI_H
#define BFC_CLI_H

#include <stdbool.h>

#include "bfc_error.h"

/*
 * Parsed command-line state.
 *
 * input, output, and target are borrowed pointers into argv and must not be
 * modified or freed. do_assemble means "emit assembly and stop" for -S.
 */
typedef struct
{
    bool do_assemble;
    bool ask_help;
    bool f_no_comments;

    const char* input;
    const char* output;
    const char* target;
} bfc_args_t;

/* Print command-line usage and the available options to stdout. */
void bfc_cmd_help(void);

/*
 * Parse argv into cmd_args.
 *
 * Exactly one input path is accepted unless help is requested. The special
 * argument "--" stops option parsing. No allocations are performed.
 */
[[gnu::nonnull(1)]]
bfc_error_t bfc_process_args(bfc_args_t* cmd_args, int argc, char* const argv[]);

#endif  // BFC_CLI_H
