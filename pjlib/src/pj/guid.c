/* $Header: /pjproject/pjlib/src/pj/guid.c 9     6/14/05 2:15p Bennylp $ */
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
#include <pj/guid.h>
#include <pj/string.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/os.h>
#include <time.h>
#include <stdlib.h>

/*
 * The algorithm presented here needs serious checking about it's correctness,
 * especially the endian thingy.
 */

#define PJ_GUID_COCREATEGUID   1
#define PJ_GUID_OWNGUID	    2
#define PJ_GUID_SIMPLE	    3

#ifndef PJ_GUID_TYPE
# define PJ_GUID_TYPE	    PJ_GUID_SIMPLE
#endif

#define MHZ_IN_HZ	    1000000.0
#define SECOND_IN_MICRO	    1000000
#define SECOND_IN_100_NANO  10000000

static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
		     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

PJ_INLINE(void) pj_val_to_hex_digit(unsigned value, char *p)
{
    *p++ = hex[ (value & 0xF0) >> 4 ];
    *p++ = hex[ (value & 0x0F) ];
}

#if PJ_GUID_TYPE != PJ_GUID_COCREATEGUID
static int guid_initialized;
static unsigned char mac_addr[6];
static pj_uint16_t   clock_seq;
#endif

#ifdef _WIN32
#include <windows.h>

#if PJ_GUID_TYPE != PJ_GUID_COCREATEGUID
#include <wincon.h>

static void init_mac_address()
{
    /* Reference:
       http://support.microsoft.com/support/kb/articles/q118/6/23.asp
     */
    typedef struct _ASTAT_
    {
	ADAPTER_STATUS adapt;
	NAME_BUFFER    NameBuff [30];
    } ASTAT, * PASTAT;
    
    ASTAT	Adapter;
    NCB		Ncb;
    UCHAR	uRetCode;
    LANA_ENUM	lenum;
    
    memset( &Ncb, 0, sizeof(Ncb) );
    Ncb.ncb_command = NCBENUM;
    Ncb.ncb_buffer = (UCHAR *)&lenum;
    Ncb.ncb_length = sizeof(lenum);
    uRetCode = Netbios( &Ncb );
    
    if (uRetCode == 0 && lenum.length > 0) {
	memset( &Ncb, 0, sizeof(Ncb) );
	Ncb.ncb_command = NCBRESET;
	Ncb.ncb_lana_num = lenum.lana[0];
	
	uRetCode = Netbios( &Ncb );
	if ( uRetCode == 0 )
	{
	    memset( &Ncb, 0, sizeof (Ncb) );
	    Ncb.ncb_command = NCBASTAT;
	    Ncb.ncb_lana_num = lenum.lana[0];
	    
	    strcpy( (char*)Ncb.ncb_callname,  "*               " );
	    Ncb.ncb_buffer = (unsigned char *) &Adapter;
	    Ncb.ncb_length = sizeof(Adapter);
	    
	    uRetCode = Netbios( &Ncb );
	    if ( uRetCode == 0 )
	    {
		PJ_LOG( 5, ("guid.c", "MAC address on LANA %d is: "
				      "%02x-%02x-%02x-%02x-%02x-%02x",
		    lenum.lana[0],
		    Adapter.adapt.adapter_address[0],
		    Adapter.adapt.adapter_address[1],
		    Adapter.adapt.adapter_address[2],
		    Adapter.adapt.adapter_address[3],
		    Adapter.adapt.adapter_address[4],
		    Adapter.adapt.adapter_address[5] ));
		memcpy(mac_addr, Adapter.adapt.adapter_address, 6);
	    }
	}
    } else {
	uRetCode = 1;
    }
    
    if (uRetCode != 0) {
	unsigned my_mac_addr[6] = { 0x00, 0x02, 0xE3, 0x3B, 0x34, 0x20 };
	memcpy(mac_addr, my_mac_addr, 6);

	PJ_LOG( 3, ("guid.c", "Unable to get MAC address (retcode=0x%x). "
			      "Using my MAC address, but this won't be unique..",
		   uRetCode ));
    }
}
#endif	/* PJ_GUID_TYPE == PJGUID_COCREATEGUID */

#if PJ_GUID_TYPE==PJ_GUID_OWNGUID

static pj_uint32_t get_processor_cycle()
{
    unsigned cycle;

    __asm {
	RDTSC
	MOV DWORD PTR cycle, EAX
    }

    return cycle;
}

