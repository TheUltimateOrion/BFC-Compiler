/**
 * @file bfc_config.h
 * @brief Compiler-wide configuration constants.
 *
 * @details
 * Defines the Brainfuck machine model and initial capacities shared by the frontend and code-generation backends.
 */
#ifndef BFC_CONFIG_H
#define BFC_CONFIG_H

#include <limits.h>
#include <stddef.h>

/**
 * @brief Verifies the backend assumption that one C byte contains eight bits.
 */
static_assert(CHAR_BIT == 8, "bfc requires 8-bit bytes");

/**
 * @brief Defines the generated Brainfuck tape length in byte-sized cells.
 */
#define BFC_TAPE_SIZE ((size_t) 30000)
/**
 * @brief Defines initial capacities for dynamically growing compiler structures.
 */
#define BFC_INITIAL_ASM_CAPACITY ((size_t) 4096)
#define BFC_INITIAL_IR_CAPACITY ((size_t) 10)
#define BFC_INITIAL_IR_STACK_CAPACITY ((size_t) 5)

#endif
