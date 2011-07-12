#include "test.h"
#include <pjmedia-codec/ffmpeg_codecs.h>
#include <pjmedia-videodev/videodev.h>
#include <pjmedia/vid_codec.h>
#include <pjmedia/port.h>

#define THIS_FILE "vid_codec.c"

#define BYPASS_CODEC	    0
#define BYPASS_PACKETIZER   0

/* 
 * Capture device setting: 
 *   -1 = colorbar, 
 *   -2 = any non-colorbar capture device (first found)
 *    x = specified capture device id
 */
#define CAPTURE_DEV	    -1


typedef struct codec_port_data_t
{
    pjmedia_vid_codec   *codec;
    pjmedia_vid_port    *rdr_port;
    pj_uint8_t          *enc_buf;
    pj_size_t            enc_buf_size;
    pj_uint8_t          *pack_buf;
    pj_size_t            pack_buf_size;
} codec_port_data_t;

static pj_status_t codec_on_event(pjmedia_event_subscription *esub,
                                  pjmedia_event *event)
{
    codec_port_data_t *port_data = (codec_port_data_t*)esub->user_data;

    if (event->type == PJMEDIA_EVENT_FMT_CHANGED) {
	pjmedia_vid_codec *codec = port_data->codec;
	pjmedia_vid_codec_param codec_param;
	pj_status_t status;

	++event->proc_cnt;

	status = codec->op->get_param(codec, &codec_param);
	if (status != PJ_SUCCESS)
	    return status;

	status = pjmedia_vid_dev_stream_set_cap(
			pjmedia_vid_port_get_stream(port_data->rdr_port),
			PJMEDIA_VID_DEV_CAP_FORMAT,
			&codec_param.dec_fmt);
	if (status != PJ_SUCCESS)
	    return status;
    }

    return PJ_SUCCESS;
}

static pj_status_t codec_put_frame(pjmedia_port *port,
			           pjmedia_frame *frame)
{
    codec_port_data_t *port_data = (codec_port_data_t*)port->port_data.pdata;
    pj_status_t status;

#if !BYPASS_CODEC
    {
	pjmedia_vid_codec *codec = port_data->codec;
	pjmedia_frame enc_frame;

	enc_frame.buf = port_data->enc_buf;
	enc_frame.size = port_data->enc_buf_size;

	status = codec->op->encode(codec, frame, enc_frame.size, &enc_frame);
	if (status != PJ_SUCCESS) goto on_error;

#if !BYPASS_PACKETIZER
	if (enc_frame.size) {
	    unsigned pos = 0;
	    pj_bool_t packetized = PJ_FALSE;
	    unsigned unpack_pos = 0;
	    
	    while (pos < enc_frame.size) {
		pj_uint8_t *payload;
		pj_size_t payload_len;

		status = codec->op->packetize(codec, 
					      (pj_uint8_t*)enc_frame.buf,
					      enc_frame.size, &pos,
					      (const pj_uint8_t**)&payload,
					      &payload_len);
		if (status == PJ_ENOTSUP)
		    break;
		if (status != PJ_SUCCESS)
		    goto on_error;

		status = codec->op->unpacketize(codec, payload, payload_len,
						port_data->pack_buf,
						port_data->pack_buf_size,
						&unpack_pos);
		if (status != PJ_SUCCESS)
		    goto on_error;

		// what happen if the bitstream is broken?
		//if (i++ != 1) unpack_pos -= 10;

		packetized = PJ_TRUE;
	    }

	    if (packetized) {
		enc_frame.buf  = port_data->pack_buf;
		enc_frame.size = unpack_pos;
	    }
	}
#endif

	status = codec->op->decode(codec, &enc_frame, frame->size, frame);
	if (status != PJ_SUCCESS) goto on_error;
    }
#endif

    status = pjmedia_port_put_frame(
			pjmedia_vid_port_get_passive_port(port_data->rdr_port),
			frame);
    if (status != PJ_SUCCESS) goto on_error;

    return PJ_SUCCESS;

on_error:
    pj_perror(3, THIS_FILE, status, "codec_put_frame() error");
    return status;
}

