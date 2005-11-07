/* $Id$ */
#include <pj/file_access.h>
#include <pj/assert.h>
#include <pj/errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>	/* rename() */
#include <errno.h>

/*
 * pj_file_exists()
 */
PJ_DEF(pj_bool_t) pj_file_exists(const char *filename)
{
    struct stat buf;

    PJ_ASSERT_RETURN(filename, 0);

    if (stat(filename, &buf) != 0)
	return 0;

    return PJ_TRUE;
}


/*
 * pj_file_size()
 */
PJ_DEF(pj_off_t) pj_file_size(const char *filename)
{
    struct stat buf;

    PJ_ASSERT_RETURN(filename, -1);

    if (stat(filename, &buf) != 0)
	return -1;

    return buf.st_size;
}


/*
 * pj_file_delete()
 */
PJ_DEF(pj_status_t) pj_file_delete(const char *filename)
{
    PJ_ASSERT_RETURN(filename, PJ_EINVAL);

    if (unlink(filename)!=0) {
	return PJ_RETURN_OS_ERROR(errno);
    }
    return PJ_SUCCESS;
}


/*
 * pj_file_move()
 */
PJ_DEF(pj_status_t) pj_file_move( const char *oldname, const char *newname)
{
    PJ_ASSERT_RETURN(oldname && newname, PJ_EINVAL);

    if (rename(oldname, newname) != 0) {
	return PJ_RETURN_OS_ERROR(errno);
    }
    return PJ_SUCCESS;
}


/*
 * pj_file_getstat()
 */
PJ_DEF(pj_status_t) pj_file_getstat(const char *filename, 
				    pj_file_stat *statbuf)
{
    struct stat buf;

    PJ_ASSERT_RETURN(filename && statbuf, PJ_EINVAL);

    if (stat(filename, &buf) != 0) {
	return PJ_RETURN_OS_ERROR(errno);
    }

    statbuf->size = buf.st_size;
    statbuf->ctime.sec = buf.st_ctime;
    statbuf->ctime.msec = 0;
    statbuf->mtime.sec = buf.st_mtime;
    statbuf->mtime.msec = 0;
    statbuf->atime.sec = buf.st_atime;
    statbuf->atime.msec = 0;

    return PJ_SUCCESS;
}

