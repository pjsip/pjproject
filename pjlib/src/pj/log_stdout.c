/* $Header: /pjproject/pjlib/src/pj/log_stdout.c 1     6/13/05 7:09p Bennylp $ */
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
#include <pj/log.h>
#include <pj/os.h>

#define CLR_FATAL    (PJ_TERM_COLOR_BRIGHT | PJ_TERM_COLOR_R)
#define CLR_WARNING  (PJ_TERM_COLOR_BRIGHT | PJ_TERM_COLOR_R | PJ_TERM_COLOR_G)
#define CLR_INFO     (PJ_TERM_COLOR_BRIGHT | PJ_TERM_COLOR_R | PJ_TERM_COLOR_G | \
		      PJ_TERM_COLOR_B)
#define CLR_DEFAULT  (PJ_TERM_COLOR_R | PJ_TERM_COLOR_G | PJ_TERM_COLOR_B)

static void term_set_color(int level)
{
    unsigned attr = 0;
    switch (level) {
    case 0:
    case 1: attr = CLR_FATAL; 
	break;
    case 2: attr = CLR_WARNING; 
	break;
    case 3: attr = CLR_INFO; 
	break;
    default:
	attr = CLR_DEFAULT;
	break;
    }

    pj_term_set_color(attr);
}

PJ_DEF(void) pj_log_to_stdout(int level, const char *buffer, int len)
{
    PJ_UNUSED_ARG(len)

    /* Copy to terminal/file. */
    term_set_color(level);
    fputs(buffer, stdout);
    pj_term_set_color(CLR_DEFAULT);

    fflush(stdout);
}

