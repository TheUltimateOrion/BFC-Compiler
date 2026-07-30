#ifndef BFC_TARGET_H
#define BFC_TARGET_H

#include "bfc_error.h"

/* Architectures understood by target parsing and backend selection. */
typedef enum
{
    BFC_ARCH_X86_64,
    BFC_ARCH_I386,
    BFC_ARCH_AARCH64,
    BFC_ARCH_ARM32,
} bfc_arch_t;

/* Operating systems that may contribute ABI and object-format differences. */
typedef enum
{
    BFC_OS_WINDOWS,
    BFC_OS_MACOS,
    BFC_OS_LINUX,
} bfc_os_t;

/* Complete code-generation target used to select one backend. */
typedef struct
{
    bfc_arch_t arch;
    bfc_os_t   os;
} bfc_target_t;

/*
 * Parse a supported target triple into target.
 * target is written only when the triple is recognized.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_target_parse(bfc_target_t* target, const char* triple);

/* Return the architecture and operating system of the compiler host. */
[[gnu::const]]
bfc_target_t bfc_target_host(void);

#endif  // BFC_TARGET_H
