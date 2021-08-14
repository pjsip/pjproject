/*
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
*
* Dicky Tamara
* dickytamara@hotmail.com
*/

#include <pjmedia/codec.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pjmedia-codec/types.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/log.h>

#if defined(PJMEDIA_HAS_AAC_CODEC) && (PJMEDIA_HAS_AAC_CODEC!=0)
#include <pjmedia-codec/aac.h>
#include <fdk-aac/FDK_audio.h>
#include <fdk-aac/aacenc_lib.h>
#include <fdk-aac/aacdecoder_lib.h>

#define THIS_FILE       "aac.c"


/** 
 * LD                  = ptime 10   0x01					MAIN
 * ELD Downsample      = ptime 20   0x01 | 0x03			    MAIN + SBR
 * ELD Dualrate        = ptime 10   0x01 | 0x03 			MAIN + SBR
 * ELDV2 Dualrate      = ptime 10   Ox01 | 0x03 | 0x08		MAIN + SBR + MPSC(SAC)
 * for HE-AACv2 minimum audio encoded buffer was 8192;
 * LC AAC-LC, HE-AAC, HE-AACv2 in Dualrate SBR mode.
 * -----------------------------------------------------------------------------------
 * Audio Object Type  |  Bit Rate Range  |            Supported  | Preferred  | No.of
 * |         [bit/s]  |       Sampling Rates  |    Sampl.  |  Chan.
 * |                  |                [kHz]  |      Rate  |
 * |                  |                       |     [kHz]  |
 * -------------------+------------------+-----------------------+------------+-------
 * AAC LC + SBR + PS  |  40000 -  64000  |  32.00, 44.10, 48.00  |     48.00  | 2
 * AAC LC + SBR       |  64000 - 128000  |  32.00, 44.10, 48.00  |     48.00  | 2
 * AAC LC             | 320002 - 576000  |                48.00  |     48.00  | 2
 * AAC-LD, AAC-ELD, AAC-ELD with SBR in Dualrate SBR
 * -----------------------------------------------------------------------------------
 * Audio Object Type  |  Bit Rate Range  |            Supported  | Preferred  | No.of
 * |         [bit/s]  |       Sampling Rates  |    Sampl.  |  Chan.
 * |                  |                [kHz]  |      Rate  |
 * |                  |                       |     [kHz]  |
 * -------------------+------------------+-----------------------+------------+-------
 * ELD + SBR          |  52000 - 128000  |        32.00 - 48.00  |     48.00  | 2
 * LD, ELD            | 136000 - 384000  |        44.10 - 48.00  |     48.00  | 2
 * ELD AAC-ELD with SBR in Downsampled SBR mode.
 * -----------------------------------------------------------------------------------
 * Audio Object Type  |  Bit Rate Range  |            Supported  | Preferred  | No.of
 * |         [bit/s]  |       Sampling Rates  |    Sampl.  |  Chan.
 * |                  |                [kHz]  |      Rate  |
 * |                  |                       |     [kHz]  |
 * -------------------+------------------+-----------------------+------------+-------
 * ELD + SBR          |  32000 - 51999   |        16.00 - 24.00  |     24.00  | 2
 * (downsampled SBR)  |  52000 - 59999   |        22.05 - 24.00  |     24.00  | 2
 * |  60000 - 95999   |        22.05 - 32.00  |     32.00  | 2
 * |  96000 - 128000  |        22.05 - 48.00  |     32.00  | 2
 * 
 * ELDv2 AAC-ELD v2, AAC-ELD v2 with SBR.
 * -----------------------------------------------------------------------------------
 * Audio Object Type  |  Bit Rate Range  |            Supported  | Preferred  | No.of
 * |         [bit/s]  |       Sampling Rates  |    Sampl.  |  Chan.
 * |                  |                [kHz]  |      Rate  |
 * |                  |                       |     [kHz]  |
 * -------------------+------------------+-----------------------+------------+-------
 * ELD-212            |  85000 - 192000  |        44.10 - 48.00  |     48.00  | 2
 * -------------------+------------------+-----------------------+------------+-------
 * ELD-212 + SBR      |  32000 -  64000  |        32.00 - 48.00  |     48.00  | 2
 * (dualrate SBR)     |                  |                       |            |
 * 
 * Best Audio Quality setting
 * ELD Dualrate:
 * #define DEFAULT_AOT 				AOT_ER_AAC_ELD
 * #define DEFAULT_USE_SBR				1
 * #define DEFAULT_SBR_RATIO           2
 * #define DEFAULT_GRANULE_LENGTH 		480
 * #define DEFAULT_PTIME       		20
 * #define DEFAULT_BITRATE_MODE 		3 
 * #define DEFAULT_IS_ELD_V2 			0
 * #define DEFAULT_ELDV2_SBR			1
 * #define DEFAULT_TRANSMUX			TT_MP4_RAW
 * #define DEFAULT_SIGNALING_MODE		2
 * #define DEFAULT_USE_INTERNAL_PLC    1
 * 
 */


/* default internal aac config */
#define DEFAULT_CLOCK_RATE          48000
#define DEFAULT_CHANNEL_COUNT       2
#define DEFAULT_AOT                 AOT_ER_AAC_ELD	// AOT_PS,AOT_ER_AAC_ELD, AOT_ER_AAC_ELD AOT_PS 
#define DEFAULT_USE_SBR             1				// -1 default configurator, 0 disable, 1 enable
#define DEFAULT_SBR_RATIO           2				// 1 downsampled, 2 dualrate;
#define DEFAULT_GRANULE_LENGTH      480				// 480
#define DEFAULT_PTIME               20
#define DEFAULT_BITRATE_MODE        0				// 0 constant, 1 -> 5;
#define DEFAULT_IS_ELD_V2           0
#define DEFAULT_ELDV2_SBR           1 
#define DEFAULT_TRANSMUX            TT_MP4_RAW
#define DEFAULT_SIGNALING_MODE      2
#define DEFAULT_USE_INTERNAL_PLC    1

