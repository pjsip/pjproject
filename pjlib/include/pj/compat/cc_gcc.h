/* $Header: /pjproject-0.3/pjlib/src/pj/compat/cc_gcc.h 2     9/17/05 10:37a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/cc_gcc.h $
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#ifndef __PJ_COMPAT_CC_GCC_H__
#define __PJ_COMPAT_CC_GCC_H__

/**
 * @file cc_gcc.h
 * @brief Describes GCC compiler specifics.
 */

#ifndef __GNUC__
#  error "This file is only for gcc!"
#endif

#define PJ_INLINE_SPECIFIER	static inline
#define PJ_THREAD_FUNC	
#define PJ_NORETURN		
#define PJ_ATTR_NORETURN	__attribute__ ((noreturn))

#define PJ_HAS_INT64		1

typedef long long pj_int64_t;
typedef unsigned long long pj_uint64_t;


#endif	/* __PJ_COMPAT_CC_GCC_H__ */

