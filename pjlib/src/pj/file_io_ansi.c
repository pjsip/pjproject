/* $Id$ */
#include <pj/file_io.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <stdio.h>
#include <errno.h>

PJ_DEF(pj_status_t) pj_file_open( pj_pool_t *pool,
                                  const char *pathname, 
                                  unsigned flags,
                                  pj_oshandle_t *fd)
{
    char mode[8];
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

    if (p==mode)
        return PJ_EINVAL;

    *p++ = 'b';
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
        return PJ_RETURN_OS_ERROR(errno);
    }

    *size = bytes;
    return PJ_SUCCESS;
}

PJ_DEF(pj_bool_t) pj_file_eof(pj_oshandle_t fd, enum pj_file_access access)
{
    PJ_UNUSED_ARG(access);
    return feof((FILE*)fd) ? PJ_TRUE : 0;
}

PJ_DEF(pj_status_t) pj_file_setpos( pj_oshandle_t fd,
                                    pj_off_t offset,
                                    enum pj_file_seek_type whence)
{
    int mode;

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


