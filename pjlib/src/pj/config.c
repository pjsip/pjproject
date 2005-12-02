/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/config.c,v 1.1 2005/12/02 20:02:28 nn Exp $ */
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
#include <pj/compat.h>
#include <pj/log.h>

static const char *id = "";
const char *PJ_VERSION = "0.2.8";

PJ_DEF(void) pj_dump_config(void)
{
    PJ_LOG(3, (id, "PJLIB (c)2005 Benny Prijono"));
    PJ_LOG(3, (id, "Dumping configurations:"));
    PJ_LOG(3, (id, " PJ_VERSION               : %s", PJ_VERSION));
    PJ_LOG(3, (id, " PJ_FUNCTIONS_ARE_INLINED : %d", PJ_FUNCTIONS_ARE_INLINED));
    PJ_LOG(3, (id, " PJ_POOL_DEBUG            : %d", PJ_POOL_DEBUG));
    PJ_LOG(3, (id, " PJ_HAS_THREADS           : %d", PJ_HAS_THREADS));
    PJ_LOG(3, (id, " PJ_LOG_MAX_LEVEL         : %d", PJ_LOG_MAX_LEVEL));
#if defined(PJ_WIN32)
    PJ_LOG(3, (id, " PJ_WIN32                 : %d", PJ_WIN32));
#if defined(PJ_WIN32_WINNT)
    PJ_LOG(3, (id, " PJ_WIN32_WINNT           : %d", PJ_WIN32_WINNT));
#endif
#endif
#if defined(PJ_LINUX)
    PJ_LOG(3, (id, " PJ_LINUX                 : %d", PJ_LINUX));
#endif
    PJ_LOG(3, (id, " PJ_HAS_HIGH_RES_TIMER    : %d", PJ_HAS_HIGH_RES_TIMER));
    PJ_LOG(3, (id, " PJ_IS_LITTLE_ENDIAN      : %d", PJ_IS_LITTLE_ENDIAN));
    PJ_LOG(3, (id, " PJ_IS_BIG_ENDIAN         : %d", PJ_IS_BIG_ENDIAN));
    PJ_LOG(3, (id, " PJ_IOQUEUE_USE_WIN32_IOCP: %d", PJ_IOQUEUE_USE_WIN32_IOCP));
    PJ_LOG(3, (id, " PJ_IOQUEUE_MAX_HANDLES   : %d", PJ_IOQUEUE_MAX_HANDLES));
    PJ_LOG(3, (id, " PJ_IOQUEUE_MAX_HANDLES   : %d", PJ_IOQUEUE_MAX_HANDLES));
}
