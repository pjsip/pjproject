/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSUA_CMD_H__
#define __PJSUA_CMD_H__

#include <pjsua-lib/pjsua.h>

PJ_BEGIN_DECL

#define current_acc	pjsua_acc_get_default()

#define NO_LIMIT_DURATION   (int)0x7FFFFFFF
#define MAX_AVI		    4
#define NO_NB		    -2

typedef struct input_result
{
    int	  nb_result;
    char *uri_result;
} input_result;

/* Call specific data */
typedef struct call_data
{
    pj_timer_entry	    timer;
    pj_bool_t		    ringback_on;
    pj_bool_t		    ring_on;
} call_data;

/* Video settings */
typedef struct app_vid
{
    unsigned		    vid_cnt;
    int			    vcapture_dev;
    int			    vrender_dev;
    pj_bool_t		    in_auto_show;
    pj_bool_t		    out_auto_transmit;
} app_vid;

/* Pjsua application data */
typedef struct pjsua_app_config
{
    pjsua_config	    cfg;
    pjsua_logging_config    log_cfg;
    pjsua_media_config	    media_cfg;
    pj_bool_t		    no_refersub;
    pj_bool_t		    ipv6;
    pj_bool_t		    enable_qos;
    pj_bool_t		    no_tcp;
    pj_bool_t		    no_udp;
    pj_bool_t		    use_tls;
    pjsua_transport_config  udp_cfg;
    pjsua_transport_config  rtp_cfg;
    pjsip_redirect_op	    redir_op;

    unsigned		    acc_cnt;
    pjsua_acc_config	    acc_cfg[PJSUA_MAX_ACC];

    unsigned		    buddy_cnt;
    pjsua_buddy_config	    buddy_cfg[PJSUA_MAX_BUDDIES];

    call_data		    call_data[PJSUA_MAX_CALLS];

    pj_pool_t		   *pool;
    /* Compatibility with older pjsua */

    unsigned		    codec_cnt;
    pj_str_t		    codec_arg[32];
    unsigned		    codec_dis_cnt;
    pj_str_t                codec_dis[32];
    pj_bool_t		    null_audio;
    unsigned		    wav_count;
    pj_str_t		    wav_files[32];
    unsigned		    tone_count;
    pjmedia_tone_desc	    tones[32];
    pjsua_conf_port_id	    tone_slots[32];
    pjsua_player_id	    wav_id;
    pjsua_conf_port_id	    wav_port;
    pj_bool_t		    auto_play;
    pj_bool_t		    auto_play_hangup;
    pj_timer_entry	    auto_hangup_timer;
    pj_bool_t		    auto_loop;
    pj_bool_t		    auto_conf;
    pj_str_t		    rec_file;
    pj_bool_t		    auto_rec;
    pjsua_recorder_id	    rec_id;
    pjsua_conf_port_id	    rec_port;
    unsigned		    auto_answer;
    unsigned		    duration;

#ifdef STEREO_DEMO
    pjmedia_snd_port	   *snd;
    pjmedia_port	   *sc, *sc_ch1;
    pjsua_conf_port_id	    sc_ch1_slot;
#endif

    float		    mic_level,
			    speaker_level;

    int			    capture_dev, playback_dev;
    unsigned		    capture_lat, playback_lat;

    pj_bool_t		    no_tones;
    int			    ringback_slot;
    int			    ringback_cnt;
    pjmedia_port	   *ringback_port;
    int			    ring_slot;
    int			    ring_cnt;
    pjmedia_port	   *ring_port;

    app_vid		    vid;
    unsigned		    aud_cnt;

    /* AVI to play */
    unsigned                avi_cnt;
    struct {
	pj_str_t		path;
	pjmedia_vid_dev_index	dev_id;
	pjsua_conf_port_id	slot;
    } avi[MAX_AVI];
    pj_bool_t               avi_auto_play;
    int			    avi_def_idx;

    /* CLI setting */
    pj_bool_t		    use_cli;
    unsigned		    cli_telnet_port;    
} pjsua_app_config;

/** Extern variable declaration **/
extern pjsua_call_id	    current_call;
extern pjsua_app_config	    app_config;
extern int		    stdout_refresh;
extern pj_bool_t	    stdout_refresh_quit;
extern pjsua_call_setting call_opt;
extern pjsua_msg_data msg_data;

PJ_DECL(int) my_atoi(const char *cs);
PJ_DECL(pj_bool_t) find_next_call();
PJ_DECL(pj_bool_t) find_prev_call();
PJ_DECL(void) send_request(char *cstr_method, const pj_str_t *dst_uri);
PJ_DECL(void) log_call_dump(int call_id);
PJ_DECL(int) write_settings(pjsua_app_config *cfg, char *buf, pj_size_t max);
PJ_DECL(void) app_config_init_video(pjsua_acc_config *acc_cfg);
PJ_DECL(void) arrange_window(pjsua_vid_win_id wid);

#if PJSUA_HAS_VIDEO
PJ_DECL(void) vid_print_dev(int id, const pjmedia_vid_dev_info *vdi, const char *title);
PJ_DECL(void) vid_list_devs();
PJ_DECL(void) app_config_show_video(int acc_id, const pjsua_acc_config *acc_cfg);
#endif

#ifdef HAVE_MULTIPART_TEST
    /*
    * Enable multipart in msg_data and add a dummy body into the
    * multipart bodies.
    */
    PJ_DECL(void) add_multipart(pjsua_msg_data *msg_data);
#  define TEST_MULTIPART(msg_data)	add_multipart(msg_data)
#else
#  define TEST_MULTIPART(msg_data)
#endif

#endif	/* __PJSUA_CMD_H__ */

