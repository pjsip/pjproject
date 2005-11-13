/* $Id$
 *
 */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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


