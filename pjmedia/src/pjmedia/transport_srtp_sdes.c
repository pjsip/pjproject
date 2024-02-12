/*
 * Copyright (C) 2017 Teluu Inc. (http://www.teluu.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#if defined(PJ_HAS_SSL_SOCK) && (PJ_HAS_SSL_SOCK != 0)

/* Include OpenSSL libraries for MSVC */
#  ifdef _MSC_VER 
#    if (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_OPENSSL)
#      include <openssl/opensslv.h>
#      if OPENSSL_VERSION_NUMBER >= 0x10100000L
#        pragma comment(lib, "libcrypto")
#        pragma comment(lib, "libssl")
#        pragma comment(lib, "crypt32")
#      else
#        pragma comment(lib, "libeay32")
#        pragma comment(lib, "ssleay32")
#      endif
#    endif
#  endif
#endif


#include <pj/rand.h>


static pj_status_t sdes_media_create(pjmedia_transport *tp,
                                     pj_pool_t *sdp_pool,
                                     unsigned options,
                                     const pjmedia_sdp_session *sdp_remote,
                                     unsigned media_index);
static pj_status_t sdes_encode_sdp  (pjmedia_transport *tp,
                                     pj_pool_t *sdp_pool,
                                     pjmedia_sdp_session *sdp_local,
                                     const pjmedia_sdp_session *sdp_remote,
                                     unsigned media_index);
static pj_status_t sdes_media_start (pjmedia_transport *tp,
                                     pj_pool_t *pool,
                                     const pjmedia_sdp_session *sdp_local,
                                     const pjmedia_sdp_session *sdp_remote,
                                     unsigned media_index);
static pj_status_t sdes_media_stop  (pjmedia_transport *tp);


static pjmedia_transport_op sdes_op =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &sdes_media_create,
    &sdes_encode_sdp,
    &sdes_media_start,
    &sdes_media_stop,
    NULL,
    NULL
};


static pj_status_t sdes_create(transport_srtp *srtp,
                               pjmedia_transport **p_keying)
{
    pjmedia_transport *sdes;

    sdes = PJ_POOL_ZALLOC_T(srtp->pool, pjmedia_transport);
    pj_ansi_strxcpy(sdes->name, srtp->pool->obj_name, PJ_MAX_OBJ_NAME);
    pj_memcpy(sdes->name, "sdes", 4);
    sdes->type = (pjmedia_transport_type)PJMEDIA_SRTP_KEYING_SDES;
    sdes->op = &sdes_op;
    sdes->user_data = srtp;

    *p_keying = sdes;
    PJ_LOG(5,(srtp->pool->obj_name, "SRTP keying SDES created"));
    return PJ_SUCCESS;
}


/* Generate crypto attribute, including crypto key.
 * If crypto-suite chosen is crypto NULL, just return PJ_SUCCESS,
 * and set buffer_len = 0.
 */
