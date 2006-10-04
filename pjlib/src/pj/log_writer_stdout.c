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
#include <pj/log.h>
#include <pj/os.h>
#include <pj/compat/stdfileio.h>

#define CLR_FATAL    (PJ_TERM_COLOR_BRIGHT | PJ_TERM_COLOR_R)
#define CLR_WARNING  (PJ_TERM_COLOR_BRIGHT | PJ_TERM_COLOR_R | PJ_TERM_COLOR_G)
#define CLR_INFO     (PJ_TERM_COLOR_BRIGHT | PJ_TERM_COLOR_R | PJ_TERM_COLOR_G | \
		      PJ_TERM_COLOR_B)
#define CLR_DEFAULT  (PJ_TERM_COLOR_R | PJ_TERM_COLOR_G | PJ_TERM_COLOR_B)

static void term_set_color(int level)
{
#if defined(PJ_TERM_HAS_COLOR) && PJ_TERM_HAS_COLOR != 0
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
#endif
}

static void term_restore_color(void)
{
#if defined(PJ_TERM_HAS_COLOR) && PJ_TERM_HAS_COLOR != 0
    pj_term_set_color(CLR_DEFAULT);
#endif
}


PJ_DEF(void) pj_log_write(int level, const char *buffer, int len)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(len);

    /* Copy to terminal/file. */
    term_set_color(level);
    puts(buffer);
    term_restore_color();
}

