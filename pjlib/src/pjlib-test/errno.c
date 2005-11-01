/* $Header: /pjproject-0.3/pjlib/src/pjlib-test/errno.c 4     10/14/05 3:05p Bennylp $ */
/*
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/errno.c $
 * 
 * 4     10/14/05 3:05p Bennylp
 * Fixed warning about strlen() on Linux.
 * 
 * 3     14/10/05 11:30 Bennylp
 * Verify the error message.
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     10/09/05 9:56p Bennylp
 * Created.
 *
 */
#include "test.h"
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/ctype.h>
#include <pj/compat/socket.h>
#include <pj/string.h>

#if INCLUDE_ERRNO_TEST

#define THIS_FILE   "errno"

#if defined(PJ_WIN32) && PJ_WIN32 != 0
#   include <windows.h>
#endif

#if defined(PJ_HAS_ERRNO_H) && PJ_HAS_ERRNO_H != 0
#   include <errno.h>
#endif

static void trim_newlines(char *s)
{
    while (*s) {
        if (*s == '\r' || *s == '\n')
            *s = ' ';
        ++s;
    }
}

int my_strncasecmp(const char *s1, const char *s2, int max_len)
{
    while (*s1 && *s2 && max_len > 0) {
        if (pj_tolower(*s1) != pj_tolower(*s2))
            return -1;
        ++s1;
        ++s2;
        --max_len;
    }
    return 0;
}

const char *my_stristr(const char *whole, const char *part)
{
    int part_len = strlen(part);
    while (*whole) {
        if (my_strncasecmp(whole, part, part_len) == 0)
            return whole;
        ++whole;
    }
    return NULL;
}

int errno_test(void)
{
    enum { CUT = 6 };
    pj_status_t rc;
    char errbuf[256];

    PJ_LOG(3,(THIS_FILE, "...errno test: check the msg carefully"));

    /* 
     * Windows platform error. 
     */
#   ifdef ERROR_INVALID_DATA
    rc = PJ_STATUS_FROM_OS(ERROR_INVALID_DATA);
    pj_set_os_error(rc);

    /* Whole */
    pj_strerror(rc, errbuf, sizeof(errbuf));
    trim_newlines(errbuf);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=ERROR_INVALID_DATA: '%s'", errbuf));
    if (my_stristr(errbuf, "invalid") == NULL) {
        PJ_LOG(3, (THIS_FILE, 
                   "...error: expecting \"invalid\" string in the msg"));
        return -20;
    }

    /* Cut version. */
    pj_strerror(rc, errbuf, CUT);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=ERROR_INVALID_DATA (cut): '%s'", errbuf));
#   endif

    /*
     * Unix errors
     */
#   ifdef EINVAL
    rc = PJ_STATUS_FROM_OS(EINVAL);
    pj_set_os_error(rc);

    /* Whole */
    pj_strerror(rc, errbuf, sizeof(errbuf));
    trim_newlines(errbuf);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=EINVAL: '%s'", errbuf));
    if (my_stristr(errbuf, "invalid") == NULL) {
        PJ_LOG(3, (THIS_FILE, 
                   "...error: expecting \"invalid\" string in the msg"));
        return -30;
    }

    /* Cut */
    pj_strerror(rc, errbuf, CUT);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=EINVAL (cut): '%s'", errbuf));
#   endif

    /* 
     * Windows WSA errors
     */
#   ifdef WSAEINVAL
    rc = PJ_STATUS_FROM_OS(WSAEINVAL);
    pj_set_os_error(rc);

    /* Whole */
    pj_strerror(rc, errbuf, sizeof(errbuf));
    trim_newlines(errbuf);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=WSAEINVAL: '%s'", errbuf));
    if (my_stristr(errbuf, "invalid") == NULL) {
        PJ_LOG(3, (THIS_FILE, 
                   "...error: expecting \"invalid\" string in the msg"));
        return -40;
    }

    /* Cut */
    pj_strerror(rc, errbuf, CUT);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=WSAEINVAL (cut): '%s'", errbuf));
#   endif

    pj_strerror(PJ_EBUG, errbuf, sizeof(errbuf));
    PJ_LOG(3,(THIS_FILE, "...msg for rc=PJ_EBUG: '%s'", errbuf));
    if (my_stristr(errbuf, "BUG") == NULL) {
        PJ_LOG(3, (THIS_FILE, 
                   "...error: expecting \"BUG\" string in the msg"));
        return -20;
    }

    pj_strerror(PJ_EBUG, errbuf, CUT);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=PJ_EBUG, cut at %d chars: '%s'", 
              CUT, errbuf));

    return 0;
}


#endif	/* INCLUDE_ERRNO_TEST */