static pj_status_t generate_crypto_attr_value(pj_pool_t *pool,
                                              char *buffer, int *buffer_len,
                                              pjmedia_srtp_crypto *crypto,
                                              int tag)
{
    pj_status_t status;
    int cs_idx = get_crypto_idx(&crypto->name);
    char b64_key[PJ_BASE256_TO_BASE64_LEN(MAX_KEY_LEN)+1];
    int b64_key_len = sizeof(b64_key);
    int print_len;

    if (cs_idx == -1)
        return PJMEDIA_SRTP_ENOTSUPCRYPTO;

    /* Crypto-suite NULL. */
    if (cs_idx == 0) {
        *buffer_len = 0;
        return PJ_SUCCESS;
    }

    /* Generate key if not specified. */
    if (crypto->key.slen == 0) {
        pj_bool_t key_ok;
        char key[MAX_KEY_LEN];
        unsigned i;

        PJ_ASSERT_RETURN(MAX_KEY_LEN >= crypto_suites[cs_idx].cipher_key_len,
                         PJ_ETOOSMALL);

        do {
#if defined(PJ_HAS_SSL_SOCK) && (PJ_HAS_SSL_SOCK != 0) && \
    (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_OPENSSL)
            int err = RAND_bytes((unsigned char*)key,
                                 crypto_suites[cs_idx].cipher_key_len);
            if (err != 1) {
                PJ_LOG(4,(THIS_FILE, "Failed generating random key "
                          "(native err=%d)", err));
                return PJMEDIA_ERRNO_FROM_LIBSRTP(1);
            }
#else
            PJ_LOG(3,(THIS_FILE, "Warning: simple random generator is used "
                                 "for generating SRTP key"));
            for (i=0; i<crypto_suites[cs_idx].cipher_key_len; ++i) {
                pj_timestamp ts;
                if (pj_rand() % 7 < 2)
                    pj_thread_sleep(pj_rand() % 11);
                pj_get_timestamp(&ts);
                key[i] = (char)((pj_rand() + ts.u32.lo) & 0xFF);
            }
#endif

            key_ok = PJ_TRUE;
            for (i=0; i<crypto_suites[cs_idx].cipher_key_len && key_ok; ++i)
                if (key[i] == 0) key_ok = PJ_FALSE;

        } while (!key_ok);
        crypto->key.ptr = (char*)
                          pj_pool_zalloc(pool,
                                         crypto_suites[cs_idx].cipher_key_len);
        pj_memcpy(crypto->key.ptr, key, crypto_suites[cs_idx].cipher_key_len);
        crypto->key.slen = crypto_suites[cs_idx].cipher_key_len;
    }

    if (crypto->key.slen != (pj_ssize_t)crypto_suites[cs_idx].cipher_key_len)
        return PJMEDIA_SRTP_EINKEYLEN;

    /* Key transmitted via SDP should be base64 encoded. */
    status = pj_base64_encode((pj_uint8_t*)crypto->key.ptr,
                              (int)crypto->key.slen,
                              b64_key, &b64_key_len);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4,(THIS_FILE, status,
                     "Failed encoding plain key to base64"));
        return status;
    }

    b64_key[b64_key_len] = '\0';

    PJ_ASSERT_RETURN(*buffer_len >= (crypto->name.slen + \
                     b64_key_len + 16), PJ_ETOOSMALL);

    /* Print the crypto attribute value. */
    print_len = pj_ansi_snprintf(buffer, *buffer_len, "%d %s inline:%s",
                                   tag,
                                   crypto_suites[cs_idx].name,
                                   b64_key);
    if (print_len < 1 || print_len >= *buffer_len)
        return PJ_ETOOSMALL;

    *buffer_len = print_len;

    return PJ_SUCCESS;
}


/* Parse crypto attribute line */
static pj_status_t parse_attr_crypto(pj_pool_t *pool,
                                     const pjmedia_sdp_attr *attr,
                                     pjmedia_srtp_crypto *crypto,
                                     int *tag)
{
    pj_str_t token, delim;
    pj_status_t status;
    int itmp;
    pj_ssize_t found_idx;

    pj_bzero(crypto, sizeof(*crypto));

    /* Tag */
    delim = pj_str(" ");
    found_idx = pj_strtok(&attr->value, &delim, &token, 0);
    if (found_idx == attr->value.slen) {
        PJ_LOG(4,(THIS_FILE, "Attribute crypto expecting tag"));
        return PJMEDIA_SDP_EINATTR;
    }

    /* Tag must not use leading zeroes. */
    if (token.slen > 1 && *token.ptr == '0')
        return PJMEDIA_SDP_EINATTR;

    /* Tag must be decimal, i.e: contains only digit '0'-'9'. */
    for (itmp = 0; itmp < token.slen; ++itmp)
        if (!pj_isdigit(token.ptr[itmp]))
            return PJMEDIA_SDP_EINATTR;

    /* Get tag value. */
    *tag = pj_strtoul(&token);

    /* Crypto-suite */
    found_idx = pj_strtok(&attr->value, &delim, &token, found_idx+token.slen);
    if (found_idx == attr->value.slen) {
        PJ_LOG(4,(THIS_FILE, "Attribute crypto expecting crypto suite"));
        return PJMEDIA_SDP_EINATTR;
    }
    pj_strdup(pool, &crypto->name, &token);

    /* Key method */
    delim = pj_str(": ");
    found_idx = pj_strtok(&attr->value, &delim, &token, found_idx+token.slen);
    if (found_idx == attr->value.slen) {
        PJ_LOG(4,(THIS_FILE, "Attribute crypto expecting key method"));
        return PJMEDIA_SDP_EINATTR;
    }
    if (pj_stricmp2(&token, "inline")) {
        PJ_LOG(4,(THIS_FILE, "Attribute crypto key method '%.*s' "
                  "not supported!", (int)token.slen, token.ptr));
        return PJMEDIA_SDP_EINATTR;
    }

    /* Key */    
    delim = pj_str("| ");
    found_idx = pj_strtok(&attr->value, &delim, &token, found_idx+token.slen);
    if (found_idx == attr->value.slen) {
        PJ_LOG(4,(THIS_FILE, "Attribute crypto expecting key"));
        return PJMEDIA_SDP_EINATTR;
    }
    
    if (PJ_BASE64_TO_BASE256_LEN(token.slen) > MAX_KEY_LEN) {
        PJ_LOG(4,(THIS_FILE, "Key too long"));
        return PJMEDIA_SRTP_EINKEYLEN;
    }

    /* Decode key */
    crypto->key.ptr = (char*) pj_pool_zalloc(pool, MAX_KEY_LEN);
    itmp = MAX_KEY_LEN;
    status = pj_base64_decode(&token, (pj_uint8_t*)crypto->key.ptr,
                              &itmp);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4,(THIS_FILE, status,
                     "Failed decoding crypto key from base64"));
        return status;
    }
    crypto->key.slen = itmp;

    return PJ_SUCCESS;
}


