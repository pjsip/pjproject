/* $Id$ */
/* 
 * Copyright (C) 2014 Teluu Inc. (http://www.teluu.com)
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


/**
 * \page page_pjmedia_samples_vid_codec_test_c Samples: Video Codec Test
 *
 * Video codec encode and decode test.
 *
 * This file is pjsip-apps/src/samples/vid_vodec_test.c
 *
 * \includelineno vid_vodec_test.c
 */

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>


#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)


#include <stdlib.h>	/* atoi() */
#include <stdio.h>

#include "util.h"


#define THIS_FILE	"vid_vodec_test.c"


/* If set, local renderer will be created to play original file */
#define HAS_LOCAL_RENDERER_FOR_PLAY_FILE    1


/* Default width and height for the renderer, better be set to maximum
 * acceptable size.
 */
#define DEF_RENDERER_WIDTH		    640
#define DEF_RENDERER_HEIGHT		    480


/* Prototype for LIBSRTP utility in file datatypes.c */
int hex_string_to_octet_string(char *raw, char *hex, int len);

/* 
 * Register all codecs. 
 */
static pj_status_t init_codecs(pj_pool_factory *pf)
{
    pj_status_t status;

    /* To suppress warning about unused var when all codecs are disabled */
    PJ_UNUSED_ARG(status);

#if defined(PJMEDIA_HAS_OPENH264_CODEC) && PJMEDIA_HAS_OPENH264_CODEC != 0
    status = pjmedia_codec_openh264_vid_init(NULL, pf);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#endif

#if defined(PJMEDIA_HAS_VID_TOOLBOX_CODEC) && \
    PJMEDIA_HAS_VID_TOOLBOX_CODEC != 0
    status = pjmedia_codec_vid_toolbox_init(NULL, pf);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#endif

#if defined(PJMEDIA_HAS_VPX_CODEC) && PJMEDIA_HAS_VPX_CODEC != 0
    status = pjmedia_codec_vpx_vid_init(NULL, pf);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#endif

#if defined(PJMEDIA_HAS_FFMPEG_VID_CODEC) && PJMEDIA_HAS_FFMPEG_VID_CODEC != 0
    status = pjmedia_codec_ffmpeg_vid_init(NULL, pf);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#endif

    return PJ_SUCCESS;
}

/* 
 * Register all codecs. 
 */
static void deinit_codecs()
{
#if defined(PJMEDIA_HAS_FFMPEG_VID_CODEC) && PJMEDIA_HAS_FFMPEG_VID_CODEC != 0
    pjmedia_codec_ffmpeg_vid_deinit();
#endif

#if defined(PJMEDIA_HAS_OPENH264_CODEC) && PJMEDIA_HAS_OPENH264_CODEC != 0
    pjmedia_codec_openh264_vid_deinit();
#endif

#if defined(PJMEDIA_HAS_VID_TOOLBOX_CODEC) && \
    PJMEDIA_HAS_VID_TOOLBOX_CODEC != 0
    pjmedia_codec_vid_toolbox_deinit();
#endif

#if defined(PJMEDIA_HAS_VPX_CODEC) && PJMEDIA_HAS_VPX_CODEC != 0
    pjmedia_codec_vpx_vid_deinit();
#endif

}


static void show_diff(const pj_uint8_t *buf1, const pj_uint8_t *buf2,
                      unsigned size)
{
    enum {
	STEP = 50
    };
    unsigned i=0;

    for (; i<size; ) {
	const pj_uint8_t *p1 = buf1 + i, *p2 = buf2 + i;
	unsigned j;

	printf("%8d ", i);
	for (j=0; j<STEP && i+j<size; ++j) {
	    printf(" %02x", *(p1+j));
	}
	printf("\n");
	printf("         ");
	for (j=0; j<STEP && i+j<size; ++j) {
	    if (*(p1+j) == *(p2+j)) {
		printf(" %02x", *(p2+j));
	    } else {
		printf(" %02x", *(p2+j));
	    }
	}
	printf("\n");

	i += j;
    }
}

