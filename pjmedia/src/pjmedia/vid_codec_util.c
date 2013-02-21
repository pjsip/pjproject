/* $Id$ */
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
#include <pjmedia/vid_codec_util.h>
#include <pjmedia/errno.h>
#include <pjmedia/stream_common.h>
#include <pjlib-util/base64.h>
#include <pj/ctype.h>
#include <pj/math.h>


#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)


#define THIS_FILE   "vid_codec_util.c"

/* If this is set to non-zero, H.264 custom negotiation will require
 * "profile-level-id" and "packetization-mode" to be exact match to
 * get a successful negotiation. Note that flexible answer (updating
 * SDP answer to match remote offer) is always active regardless the
 * value of this macro.
 */
#define H264_STRICT_SDP_NEGO	    0

/* Default frame rate, if not specified */
#define DEFAULT_H264_FPS_NUM	    15
#define DEFAULT_H264_FPS_DENUM	    1

/* Default aspect ratio, if not specified */
#define DEFAULT_H264_RATIO_NUM	    15
#define DEFAULT_H264_RATIO_DENUM    1


/* ITU resolution definition */
struct mpi_resolution_t
{
    pj_str_t		name;    
    pjmedia_rect_size	size;
}
mpi_resolutions [] =
{
    {{"CIF",3},     {352,288}},
    {{"QCIF",4},    {176,144}},
    {{"SQCIF",5},   {88,72}},
    {{"CIF4",4},    {704,576}},
    {{"CIF16",5},   {1408,1142}},
};


#define CALC_H264_MB_NUM(size) (((size.w+15)/16)*((size.h+15)/16))
#define CALC_H264_MBPS(size,fps) CALC_H264_MB_NUM(size)*fps.num/fps.denum


/* Parse fmtp value for custom resolution, e.g: "CUSTOM=800,600,2" */
static pj_status_t parse_custom_res_fmtp(const pj_str_t *fmtp_val,
					 pjmedia_rect_size *size,
					 unsigned *mpi)
{
    const char *p, *p_end;
    pj_str_t token;
    unsigned long val[3] = {0};
    unsigned i = 0;

    p = token.ptr = fmtp_val->ptr;
    p_end = p + fmtp_val->slen;

    while (p<=p_end && i<PJ_ARRAY_SIZE(val)) {
	if (*p==',' || p==p_end) {
	    token.slen = (char*)p - token.ptr;
	    val[i++] = pj_strtoul(&token);
	    token.ptr = (char*)p+1;
	}
	++p;
    }

    if (!val[0] || !val[1])
	return PJ_ETOOSMALL;

    if (val[2]<1 || val[2]>32)
	return PJ_EINVAL;

    size->w = val[0];
    size->h = val[1];
    *mpi = val[2];
    return PJ_SUCCESS;
}


/* H263 fmtp parser */
PJ_DEF(pj_status_t) pjmedia_vid_codec_parse_h263_fmtp(
				    const pjmedia_codec_fmtp *fmtp,
				    pjmedia_vid_codec_h263_fmtp *h263_fmtp)
{
    const pj_str_t CUSTOM = {"CUSTOM", 6};
    unsigned i;

    pj_bzero(h263_fmtp, sizeof(*h263_fmtp));

    for (i=0; i<fmtp->cnt; ++i) {
	unsigned j;
	pj_bool_t parsed = PJ_FALSE;

	if (h263_fmtp->mpi_cnt >= PJ_ARRAY_SIZE(h263_fmtp->mpi)) {
	    pj_assert(!"Too small MPI array in H263 fmtp");
	    continue;
	}

	/* Standard size MPIs */
	for (j=0; j<PJ_ARRAY_SIZE(mpi_resolutions) && !parsed; ++j) {
	    if (pj_stricmp(&fmtp->param[i].name, &mpi_resolutions[j].name)==0)
	    {
		unsigned mpi;

		mpi = pj_strtoul(&fmtp->param[i].val);
		if (mpi<1 || mpi>32)
		    return PJMEDIA_SDP_EINFMTP;

		h263_fmtp->mpi[h263_fmtp->mpi_cnt].size = 
						    mpi_resolutions[j].size;
		h263_fmtp->mpi[h263_fmtp->mpi_cnt].val = mpi;
		++h263_fmtp->mpi_cnt;
		parsed = PJ_TRUE;
	    }
	}
	if (parsed)
	    continue;

	/* Custom size MPIs */
	if (pj_stricmp(&fmtp->param[i].name, &CUSTOM)==0) {
	    pjmedia_rect_size size;
	    unsigned mpi;
	    pj_status_t status;

	    status = parse_custom_res_fmtp(&fmtp->param[i].val, &size, &mpi);
	    if (status != PJ_SUCCESS)
		return PJMEDIA_SDP_EINFMTP;

	    h263_fmtp->mpi[h263_fmtp->mpi_cnt].size = size;
	    h263_fmtp->mpi[h263_fmtp->mpi_cnt].val = mpi;
	    ++h263_fmtp->mpi_cnt;
	}
    }

    return PJ_SUCCESS;
}