static pj_status_t sdes_media_create( pjmedia_transport *tp,
                                      pj_pool_t *sdp_pool,
                                      unsigned options,
                                      const pjmedia_sdp_session *sdp_remote,
                                      unsigned media_index)
{
    struct transport_srtp *srtp = (struct transport_srtp*)tp->user_data;
    pj_uint32_t rem_proto = 0;

    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(sdp_pool);

    /* Verify remote media transport, it has to be RTP/AVP or RTP/SAVP */
    if (!srtp->offerer_side) {
        pjmedia_sdp_media *m = sdp_remote->media[media_index];

        /* Get transport protocol and drop any RTCP-FB flag */
        rem_proto = pjmedia_sdp_transport_get_proto(&m->desc.transport);
        PJMEDIA_TP_PROTO_TRIM_FLAG(rem_proto, PJMEDIA_TP_PROFILE_RTCP_FB);
        if (rem_proto != PJMEDIA_TP_PROTO_RTP_AVP &&
            rem_proto != PJMEDIA_TP_PROTO_RTP_SAVP)
        {
            return PJMEDIA_SRTP_ESDPINTRANSPORT;
        }
    }

    /* Validations */
    if (srtp->offerer_side) {
        /* As offerer: do nothing. */
    } else {
        /* Validate remote media transport based on SRTP usage option. */
        switch (srtp->setting.use) {
            case PJMEDIA_SRTP_DISABLED:
                if (rem_proto == PJMEDIA_TP_PROTO_RTP_SAVP)
                    return PJMEDIA_SRTP_ESDPINTRANSPORT;
                break;
            case PJMEDIA_SRTP_OPTIONAL:
                break;
            case PJMEDIA_SRTP_MANDATORY:
                if (rem_proto != PJMEDIA_TP_PROTO_RTP_SAVP)
                    return PJMEDIA_SRTP_ESDPINTRANSPORT;
                break;
        }
    }

    return PJ_SUCCESS;
}

