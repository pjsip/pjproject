
/* Intel x86 */
#if defined(__i386) || defined(i386) || defined(_i386_) ||                     \
    defined(__i386__) || defined(x86) || defined(_X86_) || defined(__X86__) || \
    defined(__I86__) || defined(_M_IX86) || defined(__INTEL__)

#error arch:i386

/* AMD64 (x86_64) */
#elif defined(__amd64) || defined(__amd64__) || defined(__x86_64) ||           \
    defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)

#error arch:x86_64

/* Intel Itanium (IA-64) */
#elif defined(__ia64__) || defined(_IA64) || defined(__IA64__) ||              \
    defined(_M_IA64) || defined(__itanium__)

#error arch:ia64

/* Motorola 68k */
#elif defined(__m68k__) || defined(M68000) || defined(__MC68K__)

#error arch:m68k

/* DEC Alpha */
#elif defined(__alpha) || defined(__alpha__) || defined(_M_ALPHA)

#error arch:alpha

/* MIPS */
#elif defined(__mips) || defined(__mips__) || defined(mips) ||                 \
    defined(__MIPS__) || defined(_MIPS_) || defined(MIPS)

#error arch:mips

/* Sun SPARC */
#elif defined(__sparc) || defined(__sparc__)

#error arch:sparc

/* ARM */
#elif defined(__arm) || defined(__arm__) || defined(__thumb__) ||              \
    defined(ARM) || defined(_ARM) || defined(_ARM_) || defined(_M_ARM) ||      \
    defined(_M_ARMT) || defined(__TARGET_ARCH_ARM) ||                          \
    defined(__TARGET_ARCH_THUMB) || defined(_M_ARM64) || defined(__aarch64__)

#if defined(ARM64) || defined(__aarch64__) || defined(_M_ARM64)
#error arch:arm64
#elif defined(ARMV7) || defined(__ARM_ARCH_7__)
#error arch:armv7
#elif defined(ARMV4) || defined(__ARM_ARCH_4__)
#error arch:armv4
#else
#error arch:arm

#endif

/* PowerPC */
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) ||  \
    defined(__POWERPC__) || defined(__ppc) || defined(__ppc__) ||              \
    defined(__ppc64__) || defined(__PPC__) || defined(__PPC64__) ||            \
    defined(_ARCH_PPC) || defined(_ARCH_PPC6) || defined(_M_PPC)

#error arch:powerpc

/* NIOS2 */
#elif defined(__nios2) || defined(__nios2__) || defined(__NIOS2__) ||          \
    defined(__M_NIOS2) || defined(_ARCH_NIOS2)

#error arch:nios2

#endif