#define INDEX_BIT_LENGTH    		3

/* Prototypes for AAC factory */
static pj_status_t factory_test_alloc(pjmedia_codec_factory *factory, const pjmedia_codec_info *id);
static pj_status_t factory_default_attr(pjmedia_codec_factory *factory, const pjmedia_codec_info *id, pjmedia_codec_param *attr);
static pj_status_t factory_enum_codecs(pjmedia_codec_factory *factory, unsigned *count, pjmedia_codec_info codecs[]); \
static pj_status_t factory_alloc_codec(pjmedia_codec_factory *factory, const pjmedia_codec_info *id, pjmedia_codec **p_codec);
static pj_status_t factory_dealloc_codec(pjmedia_codec_factory *factory, pjmedia_codec *codec);

/* Prototypes for AAC implementation. */
static pj_status_t aac_codec_init(pjmedia_codec *codec, pj_pool_t *pool);
static pj_status_t aac_codec_open(pjmedia_codec *codec, pjmedia_codec_param *attr);
static pj_status_t aac_codec_close(pjmedia_codec *codec);
static pj_status_t aac_codec_modify(pjmedia_codec *codec, const pjmedia_codec_param *attr);
static pj_status_t aac_codec_parse(pjmedia_codec *codec, void *pkt, pj_size_t pkt_size, const pj_timestamp *timestamp, unsigned *frame_cnt, pjmedia_frame frames[]);
static pj_status_t aac_codec_encode(pjmedia_codec *codec, const struct pjmedia_frame *input, unsigned output_buf_len, struct pjmedia_frame *output);
static pj_status_t aac_codec_decode(pjmedia_codec *codec, const struct pjmedia_frame *input, unsigned output_buf_len, struct pjmedia_frame *output);
static pj_status_t aac_codec_recover(pjmedia_codec *codec, unsigned output_buf_len, struct pjmedia_frame *output);

/* Definition for AAC codec operations. */
static pjmedia_codec_op aac_op = {
	&aac_codec_init,
	&aac_codec_open,
	&aac_codec_close,
	&aac_codec_modify,
	&aac_codec_parse,
	&aac_codec_encode,
	&aac_codec_decode,
	&aac_codec_recover
};

/* Definition for AAC codec factory operations. */
static pjmedia_codec_factory_op aac_factory_op = {
	&factory_test_alloc,
	&factory_default_attr,
	&factory_enum_codecs,
	&factory_alloc_codec,
	&factory_dealloc_codec,
	&pjmedia_codec_aac_deinit
};

/* AAC factory private data */
static struct aac_factory {
	pjmedia_codec_factory base;
	pjmedia_endpt *endpt;
	pj_pool_t *pool;
};

/* AAC codec private data. */
struct aac_encoder_params {
	AUDIO_OBJECT_TYPE           aot;
	CHANNEL_MODE                channel_mode;
	int                         granule_length;
	pj_bool_t                   sbr;
};

struct aac_private {
	pj_pool_t *pool; /**< Pool for each instance.    */
	pj_mutex_t                  *mutex; // mutex for locking

	pj_bool_t                   enc_ready;
	unsigned                    clock_rate;      /**< Sampling rate in Hz        */
	unsigned                    channel_cnt;
	HANDLE_AACENCODER           hAacEnc;
	struct aac_encoder_params   enc_params;


	pj_bool_t                   dec_ready;
	HANDLE_AACDECODER           hAacDec;
	unsigned                    pcm_frame_size;  /**< PCM frame size in bytes */
	unsigned 					enc_frameLength;
//	unsigned                    min_peak;
//	unsigned                    max_peak;
};



/* codec factory instance */
static struct aac_factory aac_factory;

/**
* Utilities from pjsip
*/
#define hex_digits   "0123456789abcdef"

static void val_to_hex_digit(unsigned value, char *p) {
	*p++ = hex_digits[(value & 0xF0) >> 4];
	*p = hex_digits[(value & 0x0F)];
}

static unsigned hex_digit_to_val(unsigned char c) {
	if (c <= '9')
		return (c - '0') & 0x0F;
	else if (c <= 'F')
		return  (c - 'A' + 10) & 0x0F;
	else
		return (c - 'a' + 10) & 0x0F;
}


