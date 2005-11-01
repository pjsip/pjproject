/* $Header: /pjproject-0.3/pjlib/src/pj/os_error_linux_kernel.c 2     10/29/05 11:51a Bennylp $ */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pj/os_error_linux_kernel.c $
 * 
 * 2     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 1     10/19/05 1:48p Bennylp
 * Created.
 *
 */
#include <pj/string.h>
#include <pj/compat/errno.h>
#include <linux/config.h>
#include <linux/version.h>
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#include <linux/kernel.h>
#include <linux/errno.h>

int kernel_errno;

PJ_DEF(pj_status_t) pj_get_os_error(void)
{
    return errno;
}

PJ_DEF(void) pj_set_os_error(pj_status_t code)
{
    errno = code;
}

PJ_DEF(pj_status_t) pj_get_netos_error(void)
{
    return errno;
}

PJ_DEF(void) pj_set_netos_error(pj_status_t code)
{
    errno = code;
}

/* 
 * platform_strerror()
 *
 * Platform specific error message. This file is called by pj_strerror() 
 * in errno.c 
 */
int platform_strerror( pj_os_err_type os_errcode, 
                       char *buf, pj_size_t bufsize)
{
    char errmsg[32];
    int len;
    
    /* Handle EINVAL as special case so that it'll pass errno test. */
    if (os_errcode==EINVAL)
	strcpy(errmsg, "Invalid value");
    else
	sprintf(errmsg, "errno=%d", os_errcode);
    
    len = strlen(errmsg);

    if (len >= bufsize)
	len = bufsize-1;

    pj_memcpy(buf, errmsg, len);
    buf[len] = '\0';

    return len;
}


