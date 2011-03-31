/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#include <pjlib-util/base64.h>
#include <pj/math.h>

#define THIS_FILE   "vid_codec_util.c"


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


PJ_DEF(pj_status_t) pjmedia_vid_codec_h263_apply_fmtp(
				pjmedia_vid_codec_param *param)
{
    if (param->dir & PJMEDIA_DIR_ENCODING) {
	pjmedia_vid_codec_h263_fmtp fmtp_loc, fmtp_rem;
	pjmedia_rect_size size = {0};
	unsigned mpi = 0;
	pjmedia_video_format_detail *vfd;
	pj_status_t status;

	/* Get local param */
	status = pjmedia_vid_codec_parse_h263_fmtp(&param->dec_fmtp,
						   &fmtp_loc);
	if (status != PJ_SUCCESS)
	    return status;

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
	} else if (fmtp_loc.mpi_cnt == 0) {
	    /* Local MPI setting not set, just use remote preference. */
	    size = fmtp_rem.mpi[0].size;
	    mpi  = fmtp_rem.mpi[0].val;
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
	vfd = pjmedia_format_get_video_format_detail(&param->enc_fmt,
						     PJ_TRUE);
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


/* H264 fmtp parser */
PJ_DEF(pj_status_t) pjmedia_vid_codec_parse_h264_fmtp(
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

    pj_bzero(h264_fmtp, sizeof(*h264_fmtp));

    for (i=0; i<fmtp->cnt; ++i) {
	unsigned tmp;
	if (pj_stricmp(&fmtp->param[i].name, &PROFILE_LEVEL_ID)==0) {
	    pj_str_t endst;

	    if (fmtp->param[i].val.slen != 6)
		return PJMEDIA_SDP_EINFMTP;

	    tmp = pj_strtoul2(&fmtp->param[i].val, &endst, 16);
	    if (endst.slen)
		return PJMEDIA_SDP_EINFMTP;

	    h264_fmtp->profile_idc = (pj_uint8_t)((tmp >> 16) & 0xFF);
	    h264_fmtp->profile_iop = (pj_uint8_t)((tmp >> 8) & 0xFF);
	    h264_fmtp->profile_idc = (pj_uint8_t)(tmp & 0xFF);
	} else if (pj_stricmp(&fmtp->param[i].name, &PACKETIZATION_MODE)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    if (tmp) h264_fmtp->max_br = tmp;
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_MBPS)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    if (tmp) h264_fmtp->max_mbps = tmp;
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_FS)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    if (tmp) h264_fmtp->max_fs = tmp;
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_CPB)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    if (tmp) h264_fmtp->max_cpb = tmp;
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_DPB)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    if (tmp) h264_fmtp->max_dpb = tmp;
	} else if (pj_stricmp(&fmtp->param[i].name, &MAX_BR)==0) {
	    tmp = pj_strtoul(&fmtp->param[i].val);
	    if (tmp) h264_fmtp->max_br = tmp;
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

    return PJ_SUCCESS;
}
