#include "bfc_ir.h"

#include <stdlib.h>

bfc_ir_instr_t bfc_ir_make_imm_instr(const bfc_ir_token_type_t ir_token_type, const ssize_t imm) {

	return (bfc_ir_instr_t) {
		.op = ir_token_type,
		.val = {imm},
	};
}

bfc_ir_instr_t bfc_ir_make_zero_instr(const bfc_ir_token_type_t ir_token_type) {

	return (bfc_ir_instr_t) { 
		.op = ir_token_type,
	};
}

bfc_error_t bfc_ir_create(bfc_ir_block_t **root_block, const bfc_token_stream_t *const tok_stream) {

	bfc_error_t err = BFC_ERR_ALLOC;
	*root_block = NULL;
	
	bfc_ir_stack_t stack = (bfc_ir_stack_t) {
		.capacity = 5,
		.length = 0,
	};

	stack.blocks = (bfc_ir_block_t**) malloc(stack.capacity * sizeof(bfc_ir_block_t*));
	if (!stack.blocks) goto end;

	stack.blocks[stack.length] = (bfc_ir_block_t*) malloc(sizeof(bfc_ir_block_t));
	if (!stack.blocks[stack.length]) goto end;

	bfc_ir_block_t *current_block = stack.blocks[stack.length++];
	current_block->instr = NULL;
	current_block->capacity = 10;
	current_block->length = 0;

	current_block->instr = (bfc_ir_instr_t*) malloc(current_block->capacity * sizeof(bfc_ir_instr_t));
	if (!current_block->instr) goto end;

	size_t i = 0;
	while (i < tok_stream->length) {
		if (stack.length >= stack.capacity) {
			stack.capacity *= 2;

			bfc_ir_block_t **tmp = (bfc_ir_block_t**) realloc(
				stack.blocks, 
				stack.capacity * sizeof(bfc_ir_block_t*)
			);

			if (!tmp) goto end;

			stack.blocks = tmp;
		}

		if (current_block->length >= current_block->capacity) {
			current_block->capacity *= 2;

			bfc_ir_instr_t *tmp = (bfc_ir_instr_t*) realloc(
				current_block->instr, 
				current_block->capacity * sizeof(bfc_ir_instr_t)
			);
			
			if (!tmp) goto end;

			current_block->instr = tmp;
		}

		switch (tok_stream->tokens[i].type) {
			case TT_INC: {
				current_block->instr[current_block->length++] = bfc_ir_make_imm_instr(IR_ADD, 1);
			} break;

			case TT_DEC: {
				current_block->instr[current_block->length++] = bfc_ir_make_imm_instr(IR_ADD, -1);
			} break;

			case TT_PTR_LEFT: {
				current_block->instr[current_block->length++] = bfc_ir_make_imm_instr(IR_MOVE, -1);
			} break;

			case TT_PTR_RIGHT: {
				current_block->instr[current_block->length++] = bfc_ir_make_imm_instr(IR_MOVE, 1);
			} break;

			case TT_INPUT: {
				current_block->instr[current_block->length++] = bfc_ir_make_zero_instr(IR_GET);
			} break;

			case TT_OUTPUT: {
				current_block->instr[current_block->length++] = bfc_ir_make_zero_instr(IR_PUT);
			} break;

			case TT_LOOP_START: {
				bfc_ir_instr_t loop_instr = (bfc_ir_instr_t) {
					.op = IR_LOOP,
					.val = { 
						.body = (struct bfc_ir_block_t*) malloc(sizeof(bfc_ir_block_t)) 
					},
				};

				if (!loop_instr.val.body) goto end;

				current_block->instr[current_block->length++] = loop_instr;

				stack.blocks[stack.length] = (bfc_ir_block_t*) loop_instr.val.body;

				current_block = stack.blocks[stack.length++];
				current_block->capacity = 10;
				current_block->length = 0;

				current_block->instr = (bfc_ir_instr_t*) malloc(
					current_block->capacity * sizeof(bfc_ir_instr_t)
				);

				if (!current_block->instr) goto end;
			} break;

			case TT_LOOP_END: {
				--stack.length;
				current_block = stack.blocks[stack.length - 1];
			} break;
		}

		++i;
	}

	err = BFC_ERR_OK;

end:
	if (stack.blocks) {
		if (stack.blocks[0]) *root_block = stack.blocks[0];
		free(stack.blocks);
	}

	return err;
}

bfc_error_t bfc_ir_optimize_rep(bfc_ir_block_t **ir_block) {
	
	if ((*ir_block)->length == 0) return BFC_ERR_OK;

	bfc_error_t err = BFC_ERR_ALLOC;

	bfc_ir_block_t *optimized_block = (bfc_ir_block_t*) malloc(sizeof(bfc_ir_block_t));
	if (!optimized_block) goto end;
	optimized_block->instr = NULL;
	optimized_block->capacity = (*ir_block)->capacity;
	optimized_block->length = 0;

	optimized_block->instr = (bfc_ir_instr_t*) malloc(optimized_block->capacity * sizeof(bfc_ir_instr_t));
	if (!optimized_block->instr) goto end;

	bfc_ir_instr_t prev_instr = (*ir_block)->instr[0];
	ssize_t instr_delta = 0;
	size_t i = 0;
	while (i < (*ir_block)->length) {
		if ((*ir_block)->instr[i].op == IR_ADD || (*ir_block)->instr[i].op == IR_MOVE) {
			do {
				instr_delta += (*ir_block)->instr[i].val.imm;
				prev_instr = (*ir_block)->instr[i++];
			} while(i < (*ir_block)->length && (*ir_block)->instr[i].op == prev_instr.op);

			if (instr_delta != 0)
				optimized_block->instr[optimized_block->length++] = bfc_ir_make_imm_instr(prev_instr.op, instr_delta);
			
			instr_delta = 0;
		} else {
			if ((*ir_block)->instr[i].op == IR_LOOP) {
				err = bfc_ir_optimize_rep((bfc_ir_block_t**) &(*ir_block)->instr[i].val.body);

				if (err.code != ERR_OK) goto end;
			}
			
			optimized_block->instr[optimized_block->length++] = (*ir_block)->instr[i];
			prev_instr = (*ir_block)->instr[i++];
		}
	}
	
	free((*ir_block)->instr);
	free(*ir_block);

	*ir_block = optimized_block;
	optimized_block = NULL; 

	err = BFC_ERR_OK;

end:
	if (optimized_block) {
		free(optimized_block->instr);
		free(optimized_block);
	}

	return err;
}

void bfc_ir_destroy(bfc_ir_block_t **proot_block) {

	if (!proot_block || !*proot_block) return;

	for (size_t i = 0; i < (*proot_block)->length; ++i) {
		if ((*proot_block)->instr[i].op == IR_LOOP && (*proot_block)->instr[i].val.body)
			bfc_ir_destroy((bfc_ir_block_t**) &(*proot_block)->instr[i].val.body);
	}

	free((*proot_block)->instr);
	free(*proot_block);

	*proot_block = NULL;
}