static pj_status_t aac_encoder_open(HANDLE_AACENCODER* hAacEnc, const pjmedia_codec_param *attr, const struct aac_encoder_params *aac_params) {
	AACENC_ERROR error;
	UINT         encModules = 0x01; /* AAC core */

	if (aac_params->sbr) {
		/* AAC SBR */
		encModules |= 0x02;

		/* AAC MPS MODULE */
		if (DEFAULT_IS_ELD_V2 == 1 && DEFAULT_USE_SBR == 0) {
			encModules |= 0x02;
			encModules |= 0x08;
		}
		else {
			encModules |= 0x08;
		}

		/* HE-AAC v2 for DAB+*/
		if (DEFAULT_AOT == AOT_PS) {
			// encModules |= 0x04;
			encModules = 0x0;
		}
	}

	error = aacEncOpen(hAacEnc, encModules, attr->info.channel_cnt);
	if (error != AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "Error while creating encoder %x", error));
		return PJ_EINVAL;
	}

	/* setting aac encoder Audio Object Type */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_AOT, aac_params->aot) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_AOT: %d", aac_params->aot));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_AOT: %x", error));
	}

	/* setting samplerate or clock_rate */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_SAMPLERATE, attr->info.clock_rate) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_SAMPLERATE: %d", attr->info.clock_rate));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_SAMPLERATE: %x", error));
	}

	/* setting channel mode */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_CHANNELMODE, aac_params->channel_mode) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_CHANNELMODE: %d", aac_params->channel_mode));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_CHANNELMODE: %x", error));
	}

	/* setting channel order */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_CHANNELORDER, 1) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_CHANNELORDER: %d", 1));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_CHANNELORDER: %x", error));
	}

	/* setting transmux */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_TRANSMUX, DEFAULT_TRANSMUX/* TT_MP4_RAW */) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_TRANSMUX: %d", DEFAULT_TRANSMUX));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_TRANSMUX: %x", error));
	}

	/* setting Signaling Mode */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_SIGNALING_MODE, DEFAULT_SIGNALING_MODE) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_SIGNALING_MODE: %d", DEFAULT_SIGNALING_MODE));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_SIGNALING_MODE: %x", error));
	}

	/* setting Granule Length */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_GRANULE_LENGTH, aac_params->granule_length) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_GRANULE_LENGTH: %d", aac_params->granule_length));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_GRANULE_LENGTH: %x", error));
	}

	/* setting SBR Mode */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_SBR_MODE, aac_params->sbr) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_SBR_MODE: %d", aac_params->sbr));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_SBR_MODE: %x", error));
	}

	/* setting SBR Mode */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_SBR_RATIO, DEFAULT_SBR_RATIO) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_SBR_RATIO: %d", DEFAULT_SBR_RATIO));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_SBR_RATIO: %x", error));
	}

	if (DEFAULT_BITRATE_MODE <= 5) {
		/* setting bitrate mode 0 constant bitrate, 1,2,3,4,5 is vbr */
		if (error = aacEncoder_SetParam(*hAacEnc, AACENC_BITRATEMODE, DEFAULT_BITRATE_MODE) == AACENC_OK) {
			PJ_LOG(1, (THIS_FILE, "SET AACENC_BITRATEMODE: %d", DEFAULT_BITRATE_MODE));
		}
		else {
			PJ_LOG(1, (THIS_FILE, "ERR AACENC_BITRATEMODE: %x", error));
		}
		// unsigned bitrate = aacEncoder_GetParam(*hAacEnc, AACENC_BITRATE);
		// PJ_LOG(1, (THIS_FILE, "CURRENT BITRATE: %d", bitrate)); 
	}

	if (DEFAULT_BITRATE_MODE > 5 && DEFAULT_ELDV2_SBR == 0) {
		// /* setting bitrate */
		if (error = aacEncoder_SetParam(*hAacEnc, AACENC_BITRATE, attr->info.avg_bps) == AACENC_OK) {
			PJ_LOG(1, (THIS_FILE, "SET AACENC_BITRATE: %d", attr->info.max_bps));
		}
		else {
			PJ_LOG(1, (THIS_FILE, "ERR AACENC_BITRATE: %x", error));
		}
	}


	/* setting afterburner */
	if (error = aacEncoder_SetParam(*hAacEnc, AACENC_AFTERBURNER, 1) == AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "SET AACENC_AFTERBURNER: %d", 1));
	}
	else {
		PJ_LOG(1, (THIS_FILE, "ERR AACENC_PEAK_BITRATE: %x", error));
	}

	// PJ_LOG(1, (THIS_FILE, "Error Set Param %d", error));
	error = aacEncEncode(*hAacEnc, NULL, NULL, NULL, NULL);
	if (error != AACENC_OK) {
		PJ_LOG(1, (THIS_FILE, "Error while initializing encoder %d", error));
		return PJ_EINVAL;
	}
	return PJ_SUCCESS;
}

static void aac_add_int_codec_param(pj_pool_t* pool, pjmedia_codec_fmtp* fmtp, pj_str_t name, int value) {
	fmtp->param[fmtp->cnt].name = name;
	fmtp->param[fmtp->cnt].val.ptr = (char*)pj_pool_alloc(pool, 32);
	fmtp->param[fmtp->cnt].val.slen = pj_utoa(value, fmtp->param[fmtp->cnt].val.ptr);
	fmtp->cnt++;
}

static void aac_add_str_codec_param(pj_pool_t* pool, pjmedia_codec_fmtp* fmtp, pj_str_t name, pj_str_t value) {
	fmtp->param[fmtp->cnt].name = name;
	fmtp->param[fmtp->cnt].val = value;
	fmtp->cnt++;
}

/**
* Apply aac settings to dec_fmtp parameters
*/
static void aac_apply_codec_params(pj_pool_t* pool, pjmedia_codec_param *attr, const struct aac_encoder_params *aac_params) {
	unsigned i;
	HANDLE_AACENCODER hAacEnc;
	AACENC_InfoStruct pInfo;
	attr->setting.dec_fmtp.cnt = 0;

	// sdp parameters
	aac_add_int_codec_param(pool, &attr->setting.dec_fmtp, pj_str("streamtype"), 5);
	aac_add_str_codec_param(pool, &attr->setting.dec_fmtp, pj_str("mode"), pj_str("AAC-hbr"));
	aac_add_str_codec_param(pool, &attr->setting.dec_fmtp, pj_str("profile-level-id"), pj_str("58"));
	aac_add_int_codec_param(pool, &attr->setting.dec_fmtp, pj_str("sizelength"), 16 - INDEX_BIT_LENGTH);
	aac_add_int_codec_param(pool, &attr->setting.dec_fmtp, pj_str("indexlength"), INDEX_BIT_LENGTH);
	aac_add_int_codec_param(pool, &attr->setting.dec_fmtp, pj_str("indexdeltalength"), INDEX_BIT_LENGTH);
	aac_add_int_codec_param(pool, &attr->setting.dec_fmtp, pj_str("object"), aac_params->aot);
	if (aac_params->sbr) {
		aac_add_int_codec_param(pool, &attr->setting.dec_fmtp, pj_str("sbr"), 1);
	}
	aac_add_int_codec_param(pool, &attr->setting.dec_fmtp, pj_str("bitrate"), attr->info.avg_bps);


	/* Compute config */
	aac_encoder_open(&hAacEnc, attr, aac_params);
	// aacEncoder_GetParams(&hAacEnc)
	aacEncInfo(hAacEnc, &pInfo);

	attr->setting.dec_fmtp.param[attr->setting.dec_fmtp.cnt].name = pj_str("config");
	attr->setting.dec_fmtp.param[attr->setting.dec_fmtp.cnt].val.slen = pInfo.confSize * 2;
	attr->setting.dec_fmtp.param[attr->setting.dec_fmtp.cnt].val.ptr = (char*)pj_pool_alloc(pool, pInfo.confSize * 2);
	for (i = 0; i < pInfo.confSize; i++) {
		val_to_hex_digit(pInfo.confBuf[i], &attr->setting.dec_fmtp.param[attr->setting.dec_fmtp.cnt].val.ptr[2 * i]);
	}
	attr->setting.dec_fmtp.cnt++;
	aacEncClose(&hAacEnc);
}

