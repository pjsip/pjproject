/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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
#ifndef __PJ_ERRNO_H__
#define __PJ_ERRNO_H__

/**
 * @file errno.h
 * @brief PJLIB Error Codes
 */
#include <pj/types.h>
#include <pj/compat/errno.h>

PJ_BEGIN_DECL

/**
 * @defgroup pj_errno Error Codes
 * @ingroup PJ
 * @{
 *
 * In PJLIB, error/status codes from operating system are translated
 * into PJLIB error namespace, and stored in @a pj_status_t. All functions
 * that work with @a pj_status_t expect to get PJLIB error code instead
 * of native codes.
 *
 * @section pj_errno_retval Return Values
 *
 * All functions that returns @a pj_status_t returns @a PJ_SUCCESS if the
 * operation was completed successfully, or non-zero value to indicate 
 * error. If the error came from operating system, then the native error
 * code is translated/folded into PJLIB's error namespace by using
 * #PJ_STATUS_FROM_OS() macro. The function will do this automatically
 * before returning the error to caller.
 *
 * @section pj_errno_errmsg Error Message
 *
 * To get the error message corresponding to a particular code, use function
 * #pj_strerror(). This function expects error code in PJLIB error namespace,
 * not the native error code. Application can pass the value from the 
 * following sources to this function:
 *  - #pj_get_os_error()
 *  - #pj_get_netos_error()
 *  - any return value from function returning @a pj_status_t.
 *
 * Application MUST NOT pass native error code (such as error code from
 * functions like GetLastError() or errno) to PJLIB functions expecting
 * @a pj_status_t.
 *
 */

/**
 * Get the last platform error/status, folded into pj_status_t.
 * @return	OS dependent error code, folded into pj_status_t.
 * @remark	This function gets errno, or calls GetLastError() function and
 *		convert the code into pj_status_t with PJ_STATUS_FROM_OS. Do
 *		not call this for socket functions!
 * @see	pj_get_netos_error()
 */
PJ_DECL(pj_status_t) pj_get_os_error(void);

/**
 * Set last error.
 * @param code	pj_status_t
 */
PJ_DECL(void) pj_set_os_error(pj_status_t code);

/**
 * Get the last error from socket operations.
 * @return	Last socket error, folded into pj_status_t.
 */
PJ_DECL(pj_status_t) pj_get_netos_error(void);

/**
 * Set error code.
 * @param code	pj_status_t.
 */
PJ_DECL(void) pj_set_netos_error(pj_status_t code);


/**
 * Get the error message for the specified error code. The message
 * string will be NULL terminated.
 *
 * @param statcode  The error code.
 * @param buf	    Buffer to hold the error message string.
 * @param bufsize   Size of the buffer.
 *
 * @return	    The error message as NULL terminated string,
 *                  wrapped with pj_str_t.
 */
PJ_DECL(pj_str_t) pj_strerror( pj_status_t statcode, 
			       char *buf, pj_size_t bufsize);


/**
 * @hideinitializer
 * Return platform os error code folded into pj_status_t code. This is
 * the macro that is used throughout the library for all PJLIB's functions
 * that returns error from operating system. Application may override
 * this macro to reduce size (e.g. by defining it to always return 
 * #PJ_EUNKNOWN).
 *
 * Note:
 *  This macro MUST return non-zero value regardless whether zero is
 *  passed as the argument. The reason is to protect logic error when
 *  the operating system doesn't report error codes properly.
 *
 * @param os_code   Platform OS error code. This value may be evaluated
 *		    more than once.
 * @return	    The platform os error code folded into pj_status_t.
 */
#ifndef PJ_RETURN_OS_ERROR
#   define PJ_RETURN_OS_ERROR(os_code)   (os_code ? \
					    PJ_STATUS_FROM_OS(os_code) : -1)
#endif


/**
 * @hideinitializer
 * Fold a platform specific error into an pj_status_t code.
 *
 * @param e	The platform os error code.
 * @return	pj_status_t
 * @warning	Macro implementation; the syserr argument may be evaluated
 *		multiple times.
 */
#define PJ_STATUS_FROM_OS(e) (e == 0 ? PJ_SUCCESS : e + PJ_ERRNO_START_SYS)

/**
 * @hideinitializer
 * Fold an pj_status_t code back to the native platform defined error.
 *
 * @param e	The pj_status_t folded platform os error code.
 * @return	pj_os_err_type
 * @warning	macro implementation; the statcode argument may be evaluated
 *		multiple times.  If the statcode was not created by 
 *		pj_get_os_error or PJ_STATUS_FROM_OS, the results are undefined.
 */