static const char* dump_codec_info(const pjmedia_vid_codec_info *info)
{
    static char str[80];
    unsigned i;
    char *p = str;

    /* Raw format ids */
    for (i=0; (i<info->dec_fmt_id_cnt) && (p-str+5<sizeof(str)); ++i) {
        pj_memcpy(p, &info->dec_fmt_id[i], 4);
        p += 4;
        *p++ = ' ';
    }
    *p = '\0';

    return str;
}

static int enum_codecs()
{
    unsigned i, cnt;
    pjmedia_vid_codec_info info[PJMEDIA_CODEC_MGR_MAX_CODECS];
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  codec enums"));
    cnt = PJ_ARRAY_SIZE(info);
    status = pjmedia_vid_codec_mgr_enum_codecs(NULL, &cnt, info, NULL);
    if (status != PJ_SUCCESS)
        return 100;

    for (i = 0; i < cnt; ++i) {
        PJ_LOG(3, (THIS_FILE, "  %-16.*s %c%c %s",
                   info[i].encoding_name.slen, info[i].encoding_name.ptr,
                   (info[i].dir & PJMEDIA_DIR_ENCODING? 'E' : ' '),
                   (info[i].dir & PJMEDIA_DIR_DECODING? 'D' : ' '),
                   dump_codec_info(&info[i])));
    }

    return PJ_SUCCESS;
}

static int encode_decode_test(pj_pool_t *pool, const char *codec_id)
{
    const pj_str_t port_name = {"codec", 5};

    pjmedia_vid_codec *codec=NULL;
    pjmedia_port codec_port;
    codec_port_data_t codec_port_data;
    pjmedia_vid_codec_param codec_param;
    const pjmedia_vid_codec_info *codec_info;

    pjmedia_vid_dev_index cap_idx, rdr_idx;
    pjmedia_vid_port *capture=NULL, *renderer=NULL;
    pjmedia_vid_port_param vport_param;
    pjmedia_video_format_detail *vfd;
    pjmedia_event_subscription esub;
    pj_status_t status;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "  encode decode test"));

    /* Lookup codec */
    {
        pj_str_t codec_id_st;
        unsigned info_cnt = 1;

        /* Lookup codec */
        pj_cstr(&codec_id_st, codec_id);
        status = pjmedia_vid_codec_mgr_find_codecs_by_id(NULL, &codec_id_st, 
                                                         &info_cnt, 
                                                         &codec_info, NULL);
        if (status != PJ_SUCCESS) {
            rc = 205; goto on_return;
        }
    }


#if CAPTURE_DEV == -1
    /* Lookup colorbar source */
    status = pjmedia_vid_dev_lookup("Colorbar", "Colorbar generator", &cap_idx);
    if (status != PJ_SUCCESS) {
	rc = 206; goto on_return;
    }
#elif CAPTURE_DEV == -2
    /* Lookup any first non-colorbar source */
    {
	unsigned i, cnt;
	pjmedia_vid_dev_info info;

	cap_idx = -1;
	cnt = pjmedia_vid_dev_count();
	for (i = 0; i < cnt; ++i) {
	    status = pjmedia_vid_dev_get_info(i, &info);
	    if (status != PJ_SUCCESS) {
		rc = 206; goto on_return;
	    }
	    if (info.dir & PJMEDIA_DIR_CAPTURE && 
		pj_ansi_stricmp(info.driver, "Colorbar"))
	    {
		cap_idx = i;
		break;
	    }
	}

	if (cap_idx == -1) {
	    status = PJ_ENOTFOUND;
	    rc = 206; goto on_return;
	}
    }
#else
    cap_idx = CAPTURE_DEV;
