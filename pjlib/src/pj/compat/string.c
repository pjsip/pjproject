/* $Id$ */
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
#include <pj/types.h>
#include <pj/compat/string.h>
#include <pj/ctype.h>

PJ_DEF(int) strcasecmp(const char *s1, const char *s2)
{
    while ((*s1==*s2) || (pj_tolower(*s1)==pj_tolower(*s2))) {
	if (!*s1++)
	    return 0;
	++s2;
    }
    return (pj_tolower(*s1) < pj_tolower(*s2)) ? -1 : 1;
}

PJ_DEF(int) strncasecmp(const char *s1, const char *s2, int len)
{
    if (!len) return 0;

    while ((*s1==*s2) || (pj_tolower(*s1)==pj_tolower(*s2))) {
	if (!*s1++ || --len <= 0)
	    return 0;
	++s2;
    }
    return (pj_tolower(*s1) < pj_tolower(*s2)) ? -1 : 1;
}