static unsigned fps_to_mpi(const pjmedia_ratio *fps) 
{
    unsigned mpi;

    /* Original formula = (fps->denum * 30000) / (fps->num * 1001) */
    mpi = (fps->denum*30000 + fps->num*1001/2) / (fps->num*1001);
    
    /* Normalize, should be in the range of 1-32 */
    if (mpi > 32) mpi = 32;
    if (mpi < 1) mpi = 1;

    return mpi;
};

PJ_DEF(pj_status_t) pjmedia_vid_codec_h263_apply_fmtp(
				pjmedia_vid_codec_param *param)
{
    if (param->dir & PJMEDIA_DIR_ENCODING) {
	pjmedia_vid_codec_h263_fmtp fmtp_loc, fmtp_rem;
	pjmedia_rect_size size = {0};
	unsigned mpi = 0;
	pjmedia_video_format_detail *vfd;
	pj_status_t status;

	vfd = pjmedia_format_get_video_format_detail(&param->enc_fmt,
						     PJ_TRUE);

	/* Get local param */
	// Local param should be fetched from "param->enc_fmt" instead of
	// "param->dec_fmtp".
	//status = pjmedia_vid_codec_parse_h263_fmtp(&param->dec_fmtp,
	//					   &fmtp_loc);
	//if (status != PJ_SUCCESS)
	//    return status;
	fmtp_loc.mpi_cnt = 1;
	fmtp_loc.mpi[0].size = vfd->size;
	fmtp_loc.mpi[0].val  = fps_to_mpi(&vfd->fps);

	/* Get remote param */
	status = pjmedia_vid_codec_parse_h263_fmtp(&param->enc_fmtp,
						   &fmtp_rem);
	if (status != PJ_SUCCESS)
	    return status;

	/* Negotiate size & MPI setting */
	if (fmtp_rem.mpi_cnt == 0) {
	    /* Remote doesn't specify MPI setting, send QCIF=1 */
	    size.w = 176;
	    size.h = 144;
	    mpi	   = 1;
	//} else if (fmtp_loc.mpi_cnt == 0) {
	//    /* Local MPI setting not set, just use remote preference. */
	//    size = fmtp_rem.mpi[0].size;
	//    mpi  = fmtp_rem.mpi[0].val;
	} else {
	    /* Both have preferences, let's try to match them */
	    unsigned i, j;
	    pj_bool_t matched = PJ_FALSE;
	    pj_uint32_t min_diff = 0xFFFFFFFF;
	    pj_uint32_t loc_sq, rem_sq, diff;

	    /* Find the exact size match or the closest size, then choose
	     * the highest MPI among the match/closest pair.
	     */
	    for (i = 0; i < fmtp_rem.mpi_cnt && !matched; ++i) {
		rem_sq = fmtp_rem.mpi[i].size.w * fmtp_rem.mpi[i].size.h;
		for (j = 0; j < fmtp_loc.mpi_cnt; ++j) {
		    /* See if we got exact match */
		    if (fmtp_rem.mpi[i].size.w == fmtp_loc.mpi[j].size.w &&
			fmtp_rem.mpi[i].size.h == fmtp_loc.mpi[j].size.h)
		    {
			size = fmtp_rem.mpi[i].size;
			mpi  = PJ_MAX(fmtp_rem.mpi[i].val,
				      fmtp_loc.mpi[j].val);
			matched = PJ_TRUE;
			break;
		    }

		    /* Otherwise keep looking for the closest match */
		    loc_sq = fmtp_loc.mpi[j].size.w * fmtp_loc.mpi[j].size.h;
		    diff = loc_sq>rem_sq? (loc_sq-rem_sq):(rem_sq-loc_sq);
		    if (diff < min_diff) {
			size = rem_sq<loc_sq? fmtp_rem.mpi[i].size :
					      fmtp_loc.mpi[j].size;
			mpi  = PJ_MAX(fmtp_rem.mpi[i].val,
				      fmtp_loc.mpi[j].val);
		    }
		}
	    }
	}

	/* Apply the negotiation result */
	vfd->size = size;
	vfd->fps.num = 30000;
	vfd->fps.denum = 1001 * mpi;
    }

    if (param->dir & PJMEDIA_DIR_DECODING) {
	/* Here we just want to find the highest resolution and the lowest MPI
	 * we support and set it as the decoder param.
	 */
	pjmedia_vid_codec_h263_fmtp fmtp;
	pjmedia_video_format_detail *vfd;
	pj_status_t status;
	
	status = pjmedia_vid_codec_parse_h263_fmtp(&param->dec_fmtp,
						   &fmtp);
	if (status != PJ_SUCCESS)
	    return status;

	vfd = pjmedia_format_get_video_format_detail(&param->dec_fmt,
						     PJ_TRUE);

	if (fmtp.mpi_cnt == 0) {
	    /* No resolution specified, lets just assume 4CIF=1! */
	    vfd->size.w = 704;
	    vfd->size.h = 576;
	    vfd->fps.num = 30000;
	    vfd->fps.denum = 1001;
	} else {
	    unsigned i, max_size = 0, max_size_idx = 0, min_mpi = 32;
	    
	    /* Get the largest size and the lowest MPI */
	    for (i = 0; i < fmtp.mpi_cnt; ++i) {
		if (fmtp.mpi[i].size.w * fmtp.mpi[i].size.h > max_size) {
		    max_size = fmtp.mpi[i].size.w * fmtp.mpi[i].size.h;
		    max_size_idx = i;
		}
		if (fmtp.mpi[i].val < min_mpi)
		    min_mpi = fmtp.mpi[i].val;
	    }

	    vfd->size = fmtp.mpi[max_size_idx].size;
	    vfd->fps.num = 30000;
	    vfd->fps.denum = 1001 * min_mpi;
	}
    }

    return PJ_SUCCESS;
}


