/*
 * Copyright (C)2024 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-codec/lyra.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pj/log.h>

#if defined(PJMEDIA_HAS_LYRA_CODEC) && PJMEDIA_HAS_LYRA_CODEC != 0

#ifdef _MSC_VER
#    pragma warning(disable: 4117)    // Reserved macro
#    pragma warning(disable: 4244)    // Possible loss of data
#    pragma warning(disable: 4100)    // Possible loss of data
#endif

#include "lyra_encoder.h"
#include "lyra_decoder.h"

#define THIS_FILE       "lyra.cpp"

// Available bitrate : 3200, 6000, 9200
#define LYRA_MAX_BITRATE        9200
#define LYRA_DEFAULT_VAD        1
#define LYRA_MAX_PATH_LEN       64
#define LYRA_DEFAULT_PATH       "model_coeffs"

static char LYRA_STR[] = "lyra";
static char BIT_RATE_STR[] = "bitrate";

using namespace chromemedia::codec;

/* Prototypes for lyra factory */
static pj_status_t lyra_test_alloc(pjmedia_codec_factory *factory,
                                   const pjmedia_codec_info *id);
static pj_status_t lyra_default_attr(pjmedia_codec_factory *factory,
                                     const pjmedia_codec_info* id,
                                     pjmedia_codec_param *attr);
static pj_status_t lyra_enum_codecs(pjmedia_codec_factory *factory,
                                    unsigned *count,
                                    pjmedia_codec_info codecs[]);
static pj_status_t lyra_alloc_codec(pjmedia_codec_factory *factory,
                                    const pjmedia_codec_info *id,
                                    pjmedia_codec **p_codec);
static pj_status_t lyra_dealloc_codec(pjmedia_codec_factory *factory,
                                      pjmedia_codec *codec);

/* Prototypes for lyra implementation. */
static pj_status_t lyra_codec_init(pjmedia_codec *codec,
                                   pj_pool_t *pool);
static pj_status_t lyra_codec_open(pjmedia_codec *codec,
                                   pjmedia_codec_param *attr);
static pj_status_t lyra_codec_close(pjmedia_codec *codec);
static pj_status_t lyra_codec_modify(pjmedia_codec *codec,
                                     const pjmedia_codec_param *attr);
static pj_status_t lyra_codec_parse(pjmedia_codec *codec,
                                    void *pkt,
                                    pj_size_t pkt_size,
                                    const pj_timestamp *ts,
                                    unsigned *frame_cnt,
                                    pjmedia_frame frames[]);
static pj_status_t lyra_codec_encode(pjmedia_codec *codec,
                                     const struct pjmedia_frame *input,
                                     unsigned output_buf_len,
                                     struct pjmedia_frame *output);
static pj_status_t lyra_codec_decode(pjmedia_codec *codec,
                                     const struct pjmedia_frame *input,
                                     unsigned output_buf_len,
                                     struct pjmedia_frame *output);
static pj_status_t lyra_codec_recover(pjmedia_codec *codec,
                                      unsigned output_buf_len,
                                      struct pjmedia_frame *output);

/* Definition for lyra codec operations. */
static pjmedia_codec_op lyra_op =
{
    &lyra_codec_init,
    &lyra_codec_open,
    &lyra_codec_close,
    &lyra_codec_modify,
    &lyra_codec_parse,
    &lyra_codec_encode,
    &lyra_codec_decode,
    &lyra_codec_recover
};

/* Definition for lyra codec factory operations. */
static pjmedia_codec_factory_op lyra_factory_op =
{
    &lyra_test_alloc,
    &lyra_default_attr,
    &lyra_enum_codecs,
    &lyra_alloc_codec,
    &lyra_dealloc_codec,
    &pjmedia_codec_lyra_deinit
};

struct lyra_param
{
    int              enabled;           /* Is this mode enabled?            */
    int              pt;                /* Payload type.                    */
    unsigned         clock_rate;        /* Default sampling rate to be used.*/
};

/* Lyra factory */
struct lyra_factory
{
    pjmedia_codec_factory  base;
    pjmedia_endpt         *endpt;
    pj_pool_t             *pool;
    char                   model_path_buf[LYRA_MAX_PATH_LEN];
    struct lyra_param      param[4];
};

