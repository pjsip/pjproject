/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/log.h>

/**
 * \page page_pjlib_samples_log_c Example: Log, Hello World
 *
 * Very simple program to write log.
 *
 * \includelineno pjlib-samples/log.c
 */

int main()
{
    pj_status_t rc;

    // Error handling omited for clarity
    
    // Must initialize PJLIB first!
    rc = pj_init();

    PJ_LOG(3, ("main.c", "Hello world!"));

    return 0;
}

