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
/* 
 * $Log: /pjproject-0.3/pjlib/src/pj/os_error_unix.c $
 * 
 * 1     10/14/05 12:19a Bennylp
 * Created.
 *
 */
#include <pj/errno.h>
#include <pj/string.h>
#include <errno.h>

PJ_DEF(pj_status_t) pj_get_os_error(void)
{
    return PJ_STATUS_FROM_OS(errno);
}

PJ_DEF(void) pj_set_os_error(pj_status_t code)
{
    errno = PJ_STATUS_TO_OS(code);
}

PJ_DEF(pj_status_t) pj_get_netos_error(void)
{
    return PJ_STATUS_FROM_OS(errno);
}

PJ_DEF(void) pj_set_netos_error(pj_status_t code)
{
    errno = PJ_STATUS_TO_OS(code);
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
    const char *syserr = strerror(os_errcode);
    pj_size_t len = syserr ? strlen(syserr) : 0;

    if (len >= bufsize) len = bufsize - 1;
    if (len > 0)
	pj_memcpy(buf, syserr, len);
    buf[len] = '\0';
    return len;
}


