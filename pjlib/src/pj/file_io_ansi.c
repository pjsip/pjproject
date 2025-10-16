/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/file_io.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/limits.h>
#include <stdio.h>
#include <errno.h>
#if defined(PJ_HAS_FCNTL_H) && PJ_HAS_FCNTL_H != 0
#include <fcntl.h>
#endif
#if defined(PJ_HAS_SYS_TYPES_H) && PJ_HAS_SYS_TYPES_H != 0
#include <sys/types.h>
#endif
#include <sys/stat.h>

#if PJ_FILE_IO == PJ_FILE_IO_ANSI

PJ_DEF(pj_status_t) pj_file_open( pj_pool_t *pool,
                                  const char *pathname, 
                                  unsigned flags,
                                  pj_oshandle_t *fd)
{
    char mode[10];
    char *p = mode;

    PJ_ASSERT_RETURN(pathname && fd, PJ_EINVAL);
    PJ_UNUSED_ARG(pool);

    if ((flags & PJ_O_APPEND) == PJ_O_APPEND) {
        if ((flags & PJ_O_WRONLY) == PJ_O_WRONLY) {
            *p++ = 'a';
            if ((flags & PJ_O_RDONLY) == PJ_O_RDONLY)
                *p++ = '+';
        } else {
            /* This is invalid.
             * Can not specify PJ_O_RDONLY with PJ_O_APPEND! 
             */
        }
    } else {
        if ((flags & PJ_O_RDONLY) == PJ_O_RDONLY) {
            *p++ = 'r';
            if ((flags & PJ_O_WRONLY) == PJ_O_WRONLY)
                *p++ = '+';
        } else {
            *p++ = 'w';
        }
    }

#if defined(O_CLOEXEC)
    if ((flags & PJ_O_CLOEXEC) == PJ_O_CLOEXEC)
        *p++ = 'e';
#endif

    if (p==mode)
        return PJ_EINVAL;

    *p++ = 'b';

#if defined(_O_SEQUENTIAL)
    /* MSVC: file access is primarily sequential */
    if ((flags & PJ_O_SEQUENTIAL) == PJ_O_SEQUENTIAL) 
        *p++ = 'S';
#endif

#if defined(_O_RANDOM)
    /* MSVC: file access is primarily random */
    if ((flags & PJ_O_RANDOM) == PJ_O_RANDOM) 
        *p++ = 'R';
#endif

    *p++ = '\0';

    *fd = fopen(pathname, mode);
    if (*fd == NULL)
        return PJ_RETURN_OS_ERROR(errno);
    
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_close(pj_oshandle_t fd)
{
    PJ_ASSERT_RETURN(fd, PJ_EINVAL);
    if (fclose((FILE*)fd) != 0)
        return PJ_RETURN_OS_ERROR(errno);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_write( pj_oshandle_t fd,
                                   const void *data,
                                   pj_ssize_t *size)
{
    size_t written;

    clearerr((FILE*)fd);
    written = fwrite(data, 1, *size, (FILE*)fd);
    if (ferror((FILE*)fd)) {
        *size = -1;
        return PJ_RETURN_OS_ERROR(errno);
    }

    *size = written;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_read( pj_oshandle_t fd,
                                  void *data,
                                  pj_ssize_t *size)
{
    size_t bytes;

    clearerr((FILE*)fd);
    bytes = fread(data, 1, *size, (FILE*)fd);
    if (ferror((FILE*)fd)) {
        *size = -1;
        return PJ_RETURN_OS_ERROR(errno);
    }

    *size = bytes;
    return PJ_SUCCESS;
}

/*
PJ_DEF(pj_bool_t) pj_file_eof(pj_oshandle_t fd, enum pj_file_access access)
{
    PJ_UNUSED_ARG(access);
    return feof((FILE*)fd) ? PJ_TRUE : 0;
}
*/

PJ_DEF(pj_status_t) pj_file_setpos( pj_oshandle_t fd,
                                    pj_off_t offset,
                                    enum pj_file_seek_type whence)
{
    int mode;

    if ((sizeof(pj_off_t) > sizeof(long)) &&
        (offset > PJ_MAXLONG || offset < PJ_MINLONG)) 
    {
        return PJ_ENOTSUP;
    }

    switch (whence) {
    case PJ_SEEK_SET:
        mode = SEEK_SET; break;
    case PJ_SEEK_CUR:
        mode = SEEK_CUR; break;
    case PJ_SEEK_END:
        mode = SEEK_END; break;
    default:
        pj_assert(!"Invalid whence in file_setpos");
        return PJ_EINVAL;
    }

    if (fseek((FILE*)fd, (long)offset, mode) != 0)
        return PJ_RETURN_OS_ERROR(errno);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_getpos( pj_oshandle_t fd,
                                    pj_off_t *pos)
{
    long offset;

    offset = ftell((FILE*)fd);
    if (offset == -1) {
        *pos = -1;
        return PJ_RETURN_OS_ERROR(errno);
    }

    *pos = offset;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_flush(pj_oshandle_t fd)
{
    int rc;

    rc = fflush((FILE*)fd);
    if (rc == EOF) {
        return PJ_RETURN_OS_ERROR(errno);
    }

    return PJ_SUCCESS;
}

/*
 * pj_file_size_by_handle()
 */
PJ_DECL(pj_off_t) pj_file_size_by_handle(pj_oshandle_t fh)
{
#if 0
    // ineffective, not thread safe (because it changes file pointer),
    // but better than nothing.
    // use fstat() instead

    pj_off_t size = -1, pos;

    PJ_ASSERT_RETURN(fh, -1);

    /* get initial file pointer */
    if (pj_file_getpos(fh, &pos) != PJ_SUCCESS)
        return -1;

    if (pj_file_setpos(fh, 0, PJ_SEEK_END) != PJ_SUCCESS ||/* seek to end of file */
        pj_file_getpos(fh, &size) != PJ_SUCCESS)           /* get current file pointer */
    {
        size = -1;
    }
    
    /* seek back to reset file pointer */
    if (pj_file_setpos(fh, pos, PJ_SEEK_SET) != PJ_SUCCESS)
        return -1;

    return size;

#else

#if    (defined(PJ_HAS_UNISTD_H) && PJ_HAS_UNISTD_H != 0)
#   define _fileno(f) fileno(f)
#endif
    struct stat buf;
    int fd, result;

    fd = _fileno((FILE*)fh);
    if (fd == -1)
        return -1;

    result = fstat(fd, &buf);
    if (result != 0)
        return -1;

    return buf.st_size;
#endif
}

#endif //PJ_FILE_IO == PJ_FILE_IO_ANSI