PJ_DEF(pj_status_t) pjmedia_codec_aac_init(pjmedia_endpt *endpt) {
	pjmedia_codec_mgr *codec_mgr;
	pj_status_t status;

	/* Already initialized. */
	if (aac_factory.endpt != NULL)
		return PJ_SUCCESS;

	/* Init factory */
	aac_factory.base.op = &aac_factory_op;
	aac_factory.base.factory_data = &aac_factory;
	aac_factory.endpt = endpt;

	/* Create pool */
	aac_factory.pool = pjmedia_endpt_create_pool(endpt, "aac codecs", 1000, 1000);
	if (!aac_factory.pool) {
		PJ_LOG(2, (THIS_FILE, "Unable to create memory pool for AAC-CODEC"));
		return PJ_ENOMEM;
	}

	/* Get the codec manager. */
	codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
	if (!codec_mgr) {
		PJ_LOG(2, (THIS_FILE, "Unable to get codec manager"));
		status = PJ_EINVALIDOP;
		goto on_error;
	}

	/* Register codec factory to endpoint. */
	status = pjmedia_codec_mgr_register_factory(codec_mgr, &aac_factory.base);
	if (status != PJ_SUCCESS) {
		PJ_LOG(2, (THIS_FILE, "Unable to register the codec factory"));
		goto on_error;
	}

	return PJ_SUCCESS;

on_error:
	pj_pool_release(aac_factory.pool);
	aac_factory.pool = NULL;
	return status;
}

/*
* Unregister AAC codec factory from pjmedia endpoint and deinitialize
* the AAC codec library.
*/
PJ_DEF(pj_status_t) pjmedia_codec_aac_deinit(void) {
	pjmedia_codec_mgr *codec_mgr;
	pj_status_t status;

	if (aac_factory.endpt == NULL) {
		return PJ_SUCCESS;
	}

	/* Get the codec manager. */
	codec_mgr = pjmedia_endpt_get_codec_mgr(aac_factory.endpt);
	if (!codec_mgr) {
		aac_factory.endpt = NULL;
		return PJ_EINVALIDOP;
	}

	/* Unregister AAC codec factory. */
	status = pjmedia_codec_mgr_unregister_factory(codec_mgr, &aac_factory.base);
	aac_factory.endpt = NULL;

	/* Release pool. */
	pj_pool_release(aac_factory.pool);
	aac_factory.pool = NULL;

	return status;
}

/*
* Check if factory can allocate the specified codec.
*/
static pj_status_t factory_test_alloc(pjmedia_codec_factory *factory,
	const pjmedia_codec_info *info) {
	const pj_str_t aac_tag = { "mpeg4-generic", 13 };
	unsigned i;

	PJ_UNUSED_ARG(factory);
	PJ_ASSERT_RETURN(factory == &aac_factory.base, PJ_EINVAL);

	/* Type MUST be audio. */
	if (info->type != PJMEDIA_TYPE_AUDIO)
		return PJMEDIA_CODEC_EUNSUP;

	/* Check encoding name. */
	if (pj_stricmp(&info->encoding_name, &aac_tag) != 0)
		return PJMEDIA_CODEC_EUNSUP;

	/* Check clock-rate */
	if (info->clock_rate != 48000)
		return PJMEDIA_CODEC_EUNSUP;

	return PJ_SUCCESS;
}

