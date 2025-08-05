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
#include <pjlib-util/base64.h>
#include <pj/assert.h>
#include <pj/errno.h>

#define INV         -1
#define PADDING     '='

static const char base64_char[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '+', '/' 
};

/* Base 64 Encoding with URL and Filename Safe Alphabet (RFC 4648 section 5) */
static const char base64url_char[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '-', '_' 
};

static int base256_char(char c, pj_bool_t url)
{
    if (c >= 'A' && c <= 'Z')
        return (c - 'A');
    else if (c >= 'a' && c <= 'z')
        return (c - 'a' + 26);
    else if (c >= '0' && c <= '9')
        return (c - '0' + 52);
    else if ((!url && c == '+') || (url && c == '-'))
        return (62);
    else if ((!url && c == '/') || (url && c == '_'))
        return (63);
    else {
        /* It *may* happen on bad input, so this is not a good idea.
         * pj_assert(!"Should not happen as '=' should have been filtered");
         */
        return INV;
    }
}


static void base256to64(pj_uint8_t c1, pj_uint8_t c2, pj_uint8_t c3, 
                        int padding, char *output, pj_bool_t url)
{
    const char *b64 = url ? base64url_char : base64_char;
    *output++ = b64[c1>>2];
    *output++ = b64[((c1 & 0x3)<< 4) | ((c2 & 0xF0) >> 4)];
    switch (padding) {
    case 0:
        *output++ = b64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
        *output = b64[c3 & 0x3F];
        break;
    case 1:
        *output++ = b64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
        *output = PADDING;
        break;
    case 2:
    default:
        *output++ = PADDING;
        *output = PADDING;
        break;
    }
}

static pj_status_t b64_encode(const pj_uint8_t *input, int in_len,
                              char *output, int *out_len,
                              pj_bool_t url)
{
    const pj_uint8_t *pi = input;
    pj_uint8_t c1, c2, c3;
    int i = 0;
    char *po = output;

    PJ_ASSERT_RETURN(input && output && out_len, PJ_EINVAL);
    PJ_ASSERT_RETURN(*out_len >= PJ_BASE256_TO_BASE64_LEN(in_len), 
                     PJ_ETOOSMALL);

    while (i < in_len) {
        c1 = *pi++;
        ++i;

        if (i == in_len) {
            base256to64(c1, 0, 0, 2, po, url);
            po += 4;
            break;
        } else {
            c2 = *pi++;
            ++i;

            if (i == in_len) {
                base256to64(c1, c2, 0, 1, po, url);
                po += 4;
                break;
            } else {
                c3 = *pi++;
                ++i;
                base256to64(c1, c2, c3, 0, po, url);
            }
        }

        po += 4;
    }

    *out_len = (int)(po - output);
    return PJ_SUCCESS;
}

static pj_status_t b64_decode(const pj_str_t *input,
                              pj_uint8_t *out, int *out_len,
                              pj_bool_t url)
{
    const char *buf;
    int len;
    int i, j, k;
    int c[4];

    PJ_ASSERT_RETURN(input && out && out_len, PJ_EINVAL);

    buf = input->ptr;
    len = (int)input->slen;
    while (len && buf[len-1] == '=')
        --len;

    PJ_ASSERT_RETURN(*out_len >= PJ_BASE64_TO_BASE256_LEN(len), 
                     PJ_ETOOSMALL);

    for (i=0, j=0; i<len; ) {
        /* Fill up c, silently ignoring invalid characters */
        for (k=0; k<4 && i<len; ++k) {
            do {
                c[k] = base256_char(buf[i++], url);
            } while (c[k]==INV && i<len);
        }

        if (k<4) {
            if (k > 1) {
                out[j++] = (pj_uint8_t)((c[0]<<2) | ((c[1] & 0x30)>>4));
                if (k > 2) {
                    out[j++] = (pj_uint8_t)
                               (((c[1] & 0x0F)<<4) | ((c[2] & 0x3C)>>2));
                }
            }
            break;
        }

        out[j++] = (pj_uint8_t)((c[0]<<2) | ((c[1] & 0x30)>>4));
        out[j++] = (pj_uint8_t)(((c[1] & 0x0F)<<4) | ((c[2] & 0x3C)>>2));
        out[j++] = (pj_uint8_t)(((c[2] & 0x03)<<6) | (c[3] & 0x3F));
    }

    pj_assert(j <= *out_len);
    *out_len = j;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_base64_encode(const pj_uint8_t *input, int in_len,
                                     char *output, int *out_len)
{
    return b64_encode(input, in_len, output, out_len, PJ_FALSE);
}

PJ_DEF(pj_status_t) pj_base64url_encode(const pj_uint8_t *input, int in_len,
                                        char *output, int *out_len)
{
    return b64_encode(input, in_len, output, out_len, PJ_TRUE);
}

PJ_DEF(pj_status_t) pj_base64_decode(const pj_str_t *input,
                                     pj_uint8_t *out, int *out_len)
{
    return b64_decode(input, out, out_len, PJ_FALSE);
}

PJ_DEF(pj_status_t) pj_base64url_decode(const pj_str_t *input,
                                        pj_uint8_t *out, int *out_len)
{
    return b64_decode(input, out, out_len, PJ_TRUE);
}