static pj_uint32_t get_100nsec_fraction()
{
    static pj_uint32_t cpu_mhz;
    double cycle;

    if (cpu_mhz == 0) {
	HKEY hkey;
	DWORD type, cbdata = sizeof(cpu_mhz);
	DWORD err = ERROR_SUCCESS;

	/* This doesn't seem to work on Win98 SE (presumably 98/95 as well */

	err = RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
			   "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
			   0, KEY_READ, &hkey);

	if (err == ERROR_SUCCESS) {
	    err = RegQueryValueEx( hkey, "~MHz", NULL, &type, (unsigned char*)&cpu_mhz, &cbdata);
	    RegCloseKey(hkey);
	}

	if (err != ERROR_SUCCESS) {
	    cpu_mhz = 500;
	    PJ_LOG(3, ("guid.c", "Unable to determine CPU speed. Using %u Mhz..", cpu_mhz));
	}
    }

    cycle = get_processor_cycle();
    return ((unsigned) (cycle * 10.0 / cpu_mhz)) % SECOND_IN_100_NANO;
}

/* Get the timestamp as a 60 bit value multiplexed by 4 bit version number.
 * For UUID version 1, the timestamp is represented by Coordinated Universal 
 * Time (UTC) as a count of 100-nanosecond intervals since 00:00:00.00, 
 * 15 October 1582 (the date of Gregorian reform to the Christian calendar).
 *
 * Note about this implementation:
 *  - we start the time from 1 Jan 1970 instead (easier).
 */
static void get_timestamp_and_version( unsigned char timestamp[8] )
{
    pj_uint64_t *t = (pj_uint64_t*) timestamp;

    (*t) = time(NULL);
    (*t) *= SECOND_IN_100_NANO;
    (*t) += get_100nsec_fraction();

    /* don't care about version.. */
}
#endif	/* PJ_GUID_TYPE==PJ_GUID_OWNGUID */


#endif	/* _WIN32 */

#ifdef LINUX
static void init_mac_address()
{
    unsigned long *ulval1 = (unsigned long*) &mac_addr[0];
    unsigned short *usval1 = (unsigned short*) &mac_addr[4];

    *ulval1 = pj_rand();
    *usval1 = (unsigned short) pj_rand();
}

#endif

#if PJ_GUID_TYPE==PJ_GUID_OWNGUID
unsigned PJ_GUID_LENGTH=32;
PJ_DEF(pj_str_t*) pj_generate_unique_string(pj_str_t *str)
{
    unsigned char guid[16];
    int i;
    const unsigned char *src = guid;
    char *dst = str->ptr;

    if (guid_initialized == 0) {
	init_mac_address();
	clock_seq = (pj_uint16_t) pj_rand();
	guid_initialized = 1;
    }

    get_timestamp_and_version( &guid[0] );
    memcpy( &guid[8], &clock_seq, 2);
    memcpy( &guid[10], mac_addr, 6);
    ++clock_seq;

    for (i=0; i<16; ++i) {
	pj_val_to_hex_digit( *src, dst );
	dst += 2;
	++src;
    }
    str->slen = 32;

    return str;
}
#endif

#if PJ_GUID_TYPE==PJ_GUID_COCREATEGUID
unsigned PJ_GUID_LENGTH=32;
static void pj_guid_to_str( const GUID *guid, pj_str_t *str )
{
    unsigned i;
    GUID guid_copy;
    const unsigned char *src = (const unsigned char*)&guid_copy;
    char *dst = str->ptr;

    memcpy(&guid_copy, guid, sizeof(*guid));
    guid_copy.Data1 = ntohl(guid_copy.Data1);
    guid_copy.Data2 = ntohs(guid_copy.Data2);
    guid_copy.Data3 = ntohs(guid_copy.Data3);

    for (i=0; i<16; ++i) {
	pj_val_to_hex_digit( *src, dst );
	dst += 2;
	++src;
    }
    str->slen = 32;
}


PJ_DEF(pj_str_t*) pj_generate_unique_string(pj_str_t *str)
{
    GUID guid;
    CoCreateGuid(&guid);
    pj_guid_to_str( &guid, str );
    return str;
}
#endif

#if PJ_GUID_TYPE==PJ_GUID_SIMPLE
unsigned PJ_GUID_LENGTH=20;
PJ_DEF(pj_str_t*) pj_generate_unique_string(pj_str_t *str)
{
    static unsigned pid;
    static char str_pid[5];
    static char str_mac_addr[16];

    if (guid_initialized == 0) {
	pid = pj_getpid();
	init_mac_address();
	clock_seq = 0;

	sprintf(str_pid, "%04x", pid);
	sprintf(str_mac_addr, "%02x%02x%02x%02x%02x%02x",
	    mac_addr[0], mac_addr[1], mac_addr[2],
	    mac_addr[3], mac_addr[4], mac_addr[5]);

	guid_initialized = 1;
    }

    strcpy(str->ptr, str_pid);
    sprintf(str->ptr+4, "%04x", clock_seq++);
    memcpy(str->ptr+8, str_mac_addr, 12);
    str->slen = 20;

    return str;
}
#endif

PJ_DEF(void) pj_create_unique_string(pj_pool_t *pool, pj_str_t *str)
{
    str->ptr = pj_pool_alloc(pool, PJ_GUID_LENGTH);
    pj_generate_unique_string(str);
}
