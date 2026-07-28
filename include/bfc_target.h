#ifndef BFC_TARGET_H
#define BFC_TARGET_H

#include "bfc_error.h"

typedef enum
{
    BFC_ARCH_X86_64,
    BFC_ARCH_I386,
    BFC_ARCH_AARCH64,
    BFC_ARCH_ARM32,
} bfc_arch_t;

typedef enum
{
    BFC_OS_WINDOWS,
    BFC_OS_MACOS,
    BFC_OS_LINUX,
} bfc_os_t;

typedef struct
{
    bfc_arch_t arch;
    bfc_os_t   os;
} bfc_target_t;

[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_target_parse(bfc_target_t* target, const char* triple);

[[gnu::const]]
bfc_target_t bfc_target_host(void);

#endif  // BFC_TARGET_H
