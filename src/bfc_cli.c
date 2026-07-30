/**
 * @file bfc_cli.c
 * @brief Table-driven command-line parser.
 *
 * @details
 * Defines option metadata, handlers, help rendering, positional-input rules, and the end-of-options
 * marker.
 */
#include "bfc_cli.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "bfc_common.h"

/**
 * @brief Option callback signature used by the table-driven parser.
 *
 * @internal
 */
typedef bfc_error_t (*bfc_option_handler_t)(bfc_args_t* args, const char* value);

/**
 * @brief Metadata and behaviour for one command-line option.
 *
 * @internal
 */
typedef struct
{
    const char*          short_name;
    const char*          long_name;
    const char*          value_name;
    const char*          description;
    bfc_option_handler_t handler;
} bfc_option_t;

/**
 * @brief Sets the help-request flag.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t bfc_set_help(bfc_args_t* args, const char* value)
{
    (void) value;

    args->ask_help = true;
    return BFC_ERR_OK;
}

/**
 * @brief Enables assembly-only output.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t bfc_set_assemble(bfc_args_t* args, const char* value)
{
    (void) value;

    args->do_assemble = true;
    return BFC_ERR_OK;
}

/**
 * @brief Disables the semicolon-comment extension.
 *
 * @internal
 */
[[gnu::nonnull(1)]]
static bfc_error_t bfc_set_no_comments(bfc_args_t* args, const char* value)
{
    (void) value;

    args->f_no_comments = true;
    return BFC_ERR_OK;
}

/**
 * @brief Stores the borrowed output-path argument after duplicate detection.
 *
 * @internal
 */
[[gnu::nonnull(1, 2)]]
static bfc_error_t bfc_set_output(bfc_args_t* args, const char* value)
{
    if (args->output)
    {
        return bfc_make_error(ERR_ARGS, "Output file specified more than once!");
    }

    args->output = value;
    return BFC_ERR_OK;
}

/**
 * @brief Stores the borrowed target-triple argument after duplicate detection.
 *
 * @internal
 */
[[gnu::nonnull(1, 2)]]
static bfc_error_t bfc_set_target(bfc_args_t* args, const char* value)
{
    if (args->target)
    {
        return bfc_make_error(ERR_ARGS, "Target specified more than once");
    }

    args->target = value;
    return BFC_ERR_OK;
}

/**
 * @brief Sets the version-print flag.
 *
 * @internal
 */
static bfc_error_t bfc_set_version(bfc_args_t* args, const char* value)
{
    (void) value;

    args->ask_version = true;
    return BFC_ERR_OK;
}

/**
 * @brief Single source of truth for option parsing and help rendering.
 *
 * @internal
 */
static const bfc_option_t BFC_OPTIONS[] = {
    {
     .short_name  = "-h",
     .long_name   = "--help",
     .value_name  = nullptr,
     .description = "Display available options",
     .handler     = bfc_set_help,
     },
    {
     .short_name  = "-S",
     .long_name   = "--assembly",
     .value_name  = nullptr,
     .description = "Only run compilation steps",
     .handler     = bfc_set_assemble,
     },
    {
     .short_name  = nullptr,
     .long_name   = "--fno-comments",
     .value_name  = nullptr,
     .description = "Do not treat lines starting with ';' as comments",
     .handler     = bfc_set_no_comments,
     },
    {
     .short_name  = "-o",
     .long_name   = "--output",
     .value_name  = "file",
     .description = "Write output to <file>",
     .handler     = bfc_set_output,
     },
    {
     .short_name  = "-t",
     .long_name   = "--target",
     .value_name  = "triple",
     .description = "Compile for the given target <triple>",
     .handler     = bfc_set_target,
     },
    {
     .short_name  = "-v",
     .long_name   = "--version",
     .value_name  = nullptr,
     .description = "Display compiler version",
     .handler     = bfc_set_version,
     },
};

/**
 * @brief Finds option metadata by exact short or long spelling.
 *
 * @internal
 */
[[gnu::pure, gnu::nonnull(1)]]
static const bfc_option_t* bfc_find_option(const char* argument)
{
    for (size_t i = 0; i < BFC_ARRAY_LENGTH(BFC_OPTIONS); ++i)
    {
        const bfc_option_t* option = &BFC_OPTIONS[i];

        if (option->short_name && strcmp(argument, option->short_name) == 0)
        {
            return option;
        }

        if (option->long_name && strcmp(argument, option->long_name) == 0)
        {
            return option;
        }
    }

    return nullptr;
}

/**
 * @brief Renders usage text from the same option table used for parsing.
 */

void bfc_cmd_help(void)
{
    printf("OVERVIEW: bfc Brainfuck compiler\n\n");
    printf("USAGE: bfc [options] <file.bf>\n\n");
    printf("OPTIONS:\n");

    for (size_t i = 0; i < BFC_ARRAY_LENGTH(BFC_OPTIONS); ++i)
    {
        const bfc_option_t* option = &BFC_OPTIONS[i];
        char                usage[64];

        if (option->short_name && option->long_name)
        {
            if (option->value_name)
            {
                snprintf(
                    usage, sizeof(usage), "%s <%s>, %s <%s>", option->short_name,
                    option->value_name, option->long_name, option->value_name
                );
            }
            else
            {
                snprintf(usage, sizeof(usage), "%s, %s", option->short_name, option->long_name);
            }
        }
        else
        {
            const char* name = option->short_name ? option->short_name : option->long_name;

            if (option->value_name)
            {
                snprintf(usage, sizeof(usage), "%s <%s>", name, option->value_name);
            }
            else
            {
                snprintf(usage, sizeof(usage), "%s", name);
            }
        }

        printf("  %-30s %s\n", usage, option->description);
    }
}

/**
 * @brief Parses options, option values, the end-of-options marker, and one input path.
 */

bfc_error_t bfc_process_args(bfc_args_t* cmd_args, int argc, char* const argv[])
{
    *cmd_args = (bfc_args_t) {0};

    bool parse_options = true;

    for (int i = 1; i < argc; ++i)
    {
        const char* argument = argv[i];

        if (parse_options && strcmp(argument, "--") == 0)
        {
            parse_options = false;
            continue;
        }

        if (parse_options && argument[0] == '-')
        {
            const bfc_option_t* option = bfc_find_option(argument);

            if (!option)
            {
                return bfc_make_errorf(ERR_ARGS, "Unknown argument: '%s'", argument);
            }

            const char* value = nullptr;

            if (option->value_name)
            {
                if (i + 1 >= argc)
                {
                    char error_message[512];

                    snprintf(
                        error_message, sizeof(error_message),
                        "Argument to '%s' is missing; expected <%s>", argument, option->value_name
                    );

                    return bfc_make_error(ERR_ARGS, error_message);
                }

                value = argv[++i];
            }

            bfc_error_t err = option->handler(cmd_args, value);
            if (err.code != ERR_OK)
            {
                return err;
            }

            if (cmd_args->ask_help || cmd_args->ask_version)
            {
                return BFC_ERR_OK;
            }

            continue;
        }

        if (cmd_args->input)
        {
            return bfc_make_error(ERR_ARGS, "Too many input file paths given!");
        }

        cmd_args->input = argument;
    }

    if (!cmd_args->input)
    {
        return bfc_make_error(ERR_ARGS, "No input file provided!");
    }

    return BFC_ERR_OK;
}
