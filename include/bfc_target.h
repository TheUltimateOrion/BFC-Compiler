/**
 * @file bfc_target.h
 * @brief Target architecture and operating-system interface.
 *
 * @details
 * Defines code-generation targets, target-triple parsing, and host-target detection.
 */
#ifndef BFC_TARGET_H
#define BFC_TARGET_H

#include "bfc_error.h"

/**
 * @brief Architectures understood by target parsing and backend selection.
 */
typedef enum
{
    BFC_ARCH_X86_64,
    BFC_ARCH_I386,
    BFC_ARCH_AARCH64,
    BFC_ARCH_ARM32,
} bfc_arch_t;

/**
 * @brief Operating systems that determine ABI and object-format behaviour.
 */
typedef enum
{
    BFC_OS_WINDOWS,
    BFC_OS_MACOS,
    BFC_OS_LINUX,
} bfc_os_t;

/**
 * @brief Architecture and operating-system pair used to select a backend.
 */
typedef struct
{
    bfc_arch_t arch;
    bfc_os_t   os;
} bfc_target_t;

/**
 * @brief Parses a supported target triple.
 *
 * @param[out] target Receives the parsed target on success.
 * @param[in] triple Null-terminated target triple.
 *
 * @return `BFC_ERR_OK` on success; otherwise an argument error.
 */
[[gnu::nonnull(1, 2)]]
bfc_error_t bfc_target_parse(bfc_target_t* target, const char* triple);

/**
 * @brief Returns the target corresponding to the compiler host.
 *
 * @return The host architecture and operating-system pair.
 */
[[gnu::const]]
bfc_target_t bfc_target_host(void);

#endif  // BFC_TARGET_H