/*
* Generate default attribute.
*/
static pj_status_t factory_default_attr(pjmedia_codec_factory *factory,
	const pjmedia_codec_info *id, pjmedia_codec_param *attr) {
	struct aac_encoder_params default_aac_params;
	pj_bzero(attr, sizeof(pjmedia_codec_param));
	PJ_ASSERT_RETURN(factory == &aac_factory.base, PJ_EINVAL);

	/* this AAC Codec implemetation only support stereo */

	default_aac_params.aot = DEFAULT_AOT;

	if (DEFAULT_IS_ELD_V2 == 1) {
		default_aac_params.channel_mode = MODE_212;
	}
	else {
		default_aac_params.channel_mode = (attr->info.channel_cnt == 1) ? MODE_1 : MODE_2;
	}

	default_aac_params.granule_length = DEFAULT_GRANULE_LENGTH;

	if (DEFAULT_AOT == AOT_ER_AAC_LD) {
		default_aac_params.sbr = 0;
	}
	else {
		default_aac_params.sbr = DEFAULT_USE_SBR;
	}

	attr->info.clock_rate = DEFAULT_CLOCK_RATE;
	attr->info.channel_cnt = DEFAULT_CHANNEL_COUNT;
	attr->info.frm_ptime = DEFAULT_PTIME;

	/* HE-AAC V2 */
	if (DEFAULT_AOT == AOT_PS) {
		if (DEFAULT_CHANNEL_COUNT == 1) {
			attr->info.avg_bps = 40000;
			attr->info.max_bps = 64000;
		}
		else {
			attr->info.avg_bps = 40000;
			attr->info.max_bps = 64000;
		}
	}
	/* AAC-LD Native */
	else if (DEFAULT_AOT == AOT_ER_AAC_LD) {

		// attr->info.avg_bps = 136000;
		attr->info.avg_bps = 147200;
		attr->info.max_bps = 260000;

		if (DEFAULT_BITRATE_MODE == 3) {
			attr->info.avg_bps = 136000;
			attr->info.max_bps = 384000;
		}
		else if (DEFAULT_BITRATE_MODE == 4) {
			attr->info.avg_bps = 136000;
			attr->info.max_bps = 384000;
		}
		else if (DEFAULT_BITRATE_MODE == 5) {
			attr->info.avg_bps = 136000;
			attr->info.max_bps = 384000;
		}

		attr->info.frm_ptime = 10;
		PJ_LOG(1, (THIS_FILE, "ENCODING TYPE : AAC-LD."));
	}
	/* ELD Native */
	else if (DEFAULT_AOT == AOT_ER_AAC_ELD && DEFAULT_USE_SBR == 0 && DEFAULT_IS_ELD_V2 == 0) {

		attr->info.avg_bps = 136000;
		attr->info.max_bps = 384000;

		attr->info.frm_ptime = 10;
		PJ_LOG(1, (THIS_FILE, "ENCODING TYPE : AAC-ELD."));
	}
	/* ELD Downsampled */
	else if (DEFAULT_AOT == AOT_ER_AAC_ELD && DEFAULT_USE_SBR == 1 && DEFAULT_SBR_RATIO == 1 && DEFAULT_IS_ELD_V2 == 0) {

		/* best setting for ELD Downsampled in constant rates */
		attr->info.avg_bps = 96000;
		attr->info.max_bps = 172000;
		if (DEFAULT_BITRATE_MODE == 3) {
			attr->info.avg_bps = 96000;
			attr->info.max_bps = 260000;
		}
		else if (DEFAULT_BITRATE_MODE == 4) {
			attr->info.avg_bps = 96000;
			attr->info.max_bps = 284000;

		}
		else if (DEFAULT_BITRATE_MODE == 5) {
			attr->info.avg_bps = 96000;
			attr->info.max_bps = 392000;

		}

		attr->info.frm_ptime = 10;
		PJ_LOG(1, (THIS_FILE, "ENCODING TYPE : AAC-ELD + SBR Downsampled."));
	}
	/* ELD Dualrate */
	else if (DEFAULT_AOT == AOT_ER_AAC_ELD && DEFAULT_USE_SBR == 1 && DEFAULT_SBR_RATIO == 2 && DEFAULT_IS_ELD_V2 == 0) {

		/* best setting for ELD Dualrate in constant rates */
		attr->info.avg_bps = 96000;
		attr->info.max_bps = 128000;

		if (DEFAULT_BITRATE_MODE == 3) {
			attr->info.avg_bps = 52000;
			attr->info.max_bps = 128000;
		}
		else if (DEFAULT_BITRATE_MODE == 4) {
			attr->info.avg_bps = 148000;
			attr->info.max_bps = 148000;
		}
		else if (DEFAULT_BITRATE_MODE == 5) {
			attr->info.avg_bps = 86000;
			attr->info.max_bps = 148000;
		}
		attr->info.frm_ptime = 20;
		PJ_LOG(1, (THIS_FILE, "ENCODING TYPE : AAC-ELD + SBR Dualrate."));
	}
	/* AAC-ELDv2 */
	else if (DEFAULT_AOT == AOT_ER_AAC_ELD && DEFAULT_USE_SBR == 1 && DEFAULT_ELDV2_SBR == 0 && DEFAULT_IS_ELD_V2 == 1) {
		attr->info.avg_bps = 85000;
		attr->info.max_bps = 192000;
		PJ_LOG(1, (THIS_FILE, "ENCODING TYPE : AAC-ELDv2."));
	}
	else if (DEFAULT_AOT == AOT_ER_AAC_ELD && DEFAULT_USE_SBR == 1 && DEFAULT_ELDV2_SBR == 1 && DEFAULT_IS_ELD_V2 == 1) {
		// this will only support constant bitrate, VBR_1, VBR_2
		attr->info.avg_bps = 32000;
		attr->info.max_bps = 128000;
		if (DEFAULT_BITRATE_MODE == 2) {
			attr->info.avg_bps = 64000;
			attr->info.max_bps = 64000;
		}
		else if (DEFAULT_BITRATE_MODE == 3) {
			attr->info.avg_bps = 32000;
			attr->info.max_bps = 192000;
		}

		PJ_LOG(1, (THIS_FILE, "ENCODING TYPE : AAC-ELDv2."));
	}

	attr->info.pcm_bits_per_sample = 16;
	attr->info.pt = (pj_uint8_t)id->pt;


	attr->setting.frm_per_pkt = 1;
	attr->setting.vad = 0;
	attr->setting.plc = DEFAULT_USE_INTERNAL_PLC;

	// Apply these settings to relevant fmtp parameters
	aac_apply_codec_params(aac_factory.pool, attr, &default_aac_params);

	return PJ_SUCCESS;
}

