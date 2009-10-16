/* $Id$ */
/* 
 * Copyright (C) 2009 Teluu Inc. (http://www.teluu.com)
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
#include <pj/ssl_sock.h>
#include <pj/errno.h>
#include <pj/string.h>

/* Cipher name structure */
typedef struct cipher_name_t {
    pj_ssl_cipher    cipher;
    const char	    *name;
} cipher_name_t;

/* Cipher name constants */
static cipher_name_t cipher_names[] =
{
    {TLS_NULL_WITH_NULL_NULL,               "NULL"},

    /* TLS/SSLv3 */
    {TLS_RSA_WITH_NULL_MD5,                 "TLS_RSA_WITH_NULL_MD5"},
    {TLS_RSA_WITH_NULL_SHA,                 "TLS_RSA_WITH_NULL_SHA"},
    {TLS_RSA_WITH_NULL_SHA256,              "TLS_RSA_WITH_NULL_SHA256"},
    {TLS_RSA_WITH_RC4_128_MD5,              "TLS_RSA_WITH_RC4_128_MD5"},
    {TLS_RSA_WITH_RC4_128_SHA,              "TLS_RSA_WITH_RC4_128_SHA"},
    {TLS_RSA_WITH_3DES_EDE_CBC_SHA,         "TLS_RSA_WITH_3DES_EDE_CBC_SHA"},
    {TLS_RSA_WITH_AES_128_CBC_SHA,          "TLS_RSA_WITH_AES_128_CBC_SHA"},
    {TLS_RSA_WITH_AES_256_CBC_SHA,          "TLS_RSA_WITH_AES_256_CBC_SHA"},
    {TLS_RSA_WITH_AES_128_CBC_SHA256,       "TLS_RSA_WITH_AES_128_CBC_SHA256"},
    {TLS_RSA_WITH_AES_256_CBC_SHA256,       "TLS_RSA_WITH_AES_256_CBC_SHA256"},
    {TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA,      "TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA"},
    {TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA,      "TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA"},
    {TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA,     "TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA"},
    {TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,     "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA"},
    {TLS_DH_DSS_WITH_AES_128_CBC_SHA,       "TLS_DH_DSS_WITH_AES_128_CBC_SHA"},
    {TLS_DH_RSA_WITH_AES_128_CBC_SHA,       "TLS_DH_RSA_WITH_AES_128_CBC_SHA"},
    {TLS_DHE_DSS_WITH_AES_128_CBC_SHA,      "TLS_DHE_DSS_WITH_AES_128_CBC_SHA"},
    {TLS_DHE_RSA_WITH_AES_128_CBC_SHA,      "TLS_DHE_RSA_WITH_AES_128_CBC_SHA"},
    {TLS_DH_DSS_WITH_AES_256_CBC_SHA,       "TLS_DH_DSS_WITH_AES_256_CBC_SHA"},
    {TLS_DH_RSA_WITH_AES_256_CBC_SHA,       "TLS_DH_RSA_WITH_AES_256_CBC_SHA"},
    {TLS_DHE_DSS_WITH_AES_256_CBC_SHA,      "TLS_DHE_DSS_WITH_AES_256_CBC_SHA"},
    {TLS_DHE_RSA_WITH_AES_256_CBC_SHA,      "TLS_DHE_RSA_WITH_AES_256_CBC_SHA"},
    {TLS_DH_DSS_WITH_AES_128_CBC_SHA256,    "TLS_DH_DSS_WITH_AES_128_CBC_SHA256"},
    {TLS_DH_RSA_WITH_AES_128_CBC_SHA256,    "TLS_DH_RSA_WITH_AES_128_CBC_SHA256"},
    {TLS_DHE_DSS_WITH_AES_128_CBC_SHA256,   "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256"},
    {TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,   "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256"},
    {TLS_DH_DSS_WITH_AES_256_CBC_SHA256,    "TLS_DH_DSS_WITH_AES_256_CBC_SHA256"},
    {TLS_DH_RSA_WITH_AES_256_CBC_SHA256,    "TLS_DH_RSA_WITH_AES_256_CBC_SHA256"},
    {TLS_DHE_DSS_WITH_AES_256_CBC_SHA256,   "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256"},
    {TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,   "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256"},
    {TLS_DH_anon_WITH_RC4_128_MD5,          "TLS_DH_anon_WITH_RC4_128_MD5"},
    {TLS_DH_anon_WITH_3DES_EDE_CBC_SHA,     "TLS_DH_anon_WITH_3DES_EDE_CBC_SHA"},
    {TLS_DH_anon_WITH_AES_128_CBC_SHA,      "TLS_DH_anon_WITH_AES_128_CBC_SHA"},
    {TLS_DH_anon_WITH_AES_256_CBC_SHA,      "TLS_DH_anon_WITH_AES_256_CBC_SHA"},
    {TLS_DH_anon_WITH_AES_128_CBC_SHA256,   "TLS_DH_anon_WITH_AES_128_CBC_SHA256"},
    {TLS_DH_anon_WITH_AES_256_CBC_SHA256,   "TLS_DH_anon_WITH_AES_256_CBC_SHA256"},

    /* TLS (deprecated) */
    {TLS_RSA_EXPORT_WITH_RC4_40_MD5,        "TLS_RSA_EXPORT_WITH_RC4_40_MD5"},
    {TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5,    "TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5"},
    {TLS_RSA_WITH_IDEA_CBC_SHA,             "TLS_RSA_WITH_IDEA_CBC_SHA"},
    {TLS_RSA_EXPORT_WITH_DES40_CBC_SHA,     "TLS_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {TLS_RSA_WITH_DES_CBC_SHA,              "TLS_RSA_WITH_DES_CBC_SHA"},
    {TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,  "TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA"},
    {TLS_DH_DSS_WITH_DES_CBC_SHA,           "TLS_DH_DSS_WITH_DES_CBC_SHA"},
    {TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,  "TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {TLS_DH_RSA_WITH_DES_CBC_SHA,           "TLS_DH_RSA_WITH_DES_CBC_SHA"},
    {TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA, "TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA"},
    {TLS_DHE_DSS_WITH_DES_CBC_SHA,          "TLS_DHE_DSS_WITH_DES_CBC_SHA"},
    {TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA, "TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {TLS_DHE_RSA_WITH_DES_CBC_SHA,          "TLS_DHE_RSA_WITH_DES_CBC_SHA"},
    {TLS_DH_anon_EXPORT_WITH_RC4_40_MD5,    "TLS_DH_anon_EXPORT_WITH_RC4_40_MD5"},
    {TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA, "TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA"},
    {TLS_DH_anon_WITH_DES_CBC_SHA,          "TLS_DH_anon_WITH_DES_CBC_SHA"},

    /* SSLv3 */
    {SSL_FORTEZZA_KEA_WITH_NULL_SHA,        "SSL_FORTEZZA_KEA_WITH_NULL_SHA"},
    {SSL_FORTEZZA_KEA_WITH_FORTEZZA_CBC_SHA,"SSL_FORTEZZA_KEA_WITH_FORTEZZA_CBC_SHA"},
    {SSL_FORTEZZA_KEA_WITH_RC4_128_SHA,     "SSL_FORTEZZA_KEA_WITH_RC4_128_SHA"},

    /* SSLv2 */
    {SSL_CK_RC4_128_WITH_MD5,               "SSL_CK_RC4_128_WITH_MD5"},
    {SSL_CK_RC4_128_EXPORT40_WITH_MD5,      "SSL_CK_RC4_128_EXPORT40_WITH_MD5"},
    {SSL_CK_RC2_128_CBC_WITH_MD5,           "SSL_CK_RC2_128_CBC_WITH_MD5"},
    {SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5,  "SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5"},
    {SSL_CK_IDEA_128_CBC_WITH_MD5,          "SSL_CK_IDEA_128_CBC_WITH_MD5"},
    {SSL_CK_DES_64_CBC_WITH_MD5,            "SSL_CK_DES_64_CBC_WITH_MD5"},
    {SSL_CK_DES_192_EDE3_CBC_WITH_MD5,      "SSL_CK_DES_192_EDE3_CBC_WITH_MD5"}
};


/*
 * Initialize the SSL socket configuration with the default values.
 */
PJ_DEF(void) pj_ssl_sock_param_default(pj_ssl_sock_param *param)
{
    pj_bzero(param, sizeof(*param));

    /* Socket config */
    param->sock_af = PJ_AF_INET;
    param->sock_type = pj_SOCK_STREAM();
    param->async_cnt = 1;
    param->concurrency = -1;
    param->whole_data = PJ_TRUE;
    param->send_buffer_size = 8192;
#if !defined(PJ_SYMBIAN) || PJ_SYMBIAN==0
    param->read_buffer_size = 1500;
#endif

    /* Security config */
    param->proto = PJ_SSL_SOCK_PROTO_DEFAULT;
}


PJ_DEF(const char*) pj_ssl_cipher_name(pj_ssl_cipher cipher)
{
    unsigned i, n;

    n = PJ_ARRAY_SIZE(cipher_names);
    for (i = 0; i < n; ++i) {
	if (cipher == cipher_names[i].cipher)
	    return cipher_names[i].name;
    }

    return NULL;
}
