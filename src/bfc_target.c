#include "bfc_target.h"

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