/*
* Enum codecs supported by this factory.
*/
static pj_status_t factory_enum_codecs(pjmedia_codec_factory *factory, unsigned *count, pjmedia_codec_info codecs[]) {
	unsigned max;
	int i; /* Must be signed */

	PJ_UNUSED_ARG(factory);
	PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

	max = *count;
	*count = 0;

	pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
	codecs[*count].encoding_name = pj_str("mpeg4-generic");
	// codecs[*count].pt = PJMEDIA_RTP_PT_MPEG4;
	codecs[*count].pt = PJMEDIA_RTP_PT_DYNAMIC;
	codecs[*count].type = PJMEDIA_TYPE_AUDIO;
	codecs[*count].clock_rate = DEFAULT_CLOCK_RATE;
	codecs[*count].channel_cnt = DEFAULT_CHANNEL_COUNT;

	++*count;

	return PJ_SUCCESS;
}

/*
* Allocate a new AAC codec instance.
*/
static pj_status_t factory_alloc_codec(pjmedia_codec_factory *factory, const pjmedia_codec_info *id, pjmedia_codec **p_codec) {
	pjmedia_codec *codec;
	pj_pool_t *pool;
	pj_status_t status;

	struct aac_private *aac;
	struct aac_factory *f = (struct aac_factory*) factory;

	PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
	PJ_ASSERT_RETURN(factory == &aac_factory.base, PJ_EINVAL);

	pool = pjmedia_endpt_create_pool(f->endpt, "aac", 512, 512);
	if (!pool) return PJ_ENOMEM;

	aac = PJ_POOL_ZALLOC_T(pool, struct aac_private);
	codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);

	status = pj_mutex_create_simple(pool, "aac_mutex", &aac->mutex);
	if (status != PJ_SUCCESS) {
		pj_pool_release(pool);
		return status;
	}

	// initialize default var
	aac->enc_ready = PJ_FALSE;
	aac->dec_ready = PJ_FALSE;
	// aac->min_peak = 1024;
	// aac->max_peak = 0;

	/* Create pool for codec instance */
	aac->pool = pool;
	codec->op = &aac_op;
	codec->factory = factory;
	codec->codec_data = aac;

	*p_codec = codec;
	return PJ_SUCCESS;
}

/*
* Free codec.
*/
static pj_status_t factory_dealloc_codec(pjmedia_codec_factory *factory,
	pjmedia_codec *codec) {
	struct aac_private *aac;

	PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
	PJ_UNUSED_ARG(factory);
	PJ_ASSERT_RETURN(factory == &aac_factory.base, PJ_EINVAL);
	aac = (struct aac_private*) codec->codec_data;

	if (aac) {

		if (aac->enc_ready == PJ_TRUE || aac->dec_ready == PJ_TRUE) {
			aac_codec_close(codec);
		}

		pj_mutex_destroy(aac->mutex);
		pj_pool_release(aac->pool);
		aac->mutex = NULL;
		aac->pool = NULL;
	}

	return PJ_SUCCESS;
}

/*
* Init codec.
*/
static pj_status_t aac_codec_init(pjmedia_codec *codec, pj_pool_t *pool) {
	PJ_UNUSED_ARG(codec);
	PJ_UNUSED_ARG(pool);
	return PJ_SUCCESS;
}

/*
* Open codec.
*/
static pj_status_t aac_codec_open(pjmedia_codec *codec, pjmedia_codec_param *attr) {
	AACENC_ERROR error;
	pj_status_t status;
	struct aac_private *aac = (struct aac_private*) codec->codec_data;
	int id, ret = 0, i;
	struct aac_encoder_params default_aac_params;
	AACENC_InfoStruct pInfo;
	const pj_str_t STR_FMTP_CONFIG = { "config", 6 };

	pj_assert(aac != NULL);
	pj_assert(aac->enc_ready == PJ_FALSE && aac->dec_ready == PJ_FALSE);

	/* Create Encoder */
	/* TODO : get from fmtp */
	default_aac_params.aot = DEFAULT_AOT;
	/* Assume only mono/stereo for now */
	if (DEFAULT_IS_ELD_V2 == 1) {
		default_aac_params.channel_mode = MODE_212;
	}
	else {
		default_aac_params.channel_mode = (attr->info.channel_cnt == 1) ? MODE_1 : MODE_2;
	}

	default_aac_params.granule_length = DEFAULT_GRANULE_LENGTH;
	default_aac_params.sbr = DEFAULT_USE_SBR;

	status = aac_encoder_open(&aac->hAacEnc, attr, &default_aac_params);
	PJ_LOG(1, (THIS_FILE, "AAC Encoder FrameLength: %d", aac->enc_frameLength));

	//status = aac_encoder_open(&aac->hAacEnc, 0, 2);
	aac->enc_ready = PJ_TRUE;

	/* Create Decoder */
	aac->hAacDec = aacDecoder_Open(DEFAULT_TRANSMUX, 1);
	UCHAR* configBuffers[2];
	UINT configBufferSizes[2] = { 0 };
	for (i = 0; i < attr->setting.enc_fmtp.cnt; ++i) {
		if (pj_stricmp(&attr->setting.enc_fmtp.param[i].name,
			&STR_FMTP_CONFIG) == 0) {
			pj_str_t value = attr->setting.enc_fmtp.param[i].val;
			configBufferSizes[0] = value.slen / 2;
			configBuffers[0] = pj_pool_alloc(aac->pool, configBufferSizes[0] * sizeof(UCHAR));
			for (i = 0; i < configBufferSizes[0]; i++) {
				unsigned v = 0;
				unsigned ptr_idx = 2 * i;
				if (ptr_idx < value.slen) {
					v |= (hex_digit_to_val(value.ptr[ptr_idx]) << 4);
				}
				ptr_idx++;
				if (ptr_idx < value.slen) {
					v |= (hex_digit_to_val(value.ptr[ptr_idx]));
				}
				configBuffers[0][i] = v;
			}
			break;
		}
	}
	aacDecoder_ConfigRaw(aac->hAacDec, configBuffers, configBufferSizes);

	// TODO
	aacEncInfo(aac->hAacEnc, &pInfo);
	aac->enc_frameLength = pInfo.frameLength;
	aac->dec_ready = PJ_TRUE;
	PJ_LOG(4, (THIS_FILE, "Decoder ready"));

	aac->channel_cnt = attr->info.channel_cnt;
	aac->clock_rate = attr->info.clock_rate;
	aac->pcm_frame_size = (attr->info.channel_cnt * attr->info.clock_rate
		* attr->info.pcm_bits_per_sample * attr->info.frm_ptime) / 8000;

	return PJ_SUCCESS;
}