/* lyra codec private data. */
struct lyra_data
{
    pj_pool_t           *pool;
    pj_mutex_t          *mutex;
    pj_bool_t            vad_enabled;
    pj_bool_t            plc_enabled;
    unsigned             samples_per_frame;
    unsigned             enc_bit_rate;
    unsigned             dec_bit_rate;

    std::unique_ptr<LyraEncoder> enc;
    std::unique_ptr<LyraDecoder> dec;
};

/* Codec factory instance */
static struct lyra_factory lyra_factory;

static pjmedia_codec_lyra_config lyra_cfg;

/*
 * Initialize and register lyra codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_lyra_init(pjmedia_endpt *endpt)
{
    pj_status_t status;
    pjmedia_codec_mgr *codec_mgr;

    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    if (lyra_factory.pool != NULL)
        return PJ_SUCCESS;

    /* Create the Lyra codec factory */
    lyra_factory.base.op = &lyra_factory_op;
    lyra_factory.base.factory_data = &lyra_factory;
    lyra_factory.endpt = endpt;

    lyra_factory.pool = pjmedia_endpt_create_pool(endpt, "lyra-factory",
                                                  1024, 1024);
    if (!lyra_factory.pool) {
        PJ_LOG(2, (THIS_FILE, "Unable to create memory pool for Lyra codec"));
        return PJ_ENOMEM;
    }
    lyra_factory.param[0].enabled = 0;
    lyra_factory.param[0].pt = PJMEDIA_RTP_PT_LYRA_8;
    lyra_factory.param[0].clock_rate = 8000;

    lyra_factory.param[1].enabled = 1;
    lyra_factory.param[1].pt = PJMEDIA_RTP_PT_LYRA_16;
    lyra_factory.param[1].clock_rate = 16000;

    lyra_factory.param[2].enabled = 0;
    lyra_factory.param[2].pt = PJMEDIA_RTP_PT_LYRA_32;
    lyra_factory.param[2].clock_rate = 32000;

    lyra_factory.param[3].enabled = 0;
    lyra_factory.param[3].pt = PJMEDIA_RTP_PT_LYRA_48;
    lyra_factory.param[3].clock_rate = 48000;

    /* Get the codec manager */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
        PJ_LOG(2, (THIS_FILE, "Unable to get the codec manager"));
        status = PJ_EINVALIDOP;
        goto on_codec_factory_error;
    }

    /* Register the codec factory */
    status = pjmedia_codec_mgr_register_factory(codec_mgr,
                                                &lyra_factory.base);
    if (status != PJ_SUCCESS) {
        PJ_LOG(2, (THIS_FILE, "Unable to register the codec factory"));
        goto on_codec_factory_error;
    }

    lyra_cfg.bit_rate = PJMEDIA_CODEC_LYRA_DEFAULT_BIT_RATE;
    pj_ansi_strxcpy(lyra_factory.model_path_buf,
                    PJMEDIA_CODEC_LYRA_DEFAULT_MODEL_PATH ,
                    sizeof(lyra_factory.model_path_buf));
    lyra_cfg.model_path = pj_str(lyra_factory.model_path_buf);

    return PJ_SUCCESS;

on_codec_factory_error:
    pj_pool_release(lyra_factory.pool);
    lyra_factory.pool = NULL;
    return status;
}

/*
 * Unregister lyra codec factory from pjmedia endpoint and deinitialize
 * the lyra codec library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_lyra_deinit(void)
{
    pj_status_t status;
    pjmedia_codec_mgr *codec_mgr;

    if (lyra_factory.pool == NULL)
        return PJ_SUCCESS;

    /* Get the codec manager */
    codec_mgr = pjmedia_endpt_get_codec_mgr(lyra_factory.endpt);
    if (!codec_mgr) {
        PJ_LOG(2, (THIS_FILE, "Unable to get the codec manager"));
        pj_pool_release(lyra_factory.pool);
        lyra_factory.pool = NULL;
        return PJ_EINVALIDOP;
    }

    /* Unregister the codec factory */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
                                                  &lyra_factory.base);
    if (status != PJ_SUCCESS)
        PJ_LOG(2, (THIS_FILE, "Unable to unregister the codec factory"));

    /* Release the memory pool */
    pj_pool_release(lyra_factory.pool);
    lyra_factory.pool = NULL;
    lyra_factory.endpt = NULL;

    return status;
}