static pj_status_t sdes_encode_sdp( pjmedia_transport *tp,
                                    pj_pool_t *sdp_pool,
                                    pjmedia_sdp_session *sdp_local,
                                    const pjmedia_sdp_session *sdp_remote,
                                    unsigned media_index)
{
    struct transport_srtp *srtp = (struct transport_srtp*)tp->user_data;
    pjmedia_sdp_media *m_rem, *m_loc;
    enum { MAXLEN = 512 };
    char buffer[MAXLEN];
    int buffer_len;
    pj_status_t status;
    pjmedia_sdp_attr *attr;
    pj_str_t attr_value;
    unsigned i, j;

    m_rem = sdp_remote ? sdp_remote->media[media_index] : NULL;
    m_loc = sdp_local->media[media_index];

    /* Verify media transport, it has to be RTP/AVP or RTP/SAVP */
    {
        pjmedia_sdp_media *m = sdp_remote? m_rem : m_loc;
        pj_uint32_t proto = 0;

        /* Get transport protocol and drop any RTCP-FB flag */
        proto = pjmedia_sdp_transport_get_proto(&m->desc.transport);
        PJMEDIA_TP_PROTO_TRIM_FLAG(proto, PJMEDIA_TP_PROFILE_RTCP_FB);
        if (proto != PJMEDIA_TP_PROTO_RTP_AVP &&
            proto != PJMEDIA_TP_PROTO_RTP_SAVP)
        {
            return PJMEDIA_SRTP_ESDPINTRANSPORT;
        }
    }

    /* If the media is inactive, do nothing. */
    /* No, we still need to process SRTP offer/answer even if the media is
     * marked as inactive, because the transport is still alive in this
     * case (e.g. for keep-alive). See:
     *   https://github.com/pjsip/pjproject/issues/1079
     */
    /*
    if (pjmedia_sdp_media_find_attr(m_loc, &ID_INACTIVE, NULL) ||
        (m_rem && pjmedia_sdp_media_find_attr(m_rem, &ID_INACTIVE, NULL)))
        goto BYPASS_SRTP;
    */

    /* Check remote media transport & set local media transport
     * based on SRTP usage option.
     */
    if (srtp->offerer_side) {

        /* Generate transport */
        switch (srtp->setting.use) {
            case PJMEDIA_SRTP_DISABLED:
                /* Should never reach here */
                return PJ_SUCCESS;
            case PJMEDIA_SRTP_OPTIONAL:
                m_loc->desc.transport =
                                (srtp->peer_use == PJMEDIA_SRTP_MANDATORY)?
                                ID_RTP_SAVP : ID_RTP_AVP;
                break;
            case PJMEDIA_SRTP_MANDATORY:
                m_loc->desc.transport = ID_RTP_SAVP;
                break;
        }

        /* Generate crypto attribute if not yet */
        if (pjmedia_sdp_media_find_attr(m_loc, &ID_CRYPTO, NULL) == NULL) {
            int tag = 1;

            /* Offer only current active crypto if any, otherwise offer all
             * crypto-suites in the setting.
             */
            for (i=0; i<srtp->setting.crypto_count; ++i) {
                if (srtp->srtp_ctx.tx_policy.name.slen &&
                    pj_stricmp(&srtp->srtp_ctx.tx_policy.name,
                               &srtp->setting.crypto[i].name) != 0)
                {
                    continue;
                }

                buffer_len = MAXLEN;
                status = generate_crypto_attr_value(srtp->pool, buffer,
                                                    &buffer_len,
                                                    &srtp->setting.crypto[i],
                                                    tag);
                if (status != PJ_SUCCESS)
                    return status;

                /* If buffer_len==0, just skip the crypto attribute. */
                if (buffer_len) {
                    pj_strset(&attr_value, buffer, buffer_len);
                    attr = pjmedia_sdp_attr_create(srtp->pool, ID_CRYPTO.ptr,
                                                   &attr_value);
                    m_loc->attr[m_loc->attr_count++] = attr;
                    ++tag;
                }
            }
        }

    } else {
        /* Answerer side */
        pj_uint32_t rem_proto = 0;

        pj_assert(sdp_remote && m_rem);

        /* Get transport protocol and drop any RTCP-FB flag */
        rem_proto = pjmedia_sdp_transport_get_proto(&m_rem->desc.transport);
        PJMEDIA_TP_PROTO_TRIM_FLAG(rem_proto, PJMEDIA_TP_PROFILE_RTCP_FB);

        /* Generate transport */
        switch (srtp->setting.use) {
            case PJMEDIA_SRTP_DISABLED:
                /* Should never reach here */
                if (rem_proto == PJMEDIA_TP_PROTO_RTP_SAVP)
                    return PJMEDIA_SRTP_ESDPINTRANSPORT;
                return PJ_SUCCESS;
            case PJMEDIA_SRTP_OPTIONAL:
                break;
            case PJMEDIA_SRTP_MANDATORY:
                if (rem_proto != PJMEDIA_TP_PROTO_RTP_SAVP)
                    return PJMEDIA_SRTP_ESDPINTRANSPORT;
                break;
        }

        /* Generate crypto attribute if not yet */
        if (pjmedia_sdp_media_find_attr(m_loc, &ID_CRYPTO, NULL) == NULL) {

            pjmedia_srtp_crypto tmp_rx_crypto;
            pj_bool_t has_crypto_attr = PJ_FALSE;
            int matched_idx = -1;
            int chosen_tag = 0;
            int tags[64]; /* assume no more than 64 crypto attrs in a media */
            unsigned cr_attr_count = 0;

            /* Find supported crypto-suite, get the tag, and assign
             * policy_local.
             */
            for (i=0; i<m_rem->attr_count; ++i) {
                if (pj_stricmp(&m_rem->attr[i]->name, &ID_CRYPTO) != 0)
                    continue;

                has_crypto_attr = PJ_TRUE;

                status = parse_attr_crypto(srtp->pool, m_rem->attr[i],
                                           &tmp_rx_crypto,
                                           &tags[cr_attr_count]);
                if (status != PJ_SUCCESS)
                    return status;

                /* Check duplicated tag */
                for (j=0; j<cr_attr_count; ++j) {
                    if (tags[j] == tags[cr_attr_count]) {
                        //DEACTIVATE_MEDIA(sdp_pool, m_loc);
                        return PJMEDIA_SRTP_ESDPDUPCRYPTOTAG;
                    }
                }

                if (matched_idx == -1) {
                    /* lets see if the crypto-suite offered is supported */
                    for (j=0; j<srtp->setting.crypto_count; ++j)
                        if (pj_stricmp(&tmp_rx_crypto.name,
                                       &srtp->setting.crypto[j].name) == 0)
                        {
                            int cs_idx = get_crypto_idx(&tmp_rx_crypto.name);
                            
                            if (cs_idx == -1)
                                return PJMEDIA_SRTP_ENOTSUPCRYPTO;

                            if (tmp_rx_crypto.key.slen !=
                                (int)crypto_suites[cs_idx].cipher_key_len)
                                return PJMEDIA_SRTP_EINKEYLEN;

                            srtp->srtp_ctx.rx_policy_neg = tmp_rx_crypto;
                            chosen_tag = tags[cr_attr_count];
                            matched_idx = j;
                            break;
                        }
                }
                cr_attr_count++;
            }

            /* Check crypto negotiation result */
            switch (srtp->setting.use) {
                case PJMEDIA_SRTP_DISABLED:
                    /* Should never reach here */
                    break;

                case PJMEDIA_SRTP_OPTIONAL:
                    /* Bypass SDES if remote uses RTP/AVP and:
                     * - has no crypto-attr, or
                     * - has no matching crypto
                     */
                    if ((!has_crypto_attr || matched_idx == -1) &&
                        !PJMEDIA_TP_PROTO_HAS_FLAG(rem_proto,
                                                   PJMEDIA_TP_PROFILE_SRTP))
                    {
                        return PJ_SUCCESS;
                    }
                    break;

                case PJMEDIA_SRTP_MANDATORY:
                    /* Do nothing, intentional */
                    break;
            }

            /* No crypto attr */
            if (!has_crypto_attr) {
                //DEACTIVATE_MEDIA(sdp_pool, m_loc);
                return PJMEDIA_SRTP_ESDPREQCRYPTO;
            }

            /* No crypto match */
            if (matched_idx == -1) {
                //DEACTIVATE_MEDIA(sdp_pool, m_loc);
                return PJMEDIA_SRTP_ENOTSUPCRYPTO;
            }

            /* we have to generate crypto answer,
             * with srtp->srtp_ctx.tx_policy_neg matched the offer
             * and rem_tag contains matched offer tag.
             */
            buffer_len = MAXLEN;
            status = generate_crypto_attr_value(
                                        srtp->pool, buffer, &buffer_len,
                                        &srtp->setting.crypto[matched_idx],
                                        chosen_tag);
            if (status != PJ_SUCCESS)
                return status;

            srtp->srtp_ctx.tx_policy_neg = srtp->setting.crypto[matched_idx];

            /* If buffer_len==0, just skip the crypto attribute. */
            if (buffer_len) {
                pj_strset(&attr_value, buffer, buffer_len);
                attr = pjmedia_sdp_attr_create(sdp_pool, ID_CRYPTO.ptr,
                                               &attr_value);
                m_loc->attr[m_loc->attr_count++] = attr;
            }

            /* At this point, we get valid rx_policy_neg & tx_policy_neg. */
        }

        /* Update transport description in local media SDP */
        m_loc->desc.transport = m_rem->desc.transport;
    }

    return PJ_SUCCESS;
}


