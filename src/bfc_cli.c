#include "bfc_cli.h"

#include <stdio.h>
#include <string.h>

void bfc_cmd_help(void) {

	printf("OVERVIEW: bfc Brainfuck compiler\n\n");
	printf("USAGE: bfc [options] <file.bf>\n\n");
	printf("OPTIONS:\n");
	printf("  %-20s %s\n", "--fno-comments", "Do not treat lines starting with ';' as comments (for compatibility)");
	printf("  %-20s %s\n", "--help / -h",    "Display available options");
	printf("  %-20s %s\n", "-o <file>",      "Write output to <file>");
	printf("  %-20s %s\n", "-S",             "Only run compilation steps");
}

bfc_error_t bfc_process_args(bfc_args_t *const cmd_args, int argc, char **argv) {

	cmd_args->flags = 0;
	cmd_args->input = "";
	
	int i = 1;
	uint8_t output_num = 0;
	while (i < argc) {

		if (strcmp(argv[i], "-o") == 0) {
			if (i == argc - 1) 
				return bfc_make_error(ERR_ARGS, "Argument to '-o' is missing (expected 1 value)");

			cmd_args->outputs[output_num] = argv[i + 1];
			++output_num;
		} else if (strcmp(argv[i], "-S") == 0) {
			cmd_args->do_assemble = 0x1;
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			cmd_args->ask_help = 0x1;
			
			return BFC_ERR_OK;
		} else if (strcmp(argv[i], "--fno-comments") == 0) {
			cmd_args->f_no_comments = 1;
		} else if (argv[i][0] == '-') {
			char err_str[512];
			snprintf(err_str, sizeof(err_str), "Unknown argument: '%s'", argv[i]);

			return bfc_make_error(ERR_ARGS, err_str);
		} else {
			if (strcmp(cmd_args->input, "") != 0) 
				return bfc_make_error(ERR_ARGS, "Too many input file paths given!");

			cmd_args->input = argv[i];
		}

		++i;
	}

	if (strcmp(cmd_args->input, "") == 0) return bfc_make_error(ERR_ARGS, "No input files!");

	return BFC_ERR_OK; 
}