/* Prototypes for lyra factory */
static pj_status_t lyra_test_alloc(pjmedia_codec_factory *factory,
                                   const pjmedia_codec_info *info)
{
    unsigned i;

    PJ_UNUSED_ARG(factory);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
        return PJMEDIA_CODEC_EUNSUP;

    /* Check encoding name. */
    if (pj_strcmp2(&info->encoding_name, LYRA_STR) != 0)
        return PJMEDIA_CODEC_EUNSUP;

    /* Check clock-rate */
    for (i = 0; i < PJ_ARRAY_SIZE(lyra_factory.param); ++i) {
        if (info->clock_rate == lyra_factory.param[i].clock_rate) {
            return PJ_SUCCESS;
        }
    }

    /* Unsupported, or mode is disabled. */
    return PJMEDIA_CODEC_EUNSUP;
}

static char *get_bit_rate_val(unsigned bit_rate) 
{
    static char buf_3200[] = "3200";
    static char buf_6000[] = "6000";
    static char buf_9200[] = "9200";

    switch (bit_rate) {
        case 3200:
            return buf_3200;
        case 6000:
            return buf_6000;
        case 9200:
            return buf_9200;
    }
    return 0;
}

static pj_status_t lyra_default_attr(pjmedia_codec_factory *factory,
                                     const pjmedia_codec_info *id,
                                     pjmedia_codec_param *attr)
{
    PJ_ASSERT_RETURN(factory == &lyra_factory.base, PJ_EINVAL);

    unsigned idx = 0;
    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.pt = (pj_uint8_t)id->pt;
    attr->info.channel_cnt = 1;

    for (; idx < PJ_ARRAY_SIZE(lyra_factory.param); ++idx) {
        if (lyra_factory.param[idx].enabled &&
            id->clock_rate == lyra_factory.param[idx].clock_rate)
        {
            break;
        }
    }
    if (idx == PJ_ARRAY_SIZE(lyra_factory.param)) {
        PJ_EINVAL;
    }
    attr->info.clock_rate = lyra_factory.param[idx].clock_rate;
    attr->info.avg_bps = lyra_cfg.bit_rate;
    attr->info.max_bps = LYRA_MAX_BITRATE;

    attr->info.pcm_bits_per_sample = 16;
    attr->info.frm_ptime = 20;
    attr->setting.frm_per_pkt = 1;
    attr->setting.dec_fmtp.cnt = 1;
    pj_strset2(&attr->setting.dec_fmtp.param[0].name, BIT_RATE_STR);
    pj_strset2(&attr->setting.dec_fmtp.param[0].val, 
               get_bit_rate_val(lyra_cfg.bit_rate));

    /* Default flags. */
    attr->setting.cng = 0;
    attr->setting.plc = 0;
    attr->setting.penh = 0;
    attr->setting.vad = LYRA_DEFAULT_VAD;

    return PJ_SUCCESS;
}

static pj_status_t lyra_enum_codecs(pjmedia_codec_factory *factory,
                                    unsigned *count,
                                    pjmedia_codec_info codecs[])
{
    int i;
    unsigned max_cnt;

    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(factory == &lyra_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    max_cnt = *count;
    *count = 0;

    for (i = PJ_ARRAY_SIZE(lyra_factory.param) - 1;
         i >= 0 && *count < max_cnt; --i)
    {
        if (!lyra_factory.param[i].enabled)
            continue;

        pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
        pj_strset2(&codecs[*count].encoding_name, LYRA_STR);
        codecs[*count].pt = lyra_factory.param[i].pt;
        codecs[*count].type = PJMEDIA_TYPE_AUDIO;
        codecs[*count].clock_rate = lyra_factory.param[i].clock_rate;
        codecs[*count].channel_cnt = 1;

        ++*count;
    }

    return PJ_SUCCESS;
}

static pj_status_t lyra_alloc_codec(pjmedia_codec_factory *factory,
                                    const pjmedia_codec_info *id,
                                    pjmedia_codec **p_codec)
{
    pj_pool_t *pool;
    struct lyra_data *lyra_data;
    pjmedia_codec *codec;
    pj_status_t status;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &lyra_factory.base, PJ_EINVAL);

    pool = pjmedia_endpt_create_pool(lyra_factory.endpt, "lyra%p", 2000, 2000);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    lyra_data = PJ_POOL_ZALLOC_T(pool, struct lyra_data);
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);

    status = pj_mutex_create_simple(pool, "lyra_mutex", &lyra_data->mutex);
    if (status != PJ_SUCCESS) {
        pj_pool_release(pool);
        return status;
    }

    lyra_data->pool = pool;
    lyra_data->dec_bit_rate = lyra_cfg.bit_rate;

    codec->op = &lyra_op;
    codec->codec_data = lyra_data;
    codec->factory = factory;

    *p_codec = codec;
    return PJ_SUCCESS;
}

