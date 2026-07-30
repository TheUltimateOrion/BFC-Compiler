#ifndef BFC_CONFIG_H
#define BFC_CONFIG_H

#include <limits.h>
#include <stddef.h>

/*
 * Global compiler configuration shared by the frontend and all backends.
 *
 * The current machine model assumes one Brainfuck cell occupies one C byte.
 * Backends also rely on that byte containing exactly eight bits.
 */
static_assert(CHAR_BIT == 8, "bfc requires 8-bit bytes");

/* Number of byte-sized cells in the generated Brainfuck tape. */
#define BFC_TAPE_SIZE ((size_t) 30000)

/* Initial capacities for dynamically growing compiler data structures. */
#define BFC_INITIAL_ASM_CAPACITY ((size_t) 4096)
#define BFC_INITIAL_IR_CAPACITY ((size_t) 10)
#define BFC_INITIAL_IR_STACK_CAPACITY ((size_t) 5)

#endif  // BFC_CONFIG_H
