/* $Id$
 *
 */
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/ctype.h>
#include <pj/rand.h>
#include <pj/os.h>

#if PJ_FUNCTIONS_ARE_INLINED==0
#  include <pj/string_i.h>
#endif


static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
		     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

PJ_DEF(pj_str_t*) pj_strltrim( pj_str_t *str )
{
    register char *p = str->ptr;
    while (pj_isspace(*p))
	++p;
    str->slen -= (p - str->ptr);
    str->ptr = p;
    return str;
}

PJ_DEF(pj_str_t*) pj_strrtrim( pj_str_t *str )
{
    char *end = str->ptr + str->slen;
    register char *p = end - 1;
    while (p >= str->ptr && pj_isspace(*p))
        --p;
    str->slen -= ((end - p) - 1);
    return str;
}

PJ_INLINE(void) pj_val_to_hex_digit(unsigned value, char *p)
{
    *p++ = hex[ (value & 0xF0) >> 4 ];
    *p++ = hex[ (value & 0x0F) ];
}

PJ_DEF(char*) pj_create_random_string(char *str, pj_size_t len)
{
    unsigned i;
    char *p = str;

    PJ_CHECK_STACK();

    for (i=0; i<len/8; ++i) {
	unsigned val = pj_rand();
	pj_val_to_hex_digit( (val & 0xFF000000) >> 24, p+0 );
	pj_val_to_hex_digit( (val & 0x00FF0000) >> 16, p+2 );
	pj_val_to_hex_digit( (val & 0x0000FF00) >>  8, p+4 );
	pj_val_to_hex_digit( (val & 0x000000FF) >>  0, p+6 );
	p += 8;
    }
    for (i=i * 8; i<len; ++i) {
	*p++ = hex[ pj_rand() & 0x0F ];
    }
    return str;
}


PJ_DEF(unsigned long) pj_strtoul(const pj_str_t *str)
{
    unsigned long value;
    unsigned i;

    PJ_CHECK_STACK();

    value = 0;
    for (i=0; i<(unsigned)str->slen; ++i) {
	value = value * 10 + (str->ptr[i] - '0');
    }
    return value;
}

PJ_DEF(int) pj_utoa(unsigned long val, char *buf)
{
    return pj_utoa_pad(val, buf, 0, 0);
}

PJ_DEF(int) pj_utoa_pad( unsigned long val, char *buf, int min_dig, int pad)
{
    char *p;
    int len;

    PJ_CHECK_STACK();

    p = buf;
    do {
        unsigned long digval = (unsigned long) (val % 10);
        val /= 10;
        *p++ = (char) (digval + '0');
    } while (val > 0);

    len = p-buf;
    while (len < min_dig) {
	*p++ = (char)pad;
	++len;
    }
    *p-- = '\0';

    do {
        char temp = *p;
        *p = *buf;
        *buf = temp;
        --p;
        ++buf;
    } while (buf < p);

    return len;
}