static pj_status_t fill_local_crypto(pj_pool_t *pool,
                                     const pjmedia_sdp_media *m_loc, 
                                     pjmedia_srtp_crypto loc_crypto[],
                                     int *count)
{
    int i;
    int crypto_count = 0;
    pj_status_t status = PJ_SUCCESS;
    
    for (i = 0; i < *count; ++i) {
        pj_bzero(&loc_crypto[i], sizeof(loc_crypto[i]));
    }

    for (i = 0; i < (int)m_loc->attr_count; ++i) {      
        pjmedia_srtp_crypto tmp_crypto;
        int loc_tag;

        if (pj_stricmp(&m_loc->attr[i]->name, &ID_CRYPTO) != 0)
            continue;

        status = parse_attr_crypto(pool, m_loc->attr[i],
                                   &tmp_crypto, &loc_tag);
        if (status != PJ_SUCCESS)
            return status;

        if (loc_tag <= 0 || loc_tag > *count)
            return PJMEDIA_SRTP_ESDPINCRYPTOTAG;

        loc_crypto[loc_tag-1] = tmp_crypto;
        ++crypto_count;
    }
    *count = crypto_count;
    return status;
}


static pj_status_t sdes_media_start( pjmedia_transport *tp,
                                     pj_pool_t *pool,
                                     const pjmedia_sdp_session *sdp_local,
                                     const pjmedia_sdp_session *sdp_remote,
                                     unsigned media_index)
{
    struct transport_srtp *srtp = (struct transport_srtp*)tp->user_data;
    pjmedia_sdp_media *m_rem, *m_loc;
    pj_status_t status;
    unsigned i;
    pjmedia_srtp_crypto loc_crypto[PJMEDIA_SRTP_MAX_CRYPTOS];
    int loc_cryto_cnt = PJMEDIA_SRTP_MAX_CRYPTOS;
    pjmedia_srtp_crypto tmp_tx_crypto;
    pj_bool_t has_crypto_attr = PJ_FALSE;
    int rem_tag;
    int j;


    m_rem = sdp_remote->media[media_index];
    m_loc = sdp_local->media[media_index];

    /* Verify media transport, it has to be RTP/AVP or RTP/SAVP */
    {
        pj_uint32_t rem_proto;

        /* Get transport protocol and drop any RTCP-FB flag */
        rem_proto = pjmedia_sdp_transport_get_proto(&m_rem->desc.transport);
        PJMEDIA_TP_PROTO_TRIM_FLAG(rem_proto, PJMEDIA_TP_PROFILE_RTCP_FB);
        if (rem_proto != PJMEDIA_TP_PROTO_RTP_AVP &&
            rem_proto != PJMEDIA_TP_PROTO_RTP_SAVP)
        {
            return PJMEDIA_SRTP_ESDPINTRANSPORT;
        }

        /* Also check if peer signal SRTP as mandatory */
        if (rem_proto == PJMEDIA_TP_PROTO_RTP_SAVP)
            srtp->peer_use = PJMEDIA_SRTP_MANDATORY;
        else
            srtp->peer_use = PJMEDIA_SRTP_OPTIONAL;
    }

    /* For answerer side, SRTP crypto policies have been populated in
     * media_encode_sdp(). Check if the key changes on the local SDP.
     */
    if (!srtp->offerer_side) {
        if (srtp->srtp_ctx.tx_policy_neg.name.slen == 0)
            return PJ_SUCCESS;

        /* Get the local crypto. */
        fill_local_crypto(srtp->pool, m_loc, loc_crypto, &loc_cryto_cnt);

        if (loc_cryto_cnt == 0)
            return PJ_SUCCESS;

        if ((pj_stricmp(&srtp->srtp_ctx.tx_policy_neg.name,
                        &loc_crypto[0].name) == 0) &&
            (pj_stricmp(&srtp->srtp_ctx.tx_policy_neg.key,
                        &loc_crypto[0].key) != 0))
        {
            srtp->srtp_ctx.tx_policy_neg = loc_crypto[0];
            for (i = 0; i<srtp->setting.crypto_count ;++i) {
                if ((pj_stricmp(&srtp->setting.crypto[i].name,
                                &loc_crypto[0].name) == 0) &&
                    (pj_stricmp(&srtp->setting.crypto[i].key,
                                 &loc_crypto[0].key) != 0))
                {
                    pj_strdup(pool, &srtp->setting.crypto[i].key,
                              &loc_crypto[0].key);
                }
            }
        }

        return PJ_SUCCESS;
    }

    /* Check remote media transport & set local media transport
     * based on SRTP usage option.
     */
    if (srtp->setting.use == PJMEDIA_SRTP_DISABLED) {
        if (pjmedia_sdp_media_find_attr(m_rem, &ID_CRYPTO, NULL)) {
            DEACTIVATE_MEDIA(pool, m_loc);
            return PJMEDIA_SRTP_ESDPINCRYPTO;
        }
        return PJ_SUCCESS;
    } else if (srtp->setting.use == PJMEDIA_SRTP_OPTIONAL) {
        // Regardless the answer's transport type (RTP/AVP or RTP/SAVP),
        // the answer must be processed through in optional mode.
        // Please note that at this point transport type is ensured to be
        // RTP/AVP or RTP/SAVP, see sdes_media_create()
        //if (pj_stricmp(&m_rem->desc.transport, &m_loc->desc.transport)) {
            //DEACTIVATE_MEDIA(pool, m_loc);
            //return PJMEDIA_SDP_EINPROTO;
        //}
        fill_local_crypto(srtp->pool, m_loc, loc_crypto, &loc_cryto_cnt);
    } else if (srtp->setting.use == PJMEDIA_SRTP_MANDATORY) {
        if (srtp->peer_use != PJMEDIA_SRTP_MANDATORY) {
            DEACTIVATE_MEDIA(pool, m_loc);
            return PJMEDIA_SDP_EINPROTO;
        }
        fill_local_crypto(srtp->pool, m_loc, loc_crypto, &loc_cryto_cnt);
    }

    /* find supported crypto-suite, get the tag, and assign policy_local */
    for (i=0; i<m_rem->attr_count; ++i) {
        if (pj_stricmp(&m_rem->attr[i]->name, &ID_CRYPTO) != 0)
            continue;

        /* more than one crypto attribute in media answer */
        if (has_crypto_attr) {
            DEACTIVATE_MEDIA(pool, m_loc);
            return PJMEDIA_SRTP_ESDPAMBIGUEANS;
        }

        has_crypto_attr = PJ_TRUE;

        status = parse_attr_crypto(srtp->pool, m_rem->attr[i],
                                   &tmp_tx_crypto, &rem_tag);
        if (status != PJ_SUCCESS)
            return status;


        /* Tag range check, our tags in the offer must be in the SRTP 
         * setting range, so does the remote answer's. The remote answer's 
         * tag must not exceed the tag range of the local offer.
         */
        if (rem_tag < 1 || rem_tag > (int)srtp->setting.crypto_count ||
            rem_tag > loc_cryto_cnt) 
        {
            DEACTIVATE_MEDIA(pool, m_loc);
            return PJMEDIA_SRTP_ESDPINCRYPTOTAG;
        }

        /* match the crypto name */
        if (pj_stricmp(&tmp_tx_crypto.name, &loc_crypto[rem_tag-1].name))
        {
            DEACTIVATE_MEDIA(pool, m_loc);
            return PJMEDIA_SRTP_ECRYPTONOTMATCH;
        }

        /* Find the crypto from the local crypto. */
        for (j = 0; j < (int)loc_cryto_cnt; ++j) {
            if (pj_stricmp(&tmp_tx_crypto.name,
                           &loc_crypto[j].name) == 0)
            {
                srtp->srtp_ctx.tx_policy_neg = loc_crypto[j];
                break;
            }
        }

        srtp->srtp_ctx.rx_policy_neg = tmp_tx_crypto;
    }

    if (srtp->setting.use == PJMEDIA_SRTP_DISABLED) {
        /* should never reach here */
        return PJ_SUCCESS;
    } else if (srtp->setting.use == PJMEDIA_SRTP_OPTIONAL) {
        if (!has_crypto_attr)
            return PJ_SUCCESS;
    } else if (srtp->setting.use == PJMEDIA_SRTP_MANDATORY) {
        if (!has_crypto_attr) {
            DEACTIVATE_MEDIA(pool, m_loc);
            return PJMEDIA_SRTP_ESDPREQCRYPTO;
        }
    }

    /* At this point, we get valid rx_policy_neg & tx_policy_neg. */

    return PJ_SUCCESS;
}

static pj_status_t sdes_media_stop(pjmedia_transport *tp)
{
    PJ_UNUSED_ARG(tp);
    return PJ_SUCCESS;
}