static pj_status_t lyra_dealloc_codec(pjmedia_codec_factory *factory,
                                      pjmedia_codec *codec)
{
    struct lyra_data *lyra_data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(factory == &lyra_factory.base, PJ_EINVAL);

    lyra_data = (struct lyra_data*)codec->codec_data;
    if (lyra_data) {
        pj_mutex_destroy(lyra_data->mutex);
        lyra_data->mutex = NULL;
        pj_pool_release(lyra_data->pool);
    }

    return PJ_SUCCESS;
}

/* Prototypes for lyra implementation. */
static pj_status_t lyra_codec_init(pjmedia_codec *codec,
                                   pj_pool_t *pool)
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

static pj_status_t lyra_codec_open(pjmedia_codec *codec,
                                   pjmedia_codec_param *attr)
{
    ghc::filesystem::path model_path = lyra_cfg.model_path.ptr;
    struct lyra_data *lyra_data = (struct lyra_data*)codec->codec_data;

    pj_mutex_lock(lyra_data->mutex);

    PJ_LOG(4, (THIS_FILE, "Codec opening, model_path=%.*s",
            (int)lyra_cfg.model_path.slen, lyra_cfg.model_path.ptr));

    /* Get encoder bit_rate */
    for (unsigned i = 0; i < attr->setting.enc_fmtp.cnt; ++i) {
        if (pj_strcmp2(&attr->setting.enc_fmtp.param[i].name, 
                       BIT_RATE_STR) == 0)
        {
            lyra_data->enc_bit_rate = (pj_uint16_t)
                              pj_strtoul(&attr->setting.enc_fmtp.param[i].val);
            break;
        }
    }
    if (lyra_data->enc_bit_rate == 0 ||
        (lyra_data->enc_bit_rate != 3200 && lyra_data->enc_bit_rate != 6000 &&
         lyra_data->enc_bit_rate != 9200))
    {
        lyra_data->enc_bit_rate = PJMEDIA_CODEC_LYRA_DEFAULT_BIT_RATE;
    }

    lyra_data->vad_enabled = (attr->setting.vad != 0);
    lyra_data->plc_enabled = (attr->setting.plc != 0);

    lyra_data->enc = LyraEncoder::Create(attr->info.clock_rate,
                                         attr->info.channel_cnt,
                                         lyra_data->enc_bit_rate,
                                         lyra_data->vad_enabled,
                                         model_path);
    if (lyra_data->enc == nullptr) {
        PJ_LOG(2, (THIS_FILE, "Could not create lyra encoder"));
        pj_mutex_unlock(lyra_data->mutex);
        return PJMEDIA_CODEC_EFAILED;
    }

    lyra_data->dec = LyraDecoder::Create(attr->info.clock_rate,
                                         attr->info.channel_cnt,
                                         model_path);
    if (lyra_data->dec == nullptr) {
        PJ_LOG(2, (THIS_FILE, "Could not create lyra decoder"));
        pj_mutex_unlock(lyra_data->mutex);
        return PJMEDIA_CODEC_EFAILED;
    }
    lyra_data->samples_per_frame =
                          attr->info.clock_rate / lyra_data->enc->frame_rate();

    PJ_LOG(4, (THIS_FILE, "Codec opened, model_path=%.*s, chan_cnt=%d, "
            "enc_bit_rate=%d, dec_bit_rate=%d, clockrate=%d, vad=%d, "
            "frame_rate=%d, samples_per_frame=%d",
            (int)lyra_cfg.model_path.slen, lyra_cfg.model_path.ptr,
            attr->info.channel_cnt, lyra_data->enc_bit_rate,
            lyra_data->dec_bit_rate, attr->info.clock_rate,
            lyra_data->vad_enabled, lyra_data->enc->frame_rate(),
            lyra_data->samples_per_frame));

    pj_mutex_unlock(lyra_data->mutex);
    return PJ_SUCCESS;
}