/*
* Close codec.
*/
static pj_status_t aac_codec_close(pjmedia_codec *codec) {
	struct aac_private *aac = (struct aac_private*) codec->codec_data;
	int i;
	PJ_UNUSED_ARG(codec);

	aacEncClose(&aac->hAacEnc);
	aac->enc_ready = PJ_FALSE;

	aacDecoder_Close(aac->hAacDec);
	aac->dec_ready = PJ_FALSE;

	return PJ_SUCCESS;
}

/*
* Modify codec settings.
*/
static pj_status_t aac_codec_modify(pjmedia_codec *codec,
	const pjmedia_codec_param *attr) {

	PJ_UNUSED_ARG(codec);
	PJ_UNUSED_ARG(attr);

	return PJ_SUCCESS;
}

/*
* Encode frame.
*/
static pj_status_t aac_codec_encode(pjmedia_codec *codec,
	const struct pjmedia_frame *input, unsigned output_buf_len,
	struct pjmedia_frame *output) {
	struct aac_private *aac = (struct aac_private*) codec->codec_data;

	AACENC_ERROR       error = AACENC_OK;
	AACENC_InfoStruct pInfo;

	unsigned processed_buffer = 0;
	unsigned buffer_size = 0;
	unsigned out_bytes = 0;
	PJ_ASSERT_RETURN(codec && input && output, PJ_EINVAL);
	pj_mutex_lock(aac->mutex);


	aacEncInfo(aac->hAacEnc, &pInfo);
	//PJ_LOG(1, (THIS_FILE, "Current frameLength: %d, input: %d", pInfo.frameLength, input->size));
	buffer_size = aac->channel_cnt * 2 * pInfo.frameLength;

	pj_uint8_t* p;
	pj_uint8_t* sp;

	p = (pj_uint8_t*)output->buf;
	*p++ = 0;
	*p++ = 16;
	sp = p;
	p++; p++;

	while (processed_buffer < (input->size / 2) && error == AACENC_OK) {

		static AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
		static AACENC_InArgs  in_args = { 0 };
		static AACENC_OutArgs out_args = { 0 };
		int in_buffer_identifier = IN_AUDIO_DATA;
		int in_buffer_size, in_buffer_element_size;
		int out_buffer_identifier = OUT_BITSTREAM_DATA;
		int out_buffer_size, out_buffer_element_size;
		char *in_ptr, *out_ptr;

		in_ptr = input->buf;
		for (unsigned i = 0; i < processed_buffer; i++) {
			in_ptr++;
		}
		// in_ptr = processed_buffer;
		in_buffer_size = input->size - (processed_buffer * aac->channel_cnt);
		// PJ_LOG(1, (THIS_FILE, "CALC in_buffer_size: %d", in_buffer_size));
		in_buffer_element_size = 2;

		in_args.numInSamples = in_buffer_size / 2;
		in_buf.numBufs = 1;
		in_buf.bufs = &in_ptr;
		in_buf.bufferIdentifiers = &in_buffer_identifier;
		in_buf.bufSizes = &in_buffer_size;
		in_buf.bufElSizes = &in_buffer_element_size;

		// p = out_bytes;
		out_ptr = p;
		for (unsigned i = 0; i < out_bytes; i++) {
			out_ptr++;
		}

		out_buffer_size = output_buf_len - 4;
		out_buffer_element_size = 1;
		out_buf.numBufs = 1;
		out_buf.bufs = &out_ptr;
		out_buf.bufferIdentifiers = &out_buffer_identifier;
		out_buf.bufSizes = &out_buffer_size;
		out_buf.bufElSizes = &out_buffer_element_size;

		error = aacEncEncode(aac->hAacEnc, &in_buf, &out_buf, &in_args, &out_args);
		// PJ_LOG(2, (THIS_FILE, "Outbytes: %d", out_args.numOutBytes));
		if (error == AACENC_OK) {
			if (out_args.numOutBytes > 0) {
				processed_buffer += out_args.numInSamples;
				out_bytes += out_args.numOutBytes;
				// PJ_LOG(2, (THIS_FILE, "Encode : numInSamples: %d, numOutBytes %d", processed_buffer, out_bytes));
			}
			else {
				PJ_LOG(2, (THIS_FILE, "AAC Encoder had %d (%d) > %d", out_args.numInSamples, input->size, out_args.numOutBytes));
				break;
			}
		}
		else {
			PJ_LOG(1, (THIS_FILE, "AAC Encoder error %d - inSize %d", error, input->size));
			break;
		}
	}

	output->type = PJMEDIA_FRAME_TYPE_NONE;
	output->size = 0;
	output->timestamp = input->timestamp;

	if (error == AACENC_OK) {
		if (out_bytes > 0) {
			pj_uint16_t size = (out_bytes << 3);
			output->size = out_bytes + 4;
			output->type = PJMEDIA_FRAME_TYPE_AUDIO;
			*sp++ = (size & 0xFF00) >> 8;
			*sp = (size & 0x00FF);
			// this code just for finding max_peak bitrate and min_peak_bitrate
			// if ((out_bytes + 4) < aac->min_peak) {
			//	aac->min_peak = (out_bytes + 4);
			//	PJ_LOG(2, (THIS_FILE, "min_peak: %d, max_peak: %d", aac->min_peak, aac->max_peak));
			// }
			// if ((out_bytes + 4) > aac->max_peak) {
			//	aac->max_peak = (out_bytes + 4);
			// 	PJ_LOG(2, (THIS_FILE, "min_peak: %d, max_peak: %d", aac->min_peak, aac->max_peak));
			// }
		}
	}

	pj_mutex_unlock(aac->mutex);
	return PJ_SUCCESS;
}

