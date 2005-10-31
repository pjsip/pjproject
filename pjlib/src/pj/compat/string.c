/* $Header: /pjproject-0.3/pjlib/src/pj/compat/string.c 1     9/22/05 10:43a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/string.c $
 * 
 * 1     9/22/05 10:43a Bennylp
 * Created.
 * 
 */
#include <pj/types.h>
#include <pj/compat/string.h>
#include <pj/ctype.h>

PJ_DEF(int) strcasecmp(const char *s1, const char *s2)
{
    while ((*s1==*s2) || (pj_tolower(*s1)==pj_tolower(*s2))) {
	if (!*s1++)
	    return 0;
	++s2;
    }
    return (pj_tolower(*s1) < pj_tolower(*s2)) ? -1 : 1;
}

PJ_DEF(int) strncasecmp(const char *s1, const char *s2, int len)
{
    if (!len) return 0;

    while ((*s1==*s2) || (pj_tolower(*s1)==pj_tolower(*s2))) {
	if (!*s1++ || --len <= 0)
	    return 0;
	++s2;
    }
    return (pj_tolower(*s1) < pj_tolower(*s2)) ? -1 : 1;
}