static pj_status_t lyra_codec_close(pjmedia_codec *codec)
{
    struct lyra_data *lyra_data = (struct lyra_data*)codec->codec_data;

    /* Destroy encoder*/
    if (lyra_data->enc) {
        lyra_data->enc.reset();
    }

    /* Destroy decoder */
    if (lyra_data->dec) {
        lyra_data->dec.reset();
    }

    return PJ_SUCCESS;
}

static pj_status_t lyra_codec_modify(pjmedia_codec *codec,
                                     const pjmedia_codec_param *attr)
{
    struct lyra_data *lyra_data = (struct lyra_data*)codec->codec_data;

    lyra_data->plc_enabled = (attr->setting.plc != 0);
    lyra_data->vad_enabled = (attr->setting.vad != 0);

    return PJ_SUCCESS;
}

static pj_status_t lyra_codec_parse(pjmedia_codec *codec,
                                    void *pkt,
                                    pj_size_t pkt_size,
                                    const pj_timestamp *ts,
                                    unsigned *frame_cnt,
                                    pjmedia_frame frames[])
{
    unsigned count = 0;
    struct lyra_data* lyra_data = (struct lyra_data*)codec->codec_data;
    unsigned frm_size = lyra_data->dec_bit_rate / (50 * 8);

    PJ_UNUSED_ARG(codec);

    PJ_ASSERT_RETURN(ts && frame_cnt && frames, PJ_EINVAL);

    pj_mutex_lock(lyra_data->mutex);
    while (pkt_size >= frm_size && count < *frame_cnt) {
        frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
        frames[count].buf = pkt;
        frames[count].size = frm_size;
        frames[count].timestamp.u64 = ts->u64 +
                                          lyra_data->samples_per_frame * count;
        pkt = ((char*)pkt) + frm_size;
        pkt_size -= frm_size;

        ++count;
    }

    *frame_cnt = count;
    pj_mutex_unlock(lyra_data->mutex);
    return PJ_SUCCESS;
}

static pj_status_t lyra_codec_encode(pjmedia_codec *codec,
                                     const struct pjmedia_frame *input,
                                     unsigned output_buf_len,
                                     struct pjmedia_frame *output)
{
    struct lyra_data *lyra_data = (struct lyra_data*)codec->codec_data;
    unsigned samples_per_frame = lyra_data->samples_per_frame;
    int i = 0;
    pj_int16_t *pcm_in = (pj_int16_t*)input->buf;
    pj_size_t in_size = input->size >> 1;
    std::vector<pj_uint8_t> encoded_data;

    pj_mutex_lock(lyra_data->mutex);

    if (input->type != PJMEDIA_FRAME_TYPE_AUDIO) {
        output->size = 0;
        output->buf = NULL;
        output->timestamp = input->timestamp;
        output->type = input->type;
        pj_mutex_unlock(lyra_data->mutex);
        return PJ_SUCCESS;
    }

    for (int proc_size = 0;
         proc_size + samples_per_frame <= in_size;
         proc_size += samples_per_frame, ++i)
    {
        auto encoded = lyra_data->enc->Encode(
                                     absl::MakeConstSpan(pcm_in+proc_size,
                                                         samples_per_frame));
        if (!encoded.has_value()) {
            PJ_LOG(2, (THIS_FILE, 
                  "Unable to encode starting at samples at byte %d.",
                   proc_size));
            pj_mutex_unlock(lyra_data->mutex);
            return PJMEDIA_CODEC_EFAILED;
        }
        encoded_data.insert(encoded_data.end(),
                            encoded.value().begin(),
                            encoded.value().end());

    }
    output->size = encoded_data.size();
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;
    std::copy(encoded_data.begin(), encoded_data.end(), (char*)output->buf);

