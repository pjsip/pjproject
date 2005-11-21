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
#include <pj/config.h>
#include <pj/log.h>
#include <pj/ioqueue.h>

static const char *id = "config.c";
const char *PJ_VERSION = "0.3.0-pre4";

PJ_DEF(void) pj_dump_config(void)
{
    PJ_LOG(3, (id, "PJLIB (c)2005 Benny Prijono"));
    PJ_LOG(3, (id, "Dumping configurations:"));
    PJ_LOG(3, (id, " PJ_VERSION               : %s", PJ_VERSION));
    PJ_LOG(3, (id, " PJ_DEBUG                 : %d", PJ_DEBUG));
    PJ_LOG(3, (id, " PJ_FUNCTIONS_ARE_INLINED : %d", PJ_FUNCTIONS_ARE_INLINED));
    PJ_LOG(3, (id, " PJ_POOL_DEBUG            : %d", PJ_POOL_DEBUG));
    PJ_LOG(3, (id, " PJ_HAS_THREADS           : %d", PJ_HAS_THREADS));
    PJ_LOG(3, (id, " PJ_LOG_MAX_LEVEL         : %d", PJ_LOG_MAX_LEVEL));
    PJ_LOG(3, (id, " PJ_LOG_MAX_SIZE          : %d", PJ_LOG_MAX_SIZE));
    PJ_LOG(3, (id, " PJ_LOG_USE_STACK_BUFFER  : %d", PJ_LOG_USE_STACK_BUFFER));
    PJ_LOG(3, (id, " PJ_HAS_TCP               : %d", PJ_HAS_TCP));
    PJ_LOG(3, (id, " PJ_MAX_HOSTNAME          : %d", PJ_MAX_HOSTNAME));
    PJ_LOG(3, (id, " PJ_HAS_SEMAPHORE         : %d", PJ_HAS_SEMAPHORE));
    PJ_LOG(3, (id, " PJ_HAS_EVENT_OBJ         : %d", PJ_HAS_EVENT_OBJ));
    PJ_LOG(3, (id, " PJ_HAS_HIGH_RES_TIMER    : %d", PJ_HAS_HIGH_RES_TIMER));
    PJ_LOG(3, (id, " PJ_(endianness)          : %s", 
	       (PJ_IS_BIG_ENDIAN?"big-endian":"little-endian")));
    PJ_LOG(3, (id, " ioqueue type             : %s", pj_ioqueue_name()));
    PJ_LOG(3, (id, " PJ_IOQUEUE_MAX_HANDLES   : %d", PJ_IOQUEUE_MAX_HANDLES));
}

