/*
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pj/rand.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/os.h>
#include <pj/compat/rand.h>

/* OS secure generator behind pj_csrand_bytes(), by preference */
#if defined(PJ_WIN32) && PJ_WIN32 != 0 && \
    !(defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE != 0)
#   define CSRAND_WIN32         1
#   include <windows.h>
#   if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0601
#       define CSRAND_BCRYPT    1
#       include <bcrypt.h>
#       if defined(_MSC_VER)
#           pragma comment(lib, "bcrypt.lib")
#       endif
#   else
        /* RtlGenRandom(): unlike bcrypt.dll, it exists since XP */
#       if defined(_MSC_VER)
#           pragma comment(lib, "advapi32.lib")
#       endif
        PJ_BEGIN_DECL
        BOOLEAN NTAPI SystemFunction036(PVOID buf, ULONG len);
        PJ_END_DECL
#   endif
#elif defined(PJ_HAS_GETRANDOM) && PJ_HAS_GETRANDOM != 0
    /* /dev/urandom stays as the fallback for kernels without the syscall */
#   define CSRAND_GETRANDOM     1
#   define CSRAND_URANDOM       1
#   include <sys/random.h>
#elif defined(PJ_HAS_ARC4RANDOM_BUF) && PJ_HAS_ARC4RANDOM_BUF != 0
#   define CSRAND_ARC4RANDOM    1
#elif defined(PJ_HAS_FCNTL_H) && PJ_HAS_FCNTL_H != 0 && \
      defined(PJ_HAS_UNISTD_H) && PJ_HAS_UNISTD_H != 0
#   define CSRAND_URANDOM       1
#endif

#if defined(CSRAND_URANDOM)
#   include <errno.h>
#   include <fcntl.h>
#   include <unistd.h>
#   ifndef O_CLOEXEC
#       define O_CLOEXEC        0
#   endif
    /* Interrupted calls in a row before giving up */
#   define CSRAND_MAX_EINTR     10
#endif

PJ_DEF(void) pj_srand(unsigned int seed)
{
    PJ_CHECK_STACK();
    platform_srand(seed);
}

PJ_DEF(int) pj_rand(void)
{
    PJ_CHECK_STACK();
    return platform_rand();
}

#if defined(CSRAND_URANDOM)
static pj_status_t urandom_fill(pj_uint8_t *p, pj_size_t len)
{
    pj_status_t status = PJ_SUCCESS;
    int intr = 0;
    int fd;

    do {
        fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR && ++intr < CSRAND_MAX_EINTR);
    if (fd < 0)
        return PJ_STATUS_FROM_OS(errno);

    intr = 0;

    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n > 0) {
            p += n;
            len -= (pj_size_t)n;
            intr = 0;
        } else if (n < 0 && errno == EINTR && ++intr < CSRAND_MAX_EINTR) {
            continue;
        } else {
            status = (n < 0) ? PJ_STATUS_FROM_OS(errno) : PJ_EEOF;
            break;
        }
    }

    close(fd);
    return status;
}
#endif

#if defined(CSRAND_GETRANDOM)
static pj_status_t getrandom_fill(pj_uint8_t *p, pj_size_t len)
{
    pj_uint8_t *start = p;
    pj_size_t total = len;
    int intr = 0;

    while (len > 0) {
        ssize_t n = getrandom(p, len, 0);
        if (n > 0) {
            p += n;
            len -= (pj_size_t)n;
            intr = 0;
        } else if (n < 0 && (errno == ENOSYS || errno == EPERM) &&
                   p == start)
        {
            /* Kernel without the syscall, or a seccomp filter denying it.
             * The kernel has a generator, so a missing device is an error.
             */
            return urandom_fill(start, total);
        } else if (n < 0 && errno == EINTR && ++intr < CSRAND_MAX_EINTR) {
            continue;
        } else {
            return (n < 0) ? PJ_STATUS_FROM_OS(errno) : PJ_EUNKNOWN;
        }
    }
    return PJ_SUCCESS;
}
#endif

#if defined(CSRAND_WIN32)
static pj_status_t win32_fill(pj_uint8_t *p, pj_size_t len)
{
    while (len > 0) {
        ULONG n = (len > 0x40000000) ? 0x40000000 : (ULONG)len;
#if defined(CSRAND_BCRYPT)
        if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, p, n,
                                            BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
#else
        if (!SystemFunction036(p, n))
#endif
            return PJ_EUNKNOWN;
        p += n;
        len -= n;
    }
    return PJ_SUCCESS;
}
#endif

PJ_DEF(pj_status_t) pj_csrand_bytes(void *buf, pj_size_t len)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(buf || len == 0, PJ_EINVAL);

    if (len == 0)
        return PJ_SUCCESS;

#if defined(CSRAND_WIN32)
    return win32_fill((pj_uint8_t*)buf, len);
#elif defined(CSRAND_GETRANDOM)
    return getrandom_fill((pj_uint8_t*)buf, len);
#elif defined(CSRAND_ARC4RANDOM)
    arc4random_buf(buf, len);
    return PJ_SUCCESS;
#elif defined(CSRAND_URANDOM)
    {
        /* Here the platform may lack a generator, so a missing device
         * means PJ_ENOTSUP rather than a failure.
         */
        pj_status_t status = urandom_fill((pj_uint8_t*)buf, len);
        if (status == PJ_STATUS_FROM_OS(ENOENT) ||
            status == PJ_STATUS_FROM_OS(ENXIO) ||
            status == PJ_STATUS_FROM_OS(ENODEV))
        {
            status = PJ_ENOTSUP;
        }
        return status;
    }
#else
    return PJ_ENOTSUP;
#endif
}
