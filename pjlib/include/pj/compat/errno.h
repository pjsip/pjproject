/* $Header: /pjproject-0.3/pjlib/include/pj/compat/errno.h 2     10/14/05 12:26a Bennylp $ */
#ifndef __PJ_COMPAT_ERRNO_H__
#define __PJ_COMPAT_ERRNO_H__

#if defined(PJ_WIN32) && PJ_WIN32 != 0

    typedef unsigned long pj_os_err_type;
#   define pj_get_native_os_error()	    GetLastError()
#   define pj_get_native_netos_error()	    WSAGetLastError()

#elif (defined(PJ_LINUX) && PJ_LINUX != 0) || \
      (defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL != 0) || \
      (defined(PJ_SUNOS) && PJ_SUNOS != 0)

    typedef int pj_os_err_type;
#   define pj_get_native_os_error()	    (errno)
#   define pj_get_native_netos_error()	    (errno)

#else

#   error "Please define pj_os_err_type for this platform here!"

#endif


#endif	/* __PJ_COMPAT_ERRNO_H__ */

