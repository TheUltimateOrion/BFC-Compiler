#ifndef BFC_CONFIG_H
#define BFC_CONFIG_H

#include <limits.h>
#include <stddef.h>

static_assert(CHAR_BIT == 8, "bfc requires 8-bit bytes");

#define BFC_TAPE_SIZE ((size_t) 30000)
#define BFC_INITIAL_ASM_CAPACITY ((size_t) 4096)
#define BFC_INITIAL_IR_CAPACITY ((size_t) 10)
#define BFC_INITIAL_IR_STACK_CAPACITY ((size_t) 5)

#endif