    pj_mutex_unlock(lyra_data->mutex);
    return PJ_SUCCESS;
}

static pj_status_t lyra_codec_decode(pjmedia_codec *codec,
                                     const struct pjmedia_frame *input,
                                     unsigned output_buf_len,
                                     struct pjmedia_frame *output)
{
    struct lyra_data *lyra_data = (struct lyra_data*)codec->codec_data;
    unsigned samples_per_frame = lyra_data->samples_per_frame;
    unsigned samples_decoded = 0;
    std::vector<pj_int16_t> decoded_data;

    pj_mutex_lock(lyra_data->mutex);
    if (input) {
        pj_uint8_t *in_data = (pj_uint8_t*)input->buf;
        if (input->type != PJMEDIA_FRAME_TYPE_AUDIO) {
            pjmedia_zero_samples((pj_int16_t*)output->buf, samples_per_frame);
            output->size = samples_per_frame << 1;
            output->timestamp.u64 = input->timestamp.u64;
            output->type = PJMEDIA_FRAME_TYPE_AUDIO;
            pj_mutex_unlock(lyra_data->mutex);
            return PJ_SUCCESS;
        }
        if (!lyra_data->dec->SetEncodedPacket(
                                    absl::MakeConstSpan(in_data, input->size)))
        {
            PJ_LOG(4, (THIS_FILE, "Unable to set encoded packet"));
            pj_mutex_unlock(lyra_data->mutex);
            return PJMEDIA_CODEC_EFAILED;
        }
    }

    while (samples_decoded < samples_per_frame) {
        unsigned samples_to_request = samples_per_frame - samples_decoded;

        auto decoded = lyra_data->dec->DecodeSamples(samples_to_request);
        if (!decoded.has_value()) {
            PJ_LOG(4, (THIS_FILE, "Decode failed!"));
            pj_mutex_unlock(lyra_data->mutex);
            return PJMEDIA_CODEC_EFAILED;
        }
        samples_decoded += (unsigned)decoded->size();
        decoded_data.insert(decoded_data.end(), decoded.value().begin(),
                            decoded.value().end());
    }
    std::copy(decoded_data.begin(), decoded_data.end(),
              (pj_int16_t*)output->buf);
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = samples_per_frame << 1;
    if (input) {
        output->timestamp.u64 = input->timestamp.u64;
    }
    pj_mutex_unlock(lyra_data->mutex);
    return PJ_SUCCESS;
}

static pj_status_t lyra_codec_recover(pjmedia_codec *codec,
                                      unsigned output_buf_len,
                                      struct pjmedia_frame *output)
{
    struct lyra_data *lyra_data = (struct lyra_data*)codec->codec_data;
    unsigned samples_per_frame = lyra_data->samples_per_frame;
    pj_status_t status;

    //PJ_LOG(4, (THIS_FILE, "Codec recover"));

    /* output_buf_len is unreferenced when building in Release mode */
    PJ_UNUSED_ARG(output_buf_len);

    pj_assert(samples_per_frame <= output_buf_len / 2);

    status = lyra_codec_decode(codec, NULL, output_buf_len, output);

    output->size = samples_per_frame << 1;

    return status;
}

PJ_DEF(pj_status_t)
pjmedia_codec_lyra_get_config(pjmedia_codec_lyra_config *cfg)
{
    PJ_ASSERT_RETURN(cfg, PJ_EINVAL);

    pj_memcpy(cfg, &lyra_cfg, sizeof(pjmedia_codec_lyra_config));
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t)
pjmedia_codec_lyra_set_config(const pjmedia_codec_lyra_config *cfg)
{
    if (cfg->bit_rate != 3200 && cfg->bit_rate != 6000 &&
        cfg->bit_rate != 9200)
    {
        return PJ_EINVAL;
    }
    lyra_cfg.bit_rate = cfg->bit_rate;
    pj_strncpy_with_null(&lyra_cfg.model_path, &cfg->model_path,
                         PJ_ARRAY_SIZE(lyra_factory.model_path_buf));
    return PJ_SUCCESS;
}

#if defined(_MSC_VER)
#   pragma comment(lib, "liblyra")
#endif

#endif
