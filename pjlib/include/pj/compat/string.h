/* $Id$
 *
 */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/string.h $
 * 
 * 3     9/22/05 10:31a Bennylp
 * Moving all *.h files to include/.
 * 
 * 2     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_STRING_H__
#define __PJ_COMPAT_STRING_H__

/**
 * @file string.h
 * @brief Provides string manipulation functions found in ANSI string.h.
 */

#if defined(PJ_HAS_STRING_H) && PJ_HAS_STRING_H != 0
#  include <string.h>
#else

    PJ_DECL(int) strcasecmp(const char *s1, const char *s2);
    PJ_DECL(int) strncasecmp(const char *s1, const char *s2, int len);

#endif

#if defined(_MSC_VER)
#  define strcasecmp	stricmp
#  define strncasecmp	strnicmp
#  define snprintf	_snprintf
#else
#  define stricmp	strcasecmp
#  define strnicmp	strncasecmp
#endif


#endif	/* __PJ_COMPAT_STRING_H__ */
