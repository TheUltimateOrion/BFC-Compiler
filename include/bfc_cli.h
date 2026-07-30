/**
 * @file bfc_cli.h
 * @brief Command-line parsing interface.
 *
 * @details
 * Defines parsed compiler options and the command-line processor.
 */
#ifndef BFC_CLI_H
#define BFC_CLI_H

#include <stdbool.h>

#include "bfc_error.h"

/**
 * @brief Parsed command-line state.
 *
 * @note String members borrow storage from `argv` and must not be freed.
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

/**
 * @brief Prints command-line usage and option descriptions.
 */
void bfc_cmd_help(void);

/**
 * @brief Parses compiler command-line arguments.
 *
 * @param[out] cmd_args Receives zero-initialized parsed arguments.
 * @param[in] argc Argument count.
 * @param[in] argv Argument vector.
 *
 * @return `BFC_ERR_OK` on success; otherwise an argument error.
 */
[[gnu::nonnull(1)]]
bfc_error_t bfc_process_args(bfc_args_t* cmd_args, int argc, char* const argv[]);

#endif  // BFC_CLI_H
