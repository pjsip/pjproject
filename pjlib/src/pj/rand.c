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
#   define CSRAND_BCRYPT        1
#   include <windows.h>
#   include <bcrypt.h>
#   if defined(_MSC_VER)
#       pragma comment(lib, "bcrypt.lib")
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
/* A missing device means no generator (PJ_ENOTSUP), not a failed one. */
static pj_status_t urandom_fill(pj_uint8_t *p, pj_size_t len)
{
    pj_bool_t retried = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;
    int fd;

    fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0 && errno == EINTR)
        fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return (errno == ENOENT || errno == ENXIO || errno == ENODEV) ?
               PJ_ENOTSUP : PJ_STATUS_FROM_OS(errno);
    }

    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n > 0) {
            p += n;
            len -= (pj_size_t)n;
            retried = PJ_FALSE;
        } else if (n < 0 && (errno == EINTR || errno == EAGAIN) &&
                   !retried)
        {
            retried = PJ_TRUE;
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
    pj_bool_t retried = PJ_FALSE;

    while (len > 0) {
        ssize_t n = getrandom(p, len, 0);
        if (n > 0) {
            p += n;
            len -= (pj_size_t)n;
            retried = PJ_FALSE;
        } else if (n < 0 && (errno == ENOSYS || errno == EPERM) &&
                   p == start)
        {
            /* Kernel without the syscall, or a seccomp filter denying it.
             * A denied generator is a failure, not a missing one.
             */
            int err = errno;
            pj_status_t status = urandom_fill(start, total);
            if (status == PJ_ENOTSUP && err == EPERM)
                status = PJ_STATUS_FROM_OS(err);
            return status;
        } else if (n < 0 && errno == EINTR && !retried) {
            retried = PJ_TRUE;
        } else {
            return (n < 0) ? PJ_STATUS_FROM_OS(errno) : PJ_EUNKNOWN;
        }
    }
    return PJ_SUCCESS;
}
#endif

#if defined(CSRAND_BCRYPT)
static pj_status_t bcrypt_fill(pj_uint8_t *p, pj_size_t len)
{
    while (len > 0) {
        ULONG n = (len > 0x40000000) ? 0x40000000 : (ULONG)len;
        NTSTATUS st = BCryptGenRandom(NULL, p, n,
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (!BCRYPT_SUCCESS(st))
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

#if defined(CSRAND_BCRYPT)
    return bcrypt_fill((pj_uint8_t*)buf, len);
#elif defined(CSRAND_GETRANDOM)
    return getrandom_fill((pj_uint8_t*)buf, len);
#elif defined(CSRAND_ARC4RANDOM)
    arc4random_buf(buf, len);
    return PJ_SUCCESS;
#elif defined(CSRAND_URANDOM)
    return urandom_fill((pj_uint8_t*)buf, len);
#else
    return PJ_ENOTSUP;
#endif
}
