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
#include <pj/guid.h>
#include <pj/os.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/compat/sprintf.h>

const unsigned PJ_GUID_STRING_LENGTH=20;

static void init_mac_address(unsigned char mac_addr[16])
{
    unsigned long *ulval1 = (unsigned long*) &mac_addr[0];
    unsigned short *usval1 = (unsigned short*) &mac_addr[4];

    *ulval1 = pj_rand();
    *usval1 = (unsigned short) pj_rand();
}

PJ_DEF(pj_str_t*) pj_generate_unique_string(pj_str_t *str)
{
    static int guid_initialized;
    static unsigned pid;
    static char str_pid[5];
    static unsigned char mac_addr[6];
    static char str_mac_addr[16];
    static unsigned clock_seq;

    PJ_CHECK_STACK();

    if (guid_initialized == 0) {
	pid = pj_getpid();
	init_mac_address(mac_addr);
	clock_seq = 0;

	sprintf(str_pid, "%04x", pid);
	sprintf(str_mac_addr, "%02x%02x%02x%02x%02x%02x",
	    mac_addr[0], mac_addr[1], mac_addr[2],
	    mac_addr[3], mac_addr[4], mac_addr[5]);

	guid_initialized = 1;
    }

    strcpy(str->ptr, str_pid);
    sprintf(str->ptr+4, "%04x", clock_seq++);
    pj_memcpy(str->ptr+8, str_mac_addr, 12);
    str->slen = 20;

    return str;
}

