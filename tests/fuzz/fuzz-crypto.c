/* 
 * Copyright (C) 2023 Teluu Inc. (http://www.teluu.com)
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>
#include <pjlib-util.h>

#define OPENSSL_SUPPRESS_DEPRECATED 1
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <zlib.h>

#define kMinInputLength 10
#define kMaxInputLength 1024

#define MAXSIZE 5120

void encode_base64_differential(const uint8_t *Data, size_t Size) {

    //PJSIP
    char pj_output[MAXSIZE];
    int  pj_output_len = MAXSIZE;

    memset(pj_output, 0, MAXSIZE);
    pj_base64_encode(Data, Size, pj_output, &pj_output_len);

    //OPENSSL
    BIO *bio, *bio_mem;
    char *ssl_output;
    int  ssl_output_len;

    bio = BIO_new(BIO_f_base64());
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    bio_mem = BIO_new(BIO_s_mem());
    BIO_push(bio, bio_mem);

    BIO_write(bio, Data, Size);
    BIO_flush(bio);

    ssl_output_len = BIO_get_mem_data(bio_mem, &ssl_output);

    //Differential
    int result = memcmp(pj_output, ssl_output, ssl_output_len);
    if(result != 0){
        abort();
    }
    BIO_free_all(bio);

    //PJSIP Decode After encode.
    pj_str_t pj_input;
    uint8_t pj_output_dec[MAXSIZE];
    int     pj_output_dec_len = MAXSIZE;

    pj_input.ptr = pj_output;
    pj_input.slen = ssl_output_len;

    memset(pj_output_dec, 0, MAXSIZE);
    pj_base64_decode(&pj_input, pj_output_dec, &pj_output_dec_len);

    //Differential
    int result_dec = memcmp(pj_output_dec, Data, Size);
    if(result_dec != 0) {
        abort();
    }
}

void decode_base64_differential(const uint8_t *Data, size_t Size) {

    //PJSIP
    pj_str_t pj_input;
    uint8_t pj_output[MAXSIZE];
    int  pj_output_len = MAXSIZE;

    pj_input.ptr = (char *)Data;
    pj_input.slen = Size;

    memset(pj_output, 0, MAXSIZE);
    pj_base64_decode(&pj_input, pj_output, &pj_output_len);
}

void md5_differential(const uint8_t *Data, size_t Size) {

    //PJSIP
    pj_md5_context ctx;
    pj_uint8_t pj_md5_hash[MD5_DIGEST_LENGTH];

    pj_md5_init(&ctx);
    pj_md5_update(&ctx, Data,Size);
    pj_md5_final(&ctx, pj_md5_hash);

    //OPENSSL
    uint8_t ssl_md5_hash[MD5_DIGEST_LENGTH] = {};
    MD5(Data, Size, ssl_md5_hash);

    //Differential
    int result = memcmp(pj_md5_hash, ssl_md5_hash, MD5_DIGEST_LENGTH);
    if(result != 0){
        abort();
    }
}

void sha1_differential(const uint8_t *Data, size_t Size) {

    //PJSIP
    pj_sha1_context pj_sha;
    pj_uint8_t pj_sha_hash[SHA_DIGEST_LENGTH];

    pj_sha1_init(&pj_sha);
    pj_sha1_update(&pj_sha,Data,Size);
    pj_sha1_final(&pj_sha,pj_sha_hash);

    //OPENSSL
    uint8_t ssl_sha_hash[SHA_DIGEST_LENGTH] = {};

    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx,Data,Size);
    SHA1_Final(ssl_sha_hash, &ctx);

    //Differential
    int result = memcmp(pj_sha_hash, ssl_sha_hash, SHA_DIGEST_LENGTH);
    if(result != 0){
        abort();
    }
}

void crc32_differential(const uint8_t *Data, size_t Size) {

    //PJSIP
    pj_uint32_t pj_crc;
    pj_crc = pj_crc32_calc(Data, Size);

    //zlib
    uint32_t    zlib_crc;
    zlib_crc = crc32(0L, Data, Size);

    //Differential
    if (pj_crc != zlib_crc) {
        abort();
    }
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    encode_base64_differential(Data, Size);
    decode_base64_differential(Data, Size);
    md5_differential(Data, Size);
    sha1_differential(Data, Size);
    crc32_differential(Data, Size);

    return 0;
}