/* Declaration of H.264 level info */
typedef struct h264_level_info_t
{
    unsigned id;	    /* Level id.			*/
    unsigned max_mbps;	    /* Max macroblocks per second.	*/
    unsigned max_mb;	    /* Max macroblocks.			*/
    unsigned max_br;	    /* Max bitrate (kbps).		*/
} h264_level_info_t;


/* Init H264 parameters based on profile-level-id */
static pj_status_t init_h264_profile(const pj_str_t *profile,
				     pjmedia_vid_codec_h264_fmtp *fmtp)
{
    const h264_level_info_t level_info[] =
    {
	{ 10,   1485,    99,     64 },
	{ 9,    1485,    99,    128 }, /*< level 1b */
	{ 11,   3000,   396,    192 },
	{ 12,   6000,   396,    384 },
	{ 13,  11880,   396,    768 },
	{ 20,  11880,   396,   2000 },
	{ 21,  19800,   792,   4000 },
	{ 22,  20250,  1620,   4000 },
	{ 30,  40500,  1620,  10000 },
	{ 31, 108000,  3600,  14000 },
	{ 32, 216000,  5120,  20000 },
	{ 40, 245760,  8192,  20000 },
	{ 41, 245760,  8192,  50000 },
	{ 42, 522240,  8704,  50000 },
	{ 50, 589824, 22080, 135000 },
	{ 51, 983040, 36864, 240000 },
    };
    unsigned i, tmp;
    pj_str_t endst;
    const h264_level_info_t *li = NULL;

    if (profile->slen != 6)
	return PJMEDIA_SDP_EINFMTP;

    tmp = pj_strtoul2(profile, &endst, 16);
    if (endst.slen)
	return PJMEDIA_SDP_EINFMTP;

    fmtp->profile_idc = (pj_uint8_t)((tmp >> 16) & 0xFF);
    fmtp->profile_iop = (pj_uint8_t)((tmp >> 8) & 0xFF);
    fmtp->level = (pj_uint8_t)(tmp & 0xFF);

    for (i = 0; i < PJ_ARRAY_SIZE(level_info); ++i) {
	if (level_info[i].id == fmtp->level) {
	    li = &level_info[i];
	    break;
	}
    }
    if (li == NULL)
	return PJMEDIA_SDP_EINFMTP;

    /* Init profile level spec */
    if (fmtp->max_br == 0)
	fmtp->max_br = li->max_br;
    if (fmtp->max_mbps == 0)
	fmtp->max_mbps = li->max_mbps;
    if (fmtp->max_fs == 0)
	fmtp->max_fs = li->max_mb;

    return PJ_SUCCESS;
}