#endif

    /* Lookup SDL renderer */
    status = pjmedia_vid_dev_lookup("SDL", "SDL renderer", &rdr_idx);
    if (status != PJ_SUCCESS) {
	rc = 207; goto on_return;
    }

    /* Prepare codec */
    {
        pj_str_t codec_id_st;
        unsigned info_cnt = 1;
        const pjmedia_vid_codec_info *codec_info;

        /* Lookup codec */
        pj_cstr(&codec_id_st, codec_id);
        status = pjmedia_vid_codec_mgr_find_codecs_by_id(NULL, &codec_id_st, 
                                                         &info_cnt, 
                                                         &codec_info, NULL);
        if (status != PJ_SUCCESS) {
            rc = 245; goto on_return;
        }
        status = pjmedia_vid_codec_mgr_get_default_param(NULL, codec_info,
                                                         &codec_param);
        if (status != PJ_SUCCESS) {
            rc = 246; goto on_return;
        }

#if !BYPASS_CODEC

        /* Open codec */
        status = pjmedia_vid_codec_mgr_alloc_codec(NULL, codec_info,
                                                   &codec);
        if (status != PJ_SUCCESS) {
	    rc = 250; goto on_return;
        }

        status = codec->op->init(codec, pool);
        if (status != PJ_SUCCESS) {
	    rc = 251; goto on_return;
        }

        status = codec->op->open(codec, &codec_param);
        if (status != PJ_SUCCESS) {
	    rc = 252; goto on_return;
        }

	/* After opened, codec will update the param, let's sync encoder & 
	 * decoder format detail.
	 */
	codec_param.dec_fmt.det = codec_param.enc_fmt.det;

	/* Subscribe to codec events */
	pjmedia_event_subscription_init(&esub, &codec_on_event,
	                                &codec_port_data);
	pjmedia_event_subscribe(&codec->epub, &esub);
#endif /* !BYPASS_CODEC */
    }

    pjmedia_vid_port_param_default(&vport_param);

    /* Create capture, set it to active (master) */
    status = pjmedia_vid_dev_default_param(pool, cap_idx,
					   &vport_param.vidparam);
    if (status != PJ_SUCCESS) {
	rc = 220; goto on_return;
    }
    pjmedia_format_copy(&vport_param.vidparam.fmt, &codec_param.dec_fmt);
    vport_param.vidparam.dir = PJMEDIA_DIR_CAPTURE;
    vport_param.active = PJ_TRUE;

    if (vport_param.vidparam.fmt.detail_type != PJMEDIA_FORMAT_DETAIL_VIDEO) {
	rc = 221; goto on_return;
    }

    vfd = pjmedia_format_get_video_format_detail(&vport_param.vidparam.fmt,
						 PJ_TRUE);
    if (vfd == NULL) {
	rc = 225; goto on_return;
    }

    status = pjmedia_vid_port_create(pool, &vport_param, &capture);
    if (status != PJ_SUCCESS) {
	rc = 226; goto on_return;
    }

    /* Create renderer, set it to passive (slave)  */
    vport_param.active = PJ_FALSE;
    vport_param.vidparam.dir = PJMEDIA_DIR_RENDER;
    vport_param.vidparam.rend_id = rdr_idx;
    vport_param.vidparam.disp_size = vfd->size;

    status = pjmedia_vid_port_create(pool, &vport_param, &renderer);
    if (status != PJ_SUCCESS) {
	rc = 230; goto on_return;
    }

    /* Init codec port */
    pj_bzero(&codec_port, sizeof(codec_port));
    status = pjmedia_port_info_init2(&codec_port.info, &port_name, 0x1234,
                                     PJMEDIA_DIR_ENCODING, 
                                     &codec_param.dec_fmt);
    if (status != PJ_SUCCESS) {
	rc = 260; goto on_return;
    }

    codec_port_data.codec = codec;
    codec_port_data.rdr_port = renderer;
    codec_port_data.enc_buf_size = codec_param.dec_fmt.det.vid.size.w *
				   codec_param.dec_fmt.det.vid.size.h * 4;
    codec_port_data.enc_buf = pj_pool_alloc(pool, 
					    codec_port_data.enc_buf_size);
    codec_port_data.pack_buf_size = codec_port_data.enc_buf_size;
    codec_port_data.pack_buf = pj_pool_alloc(pool, 
					     codec_port_data.pack_buf_size);

    codec_port.put_frame = &codec_put_frame;
    codec_port.port_data.pdata = &codec_port_data;

    /* Connect capture to codec port */
    status = pjmedia_vid_port_connect(capture,
				      &codec_port,
				      PJ_FALSE);
    if (status != PJ_SUCCESS) {
	rc = 270; goto on_return;
    }

