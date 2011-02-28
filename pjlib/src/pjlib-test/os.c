/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include "test.h"
#include <pj/log.h>
#include <pj/os.h>

#if INCLUDE_OS_TEST
int os_test(void)
{
    const pj_sys_info *si;
    int rc = 0;

    si = pj_get_sys_info();
    PJ_LOG(3,("", "   machine:  %s", si->machine.ptr));
    PJ_LOG(3,("", "   os_name:  %s", si->os_name.ptr));
    PJ_LOG(3,("", "   os_ver:   0x%x", si->os_ver));
    PJ_LOG(3,("", "   sdk_name: %s", si->sdk_name.ptr));
    PJ_LOG(3,("", "   sdk_ver:  0x%x", si->sdk_ver));
    PJ_LOG(3,("", "   info:     %s", si->info.ptr));

    return rc;
}

#else
int dummy_os_var;
#endif