#define PJ_STATUS_TO_OS(e) (e == 0 ? PJ_SUCCESS : e - PJ_ERRNO_START_SYS)


/**
 * @defgroup pj_errnum PJLIB's Own Error Codes
 * @ingroup pj_errno
 * @{
 */

/**
 * @hideinitializer
 * Unknown error has been reported.
 */
#define PJ_EUNKNOWN	    (PJ_ERRNO_START_STATUS + 1)	/* 70001 */
/**
 * @hideinitializer
 * The operation is pending and will be completed later.
 */
#define PJ_EPENDING	    (PJ_ERRNO_START_STATUS + 2)	/* 70002 */
/**
 * @hideinitializer
 * Too many connecting sockets.
 */
#define PJ_ETOOMANYCONN	    (PJ_ERRNO_START_STATUS + 3)	/* 70003 */
/**
 * @hideinitializer
 * Invalid argument.
 */
#define PJ_EINVAL	    (PJ_ERRNO_START_STATUS + 4)	/* 70004 */
/**
 * @hideinitializer
 * Name too long (eg. hostname too long).
 */
#define PJ_ENAMETOOLONG	    (PJ_ERRNO_START_STATUS + 5)	/* 70005 */
/**
 * @hideinitializer
 * Not found.
 */
#define PJ_ENOTFOUND	    (PJ_ERRNO_START_STATUS + 6)	/* 70006 */
/**
 * @hideinitializer
 * Not enough memory.
 */
#define PJ_ENOMEM	    (PJ_ERRNO_START_STATUS + 7)	/* 70007 */
/**
 * @hideinitializer
 * Bug detected!
 */
#define PJ_EBUG             (PJ_ERRNO_START_STATUS + 8)	/* 70008 */
/**
 * @hideinitializer
 * Operation timed out.
 */
#define PJ_ETIMEDOUT        (PJ_ERRNO_START_STATUS + 9)	/* 70009 */
/**
 * @hideinitializer
 * Too many objects.
 */
#define PJ_ETOOMANY         (PJ_ERRNO_START_STATUS + 10)/* 70010 */
/**
 * @hideinitializer
 * Object is busy.
 */
#define PJ_EBUSY            (PJ_ERRNO_START_STATUS + 11)/* 70011 */
/**
 * @hideinitializer
 * The specified option is not supported.
 */
#define PJ_ENOTSUP	    (PJ_ERRNO_START_STATUS + 12)/* 70012 */
/**
 * @hideinitializer
 * Invalid operation.
 */
#define PJ_EINVALIDOP	    (PJ_ERRNO_START_STATUS + 13)/* 70013 */
/**
 * @hideinitializer
 * Operation is cancelled.
 */
#define PJ_ECANCELLED	    (PJ_ERRNO_START_STATUS + 14)/* 70014 */
/**
 * @hideinitializer
 * Object already exists.
 */
#define PJ_EEXISTS          (PJ_ERRNO_START_STATUS + 15)/* 70015 */

/** @} */   /* pj_errnum */

/** @} */   /* pj_errno */


/**
 * PJ_ERRNO_START is where PJLIB specific error values start.
 */
#define PJ_ERRNO_START		20000

/**
 * PJ_ERRNO_SPACE_SIZE is the maximum number of errors in one of 
 * the error/status range below.
 */
#define PJ_ERRNO_SPACE_SIZE	50000

/**
 * PJ_ERRNO_START_STATUS is where PJLIB specific status codes start.
 * Effectively the error in this class would be 70000 - 119000.
 */
#define PJ_ERRNO_START_STATUS	(PJ_ERRNO_START + PJ_ERRNO_SPACE_SIZE)

/**
 * PJ_ERRNO_START_SYS converts platform specific error codes into
 * pj_status_t values.
 * Effectively the error in this class would be 120000 - 169000.
 */
#define PJ_ERRNO_START_SYS	(PJ_ERRNO_START_STATUS + PJ_ERRNO_SPACE_SIZE)

/**
 * PJ_ERRNO_START_USER are reserved for applications that use error
 * codes along with PJLIB codes.
 * Effectively the error in this class would be 170000 - 219000.
 */
#define PJ_ERRNO_START_USER	(PJ_ERRNO_START_SYS + PJ_ERRNO_SPACE_SIZE)


PJ_END_DECL

#endif	/* __PJ_ERRNO_H__ */