#if BYPASS_CODEC
    PJ_LOG(3, (THIS_FILE, "  starting loopback test: %c%c%c%c %dx%d",
        ((codec_param.dec_fmt.id & 0x000000FF) >> 0),
        ((codec_param.dec_fmt.id & 0x0000FF00) >> 8),
        ((codec_param.dec_fmt.id & 0x00FF0000) >> 16),
        ((codec_param.dec_fmt.id & 0xFF000000) >> 24),
        codec_param.dec_fmt.det.vid.size.w,
        codec_param.dec_fmt.det.vid.size.h
        ));
#else
    PJ_LOG(3, (THIS_FILE, "  starting codec test: %c%c%c%c<->%.*s %dx%d",
        ((codec_param.dec_fmt.id & 0x000000FF) >> 0),
        ((codec_param.dec_fmt.id & 0x0000FF00) >> 8),
        ((codec_param.dec_fmt.id & 0x00FF0000) >> 16),
        ((codec_param.dec_fmt.id & 0xFF000000) >> 24),
	codec_info->encoding_name.slen,
	codec_info->encoding_name.ptr,
        codec_param.dec_fmt.det.vid.size.w,
        codec_param.dec_fmt.det.vid.size.h
        ));
#endif

    /* Start streaming.. */
    status = pjmedia_vid_port_start(renderer);
    if (status != PJ_SUCCESS) {
	rc = 275; goto on_return;
    }
    status = pjmedia_vid_port_start(capture);
    if (status != PJ_SUCCESS) {
	rc = 280; goto on_return;
    }

    /* Sleep while the video is being displayed... */
    pj_thread_sleep(10000);

on_return:
    if (status != PJ_SUCCESS) {
        PJ_PERROR(3, (THIS_FILE, status, "  error"));
    }
    if (capture) {
        pjmedia_vid_port_stop(capture);
	pjmedia_vid_port_destroy(capture);
    }
    if (renderer) {
        pjmedia_vid_port_stop(renderer);
	pjmedia_vid_port_destroy(renderer);
    }
    if (codec) {
        codec->op->close(codec);
        pjmedia_vid_codec_mgr_dealloc_codec(NULL, codec);
    }

    return rc;
}

int vid_codec_test(void)
{
    pj_pool_t *pool;
    int rc = 0;
    pj_status_t status;
    int orig_log_level;
    
    orig_log_level = pj_log_get_level();
    pj_log_set_level(6);

    PJ_LOG(3, (THIS_FILE, "Performing video codec tests.."));

    pool = pj_pool_create(mem, "Vid codec test", 256, 256, 0);

    status = pjmedia_vid_dev_subsys_init(mem);
    if (status != PJ_SUCCESS)
        return -10;

    status = pjmedia_codec_ffmpeg_init(NULL, mem);
    if (status != PJ_SUCCESS)
        return -20;

    rc = enum_codecs();
    if (rc != 0)
	goto on_return;

    rc = encode_decode_test(pool, "h263-1998");
    if (rc != 0)
	goto on_return;

on_return:
    pjmedia_codec_ffmpeg_deinit();
    pjmedia_vid_dev_subsys_shutdown();
    pj_pool_release(pool);
    pj_log_set_level(orig_log_level);

    return rc;
}


