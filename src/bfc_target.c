/*
 * Target-triple parsing and host-target detection.
 *
 * The target table describes recognized triples. Backend availability is
 * checked separately by code generation, so a recognized triple may still be
 * rejected when its backend is not linked into bfc.
 */

#include "bfc_target.h"

#include <string.h>

#include "bfc_common.h"

/* One exact command-line triple mapped to bfc's compact target identity. */
typedef struct
{
    const char*  name;
    bfc_target_t target;
} bfc_target_entry_t;

/*
 * Keep aliases or additional triples here. Adding an entry only makes parsing
 * succeed; a matching backend must also be registered in bfc_codegen.c.
 */
static const bfc_target_entry_t BFC_TARGETS[] = {
    {
        .name = "aarch64-apple-darwin",
        .target = {
            .arch = BFC_ARCH_AARCH64,
            .os   = BFC_OS_MACOS,
        },
    },
    {
        .name = "x86_64-apple-darwin",
        .target = {
            .arch = BFC_ARCH_X86_64,
            .os   = BFC_OS_MACOS,
        },
    },
    {
        .name = "aarch64-unknown-linux-gnu",
        .target = {
            .arch = BFC_ARCH_AARCH64,
            .os   = BFC_OS_LINUX,
        },
    },
    {
        .name = "x86_64-unknown-linux-gnu",
        .target = {
            .arch = BFC_ARCH_X86_64,
            .os   = BFC_OS_LINUX,
        },
    },
    {
        .name = "aarch64-pc-windows-msvc",
        .target = {
            .arch = BFC_ARCH_AARCH64,
            .os   = BFC_OS_WINDOWS,
        },
    },
    {
        .name = "x86_64-pc-windows-msvc",
        .target = {
            .arch = BFC_ARCH_X86_64,
            .os   = BFC_OS_WINDOWS,
        },
    },
    {
        .name = "i386-pc-windows-msvc",
        .target = {
            .arch = BFC_ARCH_I386,
            .os   = BFC_OS_WINDOWS,
        },
    },
};

/* Parse by exact string match to avoid ambiguous partial target names. */
bfc_error_t bfc_target_parse(bfc_target_t* target, const char* triple)
{
    for (size_t i = 0; i < BFC_ARRAY_LENGTH(BFC_TARGETS); ++i)
    {
        if (strcmp(triple, BFC_TARGETS[i].name) == 0)
        {
            *target = BFC_TARGETS[i].target;
            return BFC_ERR_OK;
        }
    }

    return bfc_make_error(ERR_ARGS, "Unknown or unsupported target triple");
}

/*
 * Host detection supplies the default target when --target is omitted.
 * Predefined compiler macros describe the machine running bfc, not an
 * arbitrary cross-compilation target.
 */
bfc_target_t bfc_target_host(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    const bfc_arch_t arch = BFC_ARCH_AARCH64;
#elif defined(__x86_64__) || defined(_M_X64)
    const bfc_arch_t arch = BFC_ARCH_X86_64;
#elif defined(__i386__) || defined(_M_IX86)
    const bfc_arch_t arch = BFC_ARCH_I386;
#elif defined(__arm__) || defined(_M_ARM)
    const bfc_arch_t arch = BFC_ARCH_ARM32;
#else
    #error Unsupported host architecture
#endif

#if defined(__APPLE__)
    const bfc_os_t os = BFC_OS_MACOS;
#elif defined(__linux__)
    const bfc_os_t os = BFC_OS_LINUX;
#elif defined(_WIN32)
    const bfc_os_t os = BFC_OS_WINDOWS;
#else
    #error Unsupported host operating system
#endif

    return (bfc_target_t) {
        .arch = arch,
        .os   = os,
    };
}