/*
* Get frames in the packet.
*/
static pj_status_t aac_codec_parse(pjmedia_codec *codec, void *pkt,
	pj_size_t pkt_size, const pj_timestamp *ts, unsigned *frame_cnt,
	pjmedia_frame frames[]) {
	struct aac_private *aac = (struct aac_private*) codec->codec_data;
	unsigned i;
	pj_uint8_t* p;
	pj_uint16_t au_headers_length = 0, au_header = 0, au_size = 0;

	PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);

	pj_mutex_lock(aac->mutex);

	p = (pj_uint8_t*)pkt;
	// For now just do not use AU-headers-length should be done later
	au_headers_length |= (*p++ << 8);
	au_headers_length |= (*p++);
	if (au_headers_length != 16) {
		*frame_cnt = 0;
		PJ_LOG(1, (THIS_FILE, "Unsupported packet for now %d", au_headers_length));
		pj_mutex_unlock(aac->mutex);
		return PJMEDIA_CODEC_EFAILED;
	}
	au_header |= (*p++ << 8);
	au_header |= (*p++);
	au_size = (au_header >> 3); /* 16bits of headers with 13 of size and 3 of index (assume 0 for now) */

	if (au_size > pkt_size - 4) {
		PJ_LOG(1, (THIS_FILE, "Truncated packet or invalid size %d - %d", au_size, pkt_size - 4));
		pj_mutex_unlock(aac->mutex);
		return PJMEDIA_CODEC_EFAILED;
	}
	//PJ_LOG(4, (THIS_FILE, "Parsed packet : size %d", au_size));

	// Assume just one frame per packet for now
	*frame_cnt = 1;
	frames[0].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[0].bit_info = 0;
	frames[0].buf = p;
	frames[0].size = au_size;
	frames[0].timestamp.u64 = ts->u64;

	pj_mutex_unlock(aac->mutex);
	return PJ_SUCCESS;
}

static pj_status_t aac_codec_decode(pjmedia_codec *codec,
	const struct pjmedia_frame *input, unsigned output_buf_len,
	struct pjmedia_frame *output) {
	struct aac_private *aac = (struct aac_private*) codec->codec_data;
	int bytesValid;
	AAC_DECODER_ERROR       error;
	UCHAR* p;

	PJ_ASSERT_RETURN(codec && input && output_buf_len && output, PJ_EINVAL);
	pj_mutex_lock(aac->mutex);


	p = (UCHAR*)input->buf;
	bytesValid = input->size;

	error = aacDecoder_Fill(aac->hAacDec, &p, &input->size, &bytesValid);
	if (error != AAC_DEC_OK) {
		PJ_LOG(1, (THIS_FILE, "Error while filling decoder buffer %d", error));
		output->type = PJMEDIA_TYPE_NONE;
		output->buf = NULL;
		output->size = 0;
		pj_mutex_unlock(aac->mutex);
		return PJMEDIA_CODEC_EFAILED;
	}

	/* Normal Decoding process */
	error = aacDecoder_DecodeFrame(aac->hAacDec, output->buf, output_buf_len, 0);
	if (error != AAC_DEC_OK) {
		/* Do Conceal if stream coupted*/
		// PJ_LOG(1, (THIS_FILE, "Conceal Decoding Stream: %d, out_len: %d", error, output_buf_len));
		aacDecoder_DecodeFrame(aac->hAacDec, output->buf, output_buf_len, 1);
		if (aac->pcm_frame_size != output_buf_len) {
			output->type = PJMEDIA_TYPE_NONE;
			output->buf = NULL;
			output->size = 0;
			pj_mutex_unlock(aac->mutex);
			return PJ_SUCCESS;
		}
	}

	output->size = aac->pcm_frame_size;
	output->type = PJMEDIA_TYPE_AUDIO;
	output->timestamp = input->timestamp;
	pj_mutex_unlock(aac->mutex);
	return PJ_SUCCESS;
}

/*
* Recover lost frame.
*/
static pj_status_t aac_codec_recover(pjmedia_codec *codec,
	unsigned output_buf_len, struct pjmedia_frame *output) {
	struct aac_private *aac = (struct aac_private*) codec->codec_data;
	int ret = 0;
	int frame_size;
	pjmedia_frame *fec_frame;
	AAC_DECODER_ERROR error;

	PJ_ASSERT_RETURN(output, PJ_EINVAL);
	pj_mutex_lock(aac->mutex);

	/* Do Conceal if stream coupted*/
	error = aacDecoder_DecodeFrame(aac->hAacDec, output->buf, output_buf_len, 1);
	if (error != AAC_DEC_OK && output_buf_len != aac->pcm_frame_size) {
		output->type = PJMEDIA_TYPE_NONE;
		output->buf = NULL;
		output->size = 0;
		pj_mutex_unlock(aac->mutex);
		return PJ_SUCCESS;
	}

	output->size = output_buf_len;
	output->type = PJMEDIA_TYPE_AUDIO;
	//output->timestamp = input->timestamp;
	pj_mutex_unlock(aac->mutex);
	return PJ_SUCCESS;
}

#endif