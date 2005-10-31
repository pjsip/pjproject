/* $Header: /pjproject-0.3/pjlib/include/pj/compat/ctype.h 3     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/include/pj/compat/ctype.h $
 * 
 * 3     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 2     9/22/05 10:31a Bennylp
 * Moving all *.h files to include/.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_CTYPE_H__
#define __PJ_COMPAT_CTYPE_H__

/**
 * @file ctype.h
 * @brief Provides ctype function family.
 */

#if defined(PJ_HAS_CTYPE_H) && PJ_HAS_CTYPE_H != 0
#  include <ctype.h>
#else
#  define isalnum(c)	    (isalpha(c) || isdigit(c))
#  define isalpha(c)	    (islower(c) || isupper(c))
#  define isascii(c)	    (((unsigned char)(c))<=0x7f)
#  define isdigit(c)	    ((c)>='0' && (c)<='9')
#  define isspace(c)	    ((c)==' '  || (c)=='\t' ||\
			     (c)=='\n' || (c)=='\r' || (c)=='\v')
#  define islower(c)	    ((c)>='a' && (c)<='z')
#  define isupper(c)	    ((c)>='A' && (c)<='Z')
#  define isxdigit(c)	    (isdigit(c) || (tolower(c)>='a'&&tolower(c)<='f'))
#  define tolower(c)	    (((c) >= 'A' && (c) <= 'Z') ? (c)+('a'-'A') : (c))
#  define toupper(c)	    (((c) >= 'a' && (c) <= 'z') ? (c)-('a'-'A') : (c))
#endif

#define isblank(c)	    (c==' ' || c=='\t')


#endif	/* __PJ_COMPAT_CTYPE_H__ */
