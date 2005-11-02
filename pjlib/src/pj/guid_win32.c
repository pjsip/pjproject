/* $Id$
 */
#include <pj/guid.h>
#include <pj/string.h>
#include <pj/sock.h>
#include <windows.h>
#include <objbase.h>
#include <pj/os.h>


const unsigned PJ_GUID_STRING_LENGTH=32;

PJ_INLINE(void) hex2digit(unsigned value, char *p)
{
    static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
			 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    *p++ = hex[ (value & 0xF0) >> 4 ];
    *p++ = hex[ (value & 0x0F) ];
}

static void guid_to_str( const GUID *guid, pj_str_t *str )
{
    unsigned i;
    GUID guid_copy;
    const unsigned char *src = (const unsigned char*)&guid_copy;
    char *dst = str->ptr;

    pj_memcpy(&guid_copy, guid, sizeof(*guid));
    guid_copy.Data1 = pj_ntohl(guid_copy.Data1);
    guid_copy.Data2 = pj_ntohs(guid_copy.Data2);
    guid_copy.Data3 = pj_ntohs(guid_copy.Data3);

    for (i=0; i<16; ++i) {
	hex2digit( *src, dst );
	dst += 2;
	++src;
    }
    str->slen = 32;
}


PJ_DEF(pj_str_t*) pj_generate_unique_string(pj_str_t *str)
{
    GUID guid;

    PJ_CHECK_STACK();

    CoCreateGuid(&guid);
    guid_to_str( &guid, str );
    return str;
}