/* H264 fmtp parser */
PJ_DEF(pj_status_t) pjmedia_vid_codec_h264_parse_fmtp(
				    const pjmedia_codec_fmtp *fmtp,
				    pjmedia_vid_codec_h264_fmtp *h264_fmtp)
{
    const pj_str_t PROFILE_LEVEL_ID	= {"profile-level-id", 16};
    const pj_str_t MAX_MBPS		= {"max-mbps", 8};
    const pj_str_t MAX_FS		= {"max-fs", 6};
    const pj_str_t MAX_CPB		= {"max-cpb", 7};
    const pj_str_t MAX_DPB	    	= {"max-dpb", 7};
    const pj_str_t MAX_BR		= {"max-br", 6};
    const pj_str_t PACKETIZATION_MODE	= {"packetization-mode", 18};
    const pj_str_t SPROP_PARAMETER_SETS = {"sprop-parameter-sets", 20};
    unsigned i;
    pj_status_t status;

    pj_bzero(h264_fmtp, sizeof(*h264_fmtp));

    for (i=0; i<fmtp->cnt; ++i) {
	unsigned tmp;
	if (pj_stricmp(&fmtp->param[i].name, &PROFILE_LEVEL_ID)==0) {
	    /* Init H264 parameters based on level, if not set yet */
	    status = init_h264_profile(&fmtp->param[i].val, h264_fmtp);
	    if (status != PJ_SUCCESS)
		return status;
	} else if (pj_stricmp(&fmtp->param[i].name, &PACKETIZATION_MODE)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    if (tmp >= 0 && tmp <= 2) 
		h264_fmtp->packetization_mode = (pj_uint8_t)tmp;
	    else
		return PJMEDIA_SDP_EINFMTP;
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_MBPS)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    h264_fmtp->max_mbps = PJ_MAX(tmp, h264_fmtp->max_mbps);
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_FS)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    h264_fmtp->max_fs = PJ_MAX(tmp, h264_fmtp->max_fs);
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_CPB)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    h264_fmtp->max_cpb = PJ_MAX(tmp, h264_fmtp->max_cpb);
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_DPB)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    h264_fmtp->max_dpb = PJ_MAX(tmp, h264_fmtp->max_dpb);
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_BR)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    h264_fmtp->max_br = PJ_MAX(tmp, h264_fmtp->max_br);
	} else if (pj_stricmp(&fmtp->param[i].name, &SPROP_PARAMETER_SETS)==0)
	{
	    pj_str_t sps_st;

	    sps_st = fmtp->param[i].val;
	    while (sps_st.slen) {
		pj_str_t tmp_st;
		int tmp_len;
		const pj_uint8_t start_code[3] = {0, 0, 1};
		char *p;
		pj_uint8_t *nal;
		pj_status_t status;

		/* Find field separator ',' */
		tmp_st = sps_st;
		p = pj_strchr(&sps_st, ',');
		if (p) {
		    tmp_st.slen = p - sps_st.ptr;
		    sps_st.ptr  = p+1;
		    sps_st.slen -= (tmp_st.slen+1);
		} else {
		    sps_st.slen = 0;
		}

		/* Decode field and build NAL unit for this param */
		nal = &h264_fmtp->sprop_param_sets[
					  h264_fmtp->sprop_param_sets_len];
		tmp_len = PJ_ARRAY_SIZE(h264_fmtp->sprop_param_sets) -
			  h264_fmtp->sprop_param_sets_len -
			  PJ_ARRAY_SIZE(start_code);
		status = pj_base64_decode(&tmp_st,
					  nal + PJ_ARRAY_SIZE(start_code),
					  &tmp_len);
		if (status != PJ_SUCCESS)
		    return PJMEDIA_SDP_EINFMTP;

		tmp_len += PJ_ARRAY_SIZE(start_code);
		pj_memcpy(nal, start_code, PJ_ARRAY_SIZE(start_code));
		h264_fmtp->sprop_param_sets_len += tmp_len;
	    }
	}
    }

    /* When profile-level-id is not specified, use default value "42000A" */
    if (h264_fmtp->profile_idc == 0) {
	h264_fmtp->profile_idc = 0x42;
	h264_fmtp->profile_iop = 0x00;
	h264_fmtp->level = 0x0A;
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_vid_codec_h264_match_sdp(pj_pool_t *pool,
						     pjmedia_sdp_media *offer,
						     unsigned o_fmt_idx,
						     pjmedia_sdp_media *answer,
						     unsigned a_fmt_idx,
						     unsigned option)
{
    const pj_str_t PROFILE_LEVEL_ID	= {"profile-level-id", 16};
    const pj_str_t PACKETIZATION_MODE	= {"packetization-mode", 18};
    pjmedia_codec_fmtp o_fmtp_raw, a_fmtp_raw;
    pjmedia_vid_codec_h264_fmtp o_fmtp, a_fmtp;
    pj_status_t status;

    PJ_UNUSED_ARG(pool);

    /* Parse offer */
    status = pjmedia_stream_info_parse_fmtp(
				    NULL, offer, 
				    pj_strtoul(&offer->desc.fmt[o_fmt_idx]),
				    &o_fmtp_raw);
    if (status != PJ_SUCCESS)
	return status;

    status = pjmedia_vid_codec_h264_parse_fmtp(&o_fmtp_raw, &o_fmtp);
    if (status != PJ_SUCCESS)
	return status;

    /* Parse answer */
    status = pjmedia_stream_info_parse_fmtp(
				    NULL, answer, 
				    pj_strtoul(&answer->desc.fmt[a_fmt_idx]),
				    &a_fmtp_raw);
    if (status != PJ_SUCCESS)
	return status;

    status = pjmedia_vid_codec_h264_parse_fmtp(&a_fmtp_raw, &a_fmtp);
    if (status != PJ_SUCCESS)
	return status;

    if (option & PJMEDIA_SDP_NEG_FMT_MATCH_ALLOW_MODIFY_ANSWER) {
	unsigned i;

	/* Flexible negotiation, if the answer has higher capability than
	 * the offer, adjust the answer capability to be match to the offer.
	 */
	if (a_fmtp.profile_idc >= o_fmtp.profile_idc)
	    a_fmtp.profile_idc = o_fmtp.profile_idc;
	if (a_fmtp.profile_iop != o_fmtp.profile_iop)
	    a_fmtp.profile_iop = o_fmtp.profile_iop;
	if (a_fmtp.level >= o_fmtp.level)
	    a_fmtp.level = o_fmtp.level;
	if (a_fmtp.packetization_mode >= o_fmtp.packetization_mode)
	    a_fmtp.packetization_mode = o_fmtp.packetization_mode;

	/* Match them now */
#if H264_STRICT_SDP_NEGO
	if (a_fmtp.profile_idc != o_fmtp.profile_idc ||
	    a_fmtp.profile_iop != o_fmtp.profile_iop ||
	    a_fmtp.level != o_fmtp.level ||
	    a_fmtp.packetization_mode != o_fmtp.packetization_mode)
	{
	    return PJMEDIA_SDP_EFORMATNOTEQUAL;
	}
#else
	if (a_fmtp.profile_idc != o_fmtp.profile_idc)
	{
	    return PJMEDIA_SDP_EFORMATNOTEQUAL;
	}
#endif

	/* Update the answer */
	for (i = 0; i < a_fmtp_raw.cnt; ++i) {
	    if (pj_stricmp(&a_fmtp_raw.param[i].name, &PROFILE_LEVEL_ID) == 0)
	    {
		char *p = a_fmtp_raw.param[i].val.ptr;
		pj_val_to_hex_digit(a_fmtp.profile_idc, p);
		p += 2;
		pj_val_to_hex_digit(a_fmtp.profile_iop, p);
		p += 2;
		pj_val_to_hex_digit(a_fmtp.level, p);
	    }
	    else if (pj_stricmp(&a_fmtp_raw.param[i].name, &PACKETIZATION_MODE) == 0)
	    {
		char *p = a_fmtp_raw.param[i].val.ptr;
		*p = '0' + a_fmtp.packetization_mode;
	    }
	}
    } else {
#if H264_STRICT_SDP_NEGO
	/* Strict negotiation */
	if (a_fmtp.profile_idc != o_fmtp.profile_idc ||
	    a_fmtp.profile_iop != o_fmtp.profile_iop ||
	    a_fmtp.level != o_fmtp.level ||
	    a_fmtp.packetization_mode != o_fmtp.packetization_mode)
	{
	    return PJMEDIA_SDP_EFORMATNOTEQUAL;
	}
#else
	/* Permissive negotiation */
	if (a_fmtp.profile_idc != o_fmtp.profile_idc)
	{
	    return PJMEDIA_SDP_EFORMATNOTEQUAL;
	}
#endif
    }

    return PJ_SUCCESS;
}


/* Find greatest common divisor (GCD) */
static unsigned gcd (unsigned a, unsigned b) {
    unsigned c;
    while (b) {
	c = a % b;
	a = b;
	b = c;
    }
    return a;
}

/* Find highest resolution possible for the specified H264 fmtp and
 * aspect ratio.
 */
static pj_status_t find_highest_res(pjmedia_vid_codec_h264_fmtp *fmtp,
				    const pjmedia_ratio *fps,
				    const pjmedia_ratio *ratio,
				    pjmedia_rect_size *size)
{
    pjmedia_ratio def_ratio = { DEFAULT_H264_RATIO_NUM,
			        DEFAULT_H264_RATIO_DENUM };
    pjmedia_ratio def_fps   = { DEFAULT_H264_FPS_NUM,
			        DEFAULT_H264_FPS_DENUM };
    pjmedia_ratio asp_ratio, the_fps;
    unsigned max_fs, g, scale;

    pj_assert(size);

    /* Get the ratio, or just use default if not provided. */
    if (ratio && ratio->num && ratio->denum) {
	asp_ratio = *ratio;
    } else {
	asp_ratio = def_ratio;
    }

    /* Normalize the aspect ratio */
    g = gcd(asp_ratio.num, asp_ratio.denum);
    asp_ratio.num /= g;
    asp_ratio.denum /= g;

    /* Get the frame rate, or just use default if not provided. */
    if (fps && fps->num && fps->denum) {
	the_fps = *fps;
    } else {
	the_fps = def_fps;
    }

    /* Calculate maximum size (in macroblocks) */
    max_fs = fmtp->max_mbps * fps->denum / fps->num;
    max_fs = PJ_MIN(max_fs, fmtp->max_fs);

    /* Check if the specified ratio is using big numbers
     * (not normalizable), override it with default ratio!
     */
    if ((int)max_fs < asp_ratio.num * asp_ratio.denum)
	asp_ratio = def_ratio;

    /* Calculate the scale factor for size */
    scale = pj_isqrt(max_fs / asp_ratio.denum / asp_ratio.num);

    /* Calculate the size, note that the frame size is in macroblock units */
    size->w = asp_ratio.num   * scale * 16;
    size->h = asp_ratio.denum * scale * 16;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_vid_codec_h264_apply_fmtp(
				pjmedia_vid_codec_param *param)
{
    if (param->dir & PJMEDIA_DIR_ENCODING) {
	pjmedia_vid_codec_h264_fmtp fmtp;
	pjmedia_video_format_detail *vfd;
	pj_status_t status;

	/* Get remote param */
	status = pjmedia_vid_codec_h264_parse_fmtp(&param->enc_fmtp,
						   &fmtp);
	if (status != PJ_SUCCESS)
	    return status;

	/* Adjust fps, size, and bitrate to conform to H.264 level
	 * specified by remote SDP fmtp.
	 */
	vfd = pjmedia_format_get_video_format_detail(&param->enc_fmt,
						     PJ_TRUE);

	if (vfd->fps.num == 0 || vfd->fps.denum == 0) {
	    vfd->fps.num   = DEFAULT_H264_FPS_NUM;
	    vfd->fps.denum = DEFAULT_H264_FPS_DENUM;
	}

	if (vfd->size.w && vfd->size.h) {
	    unsigned mb, mbps;
	    
	    /* Scale down the resolution if it exceeds profile spec */
	    mb = CALC_H264_MB_NUM(vfd->size);
	    mbps = CALC_H264_MBPS(vfd->size, vfd->fps);
	    if (mb > fmtp.max_fs || mbps > fmtp.max_mbps) {
		pjmedia_ratio r;
		r.num = vfd->size.w;
		r.denum = vfd->size.h;
		find_highest_res(&fmtp, &vfd->fps, &r, &vfd->size);
	    }
	} else {
	    /* When not specified, just use the highest res possible*/
	    pjmedia_ratio r;
	    r.num = vfd->size.w;
	    r.denum = vfd->size.h;
	    find_highest_res(&fmtp, &vfd->fps, &r, &vfd->size);
	}

	/* Encoding bitrate must not be higher than H264 level spec */
	if (vfd->avg_bps > fmtp.max_br * 1000)
	    vfd->avg_bps = fmtp.max_br * 1000;
	if (vfd->max_bps > fmtp.max_br * 1000)
	    vfd->max_bps = fmtp.max_br * 1000;
    }

    if (param->dir & PJMEDIA_DIR_DECODING) {
	/* Here we just want to find the highest resolution possible from the
	 * fmtp and set it as the decoder param.
	 */
	pjmedia_vid_codec_h264_fmtp fmtp;
	pjmedia_video_format_detail *vfd;
	pjmedia_ratio r;
	pjmedia_rect_size highest_size;
	pj_status_t status;
	
	status = pjmedia_vid_codec_h264_parse_fmtp(&param->dec_fmtp,
						   &fmtp);
	if (status != PJ_SUCCESS)
	    return status;

	vfd = pjmedia_format_get_video_format_detail(&param->dec_fmt,
						     PJ_TRUE);

	if (vfd->fps.num == 0 || vfd->fps.denum == 0) {
	    vfd->fps.num   = DEFAULT_H264_FPS_NUM;
	    vfd->fps.denum = DEFAULT_H264_FPS_DENUM;
	}

	/* Normalize decoding resolution, i.e: it must not be lower than
	 * the H264 profile level setting used, as this may be used by
	 * app to allocate buffer.
	 */
	r.num = vfd->size.w;
	r.denum = vfd->size.h;
	find_highest_res(&fmtp, &vfd->fps, &r, &highest_size);
	if (vfd->size.w * vfd->size.h < highest_size.w * highest_size.h)
	    vfd->size = highest_size;

	/* Normalize decoding bitrate based on H264 level spec */
	if (vfd->avg_bps < fmtp.max_br * 1000)
	    vfd->avg_bps = fmtp.max_br * 1000;
	if (vfd->max_bps < fmtp.max_br * 1000)
	    vfd->max_bps = fmtp.max_br * 1000;
    }

    return PJ_SUCCESS;
}


#endif /* PJMEDIA_HAS_VIDEO */