static void diff_file()
{
    const char *filename[2] = {
        "/home/bennylp/Desktop/opt/src/openh264-svn/testbin/test.264",
        "/home/bennylp/Desktop/opt/src/openh264-svn/testbin/test2.264"
    };
    unsigned size[2];
    pj_uint8_t *buf[2], start_nal[3] = {0, 0, 1};
    unsigned i, pos[2], frame_cnt, mismatch_cnt=0;

    for (i=0; i<2; ++i) {
	FILE *fhnd;
	const pj_uint8_t start_nal[] = { 0, 0, 1};

	fhnd = fopen(filename[i], "rb");
	if (!fhnd) {
	    printf("Error opening %s\n", filename[i]);
	    return;
	}

	fseek(fhnd, 0, SEEK_END);
	size[i] = ftell(fhnd);
	fseek(fhnd, 0, SEEK_SET);

	buf[i] = (pj_uint8_t*)malloc(size[i] + 4);
	if (!buf[i])
	    return;

	if (fread (buf[i], 1, size[i], fhnd) != (unsigned)size[i]) {
	    fprintf (stderr, "Unable to read whole file\n");
	    return;
	}

	memcpy (buf[i] + size[i], start_nal, sizeof(start_nal));

	fclose(fhnd);
    }

    if (size[0] != size[1]) {
	printf("File size mismatch\n");
	return;
    }

    pos[0] = pos[1] = 0;
    for ( frame_cnt=0; ; ++frame_cnt) {
	unsigned nal_len[2];
	for (i = 0; i < size[0]; i++) {
	    if (memcmp(buf[0] + pos[0] + i, start_nal,
	               sizeof(start_nal)) == 0 && i > 0)
	    {
		break;
	    }
	}
	nal_len[0] = i;
	for (i = 0; i < size[1]; i++) {
	    if (memcmp(buf[1] + pos[1] + i, start_nal,
	               sizeof(start_nal)) == 0 && i > 0)
	    {
		break;
	    }
	}
	nal_len[1] = i;

	if (nal_len[0] != nal_len[1]) {
	    printf("Different size in frame %d (%d vs %d)\n",
	           frame_cnt, nal_len[0], nal_len[1]);
	}

	if (memcmp(buf[0]+pos[0], buf[1]+pos[1], nal_len[0]) != 0) {
	    printf("Mismatch in frame %d\n", frame_cnt);
	    show_diff(buf[0]+pos[0], buf[1]+pos[1], nal_len[0]);
	    puts("");
	    ++mismatch_cnt;
	}

	pos[0] += nal_len[0];
	pos[1] += nal_len[1];

	if (pos[0] >= size[0])
	    break;
    }

    free(buf[0]);
    free(buf[1]);

    if (!mismatch_cnt)
	puts("Files the same!");
    else
	printf("%d mismatches\n", mismatch_cnt);
}

