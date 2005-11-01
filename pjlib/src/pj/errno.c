/* $Header: /pjproject-0.3/pjlib/src/pj/errno.c 2     10/14/05 12:26a Bennylp $ */
/*
 * $Log: /pjproject-0.3/pjlib/src/pj/errno.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     10/08/05 9:53a Bennylp
 * Created.
 *
 */
#include <pj/errno.h>
#include <pj/string.h>
#include <pj/compat/sprintf.h>

/* Prototype for platform specific error message, which will be defined 
 * in separate file.
 */
extern int platform_strerror( pj_os_err_type code, 
                              char *buf, pj_size_t bufsize );

/* PJLIB's own error codes/messages */
static const struct 
{
    int code;
    const char *msg;
} err_str[] = 
{
    { PJ_EUNKNOWN,      "Unknown Error" },
    { PJ_EPENDING,      "Pending operation" },
    { PJ_ETOOMANYCONN,  "Too many connecting sockets" },
    { PJ_EINVAL,        "Invalid value or argument" },
    { PJ_ENAMETOOLONG,  "Name too long" },
    { PJ_ENOTFOUND,     "Not found" },
    { PJ_ENOMEM,        "Not enough memory" },
    { PJ_EBUG,          "BUG DETECTED!" },
    { PJ_ETIMEDOUT,     "Operation timed out" },
    { PJ_ETOOMANY,      "Too many objects of the specified type"},
    { PJ_EBUSY,         "Object is busy"},
    { PJ_ENOTSUP,	"Option/operation is not supported"},
    { PJ_EINVALIDOP,	"Invalid operation"}
};

/*
 * pjlib_error()
 *
 * Retrieve message string for PJLIB's own error code.
 */
static int pjlib_error(pj_status_t code, char *buf, pj_size_t size)
{
    unsigned i;

    for (i=0; i<sizeof(err_str)/sizeof(err_str[0]); ++i) {
        if (err_str[i].code == code) {
            pj_size_t len = strlen(err_str[i].msg);
            if (len >= size) len = size-1;
            pj_memcpy(buf, err_str[i].msg, len);
            buf[len] = '\0';
            return len;
        }
    }

    *buf++ = '?';
    *buf++ = '?';
    *buf++ = '?';
    *buf++ = '\0';
    return 3;
}

/*
 * pj_strerror()
 */
PJ_DEF(pj_str_t) pj_strerror( pj_status_t statcode, 
			      char *buf, pj_size_t bufsize )
{
    int len = -1;
    pj_str_t errstr;

    if (statcode < PJ_ERRNO_START + PJ_ERRNO_SPACE_SIZE) {
        len = pj_snprintf( buf, bufsize, "Unknown error %d", statcode);

    } else if (statcode < PJ_ERRNO_START_STATUS + PJ_ERRNO_SPACE_SIZE) {
        len = pjlib_error(statcode, buf, bufsize);

    } else if (statcode < PJ_ERRNO_START_SYS + PJ_ERRNO_SPACE_SIZE) {
        len = platform_strerror(PJ_STATUS_TO_OS(statcode), buf, bufsize);

    } else if (statcode < PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE) {
        len = pj_snprintf( buf, bufsize, "User error %d", statcode);

    } else {
        len = pj_snprintf( buf, bufsize, "Invalid error %d", statcode);

    }

    if (len < 1) {
        *buf = '\0';
        len = 0;
    }

    errstr.ptr = buf;
    errstr.slen = len;

    return errstr;
}