/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pj_status_t status; 

    /* Codec */
    char *codec_id = (char*)"H264";
    const pjmedia_vid_codec_info *codec_info;
    pjmedia_vid_codec_param codec_param;
    pjmedia_vid_codec *codec = NULL;

    //const char *save_filename =
    //	"/home/bennylp/Desktop/opt/src/openh264-svn/testbin/test.264";
    const char *save_filename = NULL;

    /* File */
    enum
    {
	WIDTH = 320,
	HEIGHT = 192,
	FPS = 12,
	YUV_SIZE = WIDTH * HEIGHT * 3 >> 1,
	YUV_BUF_SIZE = YUV_SIZE + WIDTH,
	MAX_FRAMES = 32,
	MTU = 1500
    };
    FILE *fyuv = NULL;
    FILE *f264 = NULL;
    typedef pj_uint8_t enc_buf_type[MTU];
    pj_uint8_t yuv_frame[YUV_BUF_SIZE];
    enc_buf_type enc_buf[MAX_FRAMES];
    unsigned read_cnt = 0,
	     pkt_cnt = 0,
	     dec_cnt = 0,
	     enc_cnt;

    if (0) {
	diff_file();
	return 1;
    }

    /* init PJLIB : */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    /* Initialize media endpoint. */
    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool for application purpose */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "app",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    /* Init video format manager */
    pjmedia_video_format_mgr_create(pool, 64, 0, NULL);

    /* Init video converter manager */
    pjmedia_converter_mgr_create(pool, NULL);

    /* Init event manager */
    pjmedia_event_mgr_create(pool, 0, NULL);

    /* Init video codec manager */
    pjmedia_vid_codec_mgr_create(pool, NULL);

    /* Register all supported codecs */
    status = init_codecs(&cp.factory);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Open YUV file */
    fyuv = fopen("pjsip-apps/bin/CiscoVT2people_320x192_12fps.yuv", "rb");
    if (!fyuv) {
	puts("Unable to open ../CiscoVT2people_320x192_12fps.yuv");
	status = -1;
	goto on_exit;
    }

    /* Write 264 file if wanted */
    if (save_filename) {
	f264 = fopen(save_filename, "wb");
    }

    /* Find which codec to use. */
    if (codec_id) {
	unsigned count = 1;
	pj_str_t str_codec_id = pj_str(codec_id);

        status = pjmedia_vid_codec_mgr_find_codecs_by_id(NULL,
						         &str_codec_id, &count,
						         &codec_info, NULL);
	if (status != PJ_SUCCESS) {
	    printf("Error: unable to find codec %s\n", codec_id);
	    return 1;
	}
    } else {
        static pjmedia_vid_codec_info info[1];
        unsigned count = PJ_ARRAY_SIZE(info);

	/* Default to first codec */
	pjmedia_vid_codec_mgr_enum_codecs(NULL, &count, info, NULL);
        codec_info = &info[0];
    }

    /* Get codec default param for info */
    status = pjmedia_vid_codec_mgr_get_default_param(NULL, codec_info, 
				                     &codec_param);
    pj_assert(status == PJ_SUCCESS);
    
    /* Alloc encoder */
    status = pjmedia_vid_codec_mgr_alloc_codec(NULL, codec_info, &codec);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(3,(THIS_FILE, status, "Error allocating codec"));
	goto on_exit;
    }

    codec_param.dir = PJMEDIA_DIR_ENCODING_DECODING;
    codec_param.packing = PJMEDIA_VID_PACKING_PACKETS;
    codec_param.enc_mtu = MTU;
    codec_param.enc_fmt.det.vid.size.w = WIDTH;
    codec_param.enc_fmt.det.vid.size.h = HEIGHT;
    codec_param.enc_fmt.det.vid.fps.num = FPS;
    codec_param.enc_fmt.det.vid.avg_bps = WIDTH * HEIGHT * FPS;

    status = pjmedia_vid_codec_init(codec, pool);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(3,(THIS_FILE, status, "Error initializing codec"));
	goto on_exit;
    }

    status = pjmedia_vid_codec_open(codec, &codec_param);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(3,(THIS_FILE, status, "Error opening codec"));
	goto on_exit;
    }

    while (fread(yuv_frame, 1, YUV_SIZE, fyuv) == YUV_SIZE) {
	pjmedia_frame frm_yuv, frm_enc[MAX_FRAMES];
	pj_bool_t has_more = PJ_FALSE;
	const pj_uint8_t start_nal[] = { 0, 0, 1 };

	++ read_cnt;

	pj_bzero(&frm_enc, sizeof(frm_enc));
	pj_bzero(&frm_yuv, sizeof(frm_yuv));

	frm_yuv.buf = yuv_frame;
	frm_yuv.size = YUV_SIZE;

	enc_cnt = 0;
	frm_enc[enc_cnt].buf = enc_buf[enc_cnt];
	frm_enc[enc_cnt].size = MTU;

	status = pjmedia_vid_codec_encode_begin(codec, NULL, &frm_yuv,
	                                        MTU, &frm_enc[enc_cnt],
	                                        &has_more);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(3,(THIS_FILE, status, "Codec encode error"));
	    goto on_exit;
	}
	if (frm_enc[enc_cnt].size) {
	    if (f264) {
		fwrite(start_nal, 1, sizeof(start_nal), f264);
		fwrite(frm_enc[enc_cnt].buf, 1, frm_enc[enc_cnt].size, f264);
	    }
	    ++pkt_cnt;
	    ++enc_cnt;
	}

	while (has_more) {

	    if (enc_cnt >= MAX_FRAMES) {
		status = -1;
		puts("Error: too many encoded frames");
		goto on_exit;
	    }

	    has_more = PJ_FALSE;
	    frm_enc[enc_cnt].buf = enc_buf[enc_cnt];
	    frm_enc[enc_cnt].size = MTU;

	    status = pjmedia_vid_codec_encode_more(codec, MTU,
	                                           &frm_enc[enc_cnt],
	                                           &has_more);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(3,(THIS_FILE, status, "Codec encode error"));
		goto on_exit;
	    }

	    if (frm_enc[enc_cnt].size) {
		if (f264) {
		    fwrite(start_nal, 1, sizeof(start_nal), f264);
		    fwrite(frm_enc[enc_cnt].buf, 1, frm_enc[enc_cnt].size,
		           f264);
		}
		++pkt_cnt;
		++enc_cnt;
	    }
	}

	if (enc_cnt) {
	    frm_yuv.buf = yuv_frame;
	    frm_yuv.size = YUV_BUF_SIZE;
	    status = pjmedia_vid_codec_decode(codec, enc_cnt,
					      frm_enc,
					      YUV_BUF_SIZE,
					      &frm_yuv);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(3,(THIS_FILE, status, "Codec decode error"));
		goto on_exit;
	    }

	    if (frm_yuv.size != 0) {
		++dec_cnt;
	    }
	}
    }

    printf("Done.\n"
	   " Read YUV frames:    %d\n"
	   " Encoded packets:    %d\n"
	   " Decoded YUV frames: %d\n",
	   read_cnt, pkt_cnt, dec_cnt);

    /* Start deinitialization: */
on_exit:
    if (codec) {
	pjmedia_vid_codec_close(codec);
	pjmedia_vid_codec_mgr_dealloc_codec(NULL, codec);
    }

    if (f264)
	fclose(f264);

    if (fyuv)
	fclose(fyuv);

    /* Deinit codecs */
    deinit_codecs();

    /* Destroy event manager */
    pjmedia_event_mgr_destroy(NULL);

    /* Release application pool */
    pj_pool_release( pool );

    /* Destroy media endpoint. */
    pjmedia_endpt_destroy( med_endpt );

    /* Destroy pool factory */
    pj_caching_pool_destroy( &cp );

    /* Shutdown PJLIB */
    pj_shutdown();

    /* Avoid compile warning */
    PJ_UNUSED_ARG(app_perror);

    return (status == PJ_SUCCESS) ? 0 : 1;
}


#else

int main(int argc, char *argv[])
{
    PJ_UNUSED_ARG(argc);
    PJ_UNUSED_ARG(argv);
    puts("Error: this sample requires video capability "
	    "(PJMEDIA_HAS_VIDEO == 1)");
    return -1;
}

#endif /* PJMEDIA_HAS_VIDEO */
