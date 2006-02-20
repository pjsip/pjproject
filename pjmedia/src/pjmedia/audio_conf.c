/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/audio_conf.h>
#include <pjmedia/vad.h>
#include <pjmedia/stream.h>
#include <pjmedia/sound.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>



#define THIS_FILE   "audio_conf.c"


struct conf_port
{
    pj_str_t		 name;
    pjmedia_stream_port	*port;
    pj_bool_t		 online;
    pj_bool_t		 is_member;
    pjmedia_vad		*vad;
    pj_int32_t		 level;
};

/*
 * Conference bridge.
 */
struct pjmedia_conf
{
    unsigned		  max_ports;	/**< Maximum ports.		*/
    unsigned		  port_cnt;	/**< Current number of ports.	*/
    pj_snd_stream	 *snd_rec;	/**< Sound recorder stream.	*/
    pj_snd_stream	 *snd_player;	/**< Sound player stream.	*/
    struct conf_port	**port;		/**< Array of ports.		*/
    pj_int16_t		 *rec_buf;	/**< Sample buffer for rec.	*/
    pj_int16_t		 *play_buf;	/**< Sample buffer for player	*/
    unsigned		  samples_cnt;	/**< Samples per frame.		*/
    pj_size_t		  buf_size;	/**< Buffer size, in bytes.	*/
};


/* Prototypes */
static pj_status_t play_cb( /* in */  void *user_data,
			    /* in */  pj_uint32_t timestamp,
			    /* out */ void *output,
			    /* out */ unsigned size);
static pj_status_t rec_cb(  /* in */  void *user_data,
			    /* in */  pj_uint32_t timestamp,
			    /* in */  const void *input,
			    /* in*/   unsigned size);


/*
 * Create conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_create( pj_pool_t *pool,
					 unsigned max_ports,
					 pjmedia_conf **p_conf )
{
    pjmedia_conf *conf;
    pj_snd_stream_info snd_info;
    pj_status_t status;

    conf = pj_pool_zalloc(pool, sizeof(pjmedia_conf));
    conf->max_ports = max_ports;
    conf->port = pj_pool_zalloc(pool, max_ports*sizeof(void*));

    /* Create default parameters. */
    pj_memset(&snd_info, 0, sizeof(snd_info));
    snd_info.samples_per_sec = 8000;
    snd_info.bits_per_sample = 16;
    snd_info.samples_per_frame = 160;
    snd_info.bytes_per_frame = 16000;
    snd_info.frames_per_packet = 1;

    /* Create buffers. */
    conf->samples_cnt = snd_info.samples_per_frame;
    conf->buf_size = snd_info.samples_per_frame * snd_info.bits_per_sample / 8;
    conf->rec_buf = pj_pool_alloc(pool,  conf->buf_size);
    conf->play_buf = pj_pool_alloc(pool, conf->buf_size );


    /* Open recorder. */
    conf->snd_rec = pj_snd_open_recorder(-1 ,&snd_info, &rec_cb, conf);
    if (conf->snd_rec == NULL) {
	status = -1;
	goto on_error;
    }

    /* Open player */
    conf->snd_player = pj_snd_open_player(-1, &snd_info, &play_cb, conf);
    if (conf->snd_player == NULL) {
	status = -1;
	goto on_error;
    }

    /* Done */

    *p_conf = conf;

    return PJ_SUCCESS;


on_error:
    if (conf->snd_rec) {
	pj_snd_stream_stop(conf->snd_rec);
	pj_snd_stream_close(conf->snd_rec);
	conf->snd_rec = NULL;
    }
    if (conf->snd_player) {
	pj_snd_stream_stop(conf->snd_player);
	pj_snd_stream_close(conf->snd_player);
	conf->snd_player = NULL;
    }
    return status;
}

/*
 * Activate sound device.
 */
static pj_status_t activate_conf( pjmedia_conf *conf )
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_status_t status;

    /* Start recorder. */
    status = pj_snd_stream_start(conf->snd_rec);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Start player. */
    status = pj_snd_stream_start(conf->snd_rec);
    if (status != PJ_SUCCESS)
	goto on_error;

    return PJ_SUCCESS;

on_error:
    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(4,(THIS_FILE, "Error starting sound player/recorder: %s",
	      errmsg));
    return status;
}


/*
 * Suspend sound device
 */
static void suspend_conf( pjmedia_conf *conf )
{
    pj_snd_stream_stop(conf->snd_rec);
    pj_snd_stream_stop(conf->snd_player);
}


/*
 * Add stream port to the conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_port( pjmedia_conf *conf,
					   pj_pool_t *pool,
					   pjmedia_stream_port *strm_port,
					   const pj_str_t *port_name,
					   unsigned *p_port )
{
    struct conf_port *conf_port;
    unsigned index;
    pj_status_t status;

    PJ_ASSERT_RETURN(conf && pool && strm_port && port_name && p_port, 
		     PJ_EINVAL);

    if (conf->port_cnt >= conf->max_ports) {
	pj_assert(!"Too many ports");
	return PJ_ETOOMANY;
    }

    /* Create port structure. */
    conf_port = pj_pool_zalloc(pool, sizeof(struct conf_port));
    pj_strdup_with_null(pool, &conf_port->name, port_name);
    conf_port->port = strm_port;
    conf_port->online = PJ_TRUE;
    conf_port->level = 0;

    /* Create VAD for this port. */
    status = pjmedia_vad_create(pool, &conf_port->vad);
    if (status != PJ_SUCCESS)
	return status;

    /* Set vad settings. */
    pjmedia_vad_set_adaptive(conf_port->vad, conf->samples_cnt);

    /* Find empty port in the conference bridge. */
    for (index=0; index < conf->max_ports; ++index) {
	if (conf->port[index] == NULL)
	    break;
    }

    pj_assert(index != conf->max_ports);

    /* Put the port. */
    conf->port[index] = conf_port;
    conf->port_cnt++;

    /* If this is the first port, activate sound device. */
    if (conf->port_cnt == 1) {
	status = activate_conf(conf);;
	if (status != PJ_SUCCESS) {
	    conf->port[index] = NULL;
	    --conf->port_cnt;
	    return status;
	}
    }

    /* Done. */
    return PJ_SUCCESS;
}


/*
 * Mute or unmute port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_set_mute( pjmedia_conf *conf,
					   unsigned port,
					   pj_bool_t mute )
{
    /* Check arguments */
    PJ_ASSERT_RETURN(conf && port < conf->max_ports, PJ_EINVAL);

    /* Port must be valid. */
    PJ_ASSERT_RETURN(conf->port[port] != NULL, PJ_EINVAL);

    conf->port[port]->online = !mute;
    
    return PJ_SUCCESS;
}


/*
 * Set the specified port to be member of conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_set_membership( pjmedia_conf *conf,
						 unsigned port,
						 pj_bool_t enabled )
{
    /* Check arguments */
    PJ_ASSERT_RETURN(conf && port < conf->max_ports, PJ_EINVAL);

    /* Port must be valid. */
    PJ_ASSERT_RETURN(conf->port[port] != NULL, PJ_EINVAL);

    conf->port[port]->is_member = enabled;
    
    return PJ_SUCCESS;
}


/*
 * Remove the specified port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_remove_port( pjmedia_conf *conf,
					      unsigned port )
{
    /* Check arguments */
    PJ_ASSERT_RETURN(conf && port < conf->max_ports, PJ_EINVAL);

    /* Port must be valid. */
    PJ_ASSERT_RETURN(conf->port[port] != NULL, PJ_EINVAL);

    /* Suspend the sound devices.
     * Don't want to remove port while port is being accessed by sound
     * device's threads.
     */
    suspend_conf(conf);

    /* Remove the port. */
    conf->port[port] = NULL;
    --conf->port_cnt;

    /* Reactivate sound device if ports are not zero */
    if (conf->port_cnt != 0)
	activate_conf(conf);

    return PJ_SUCCESS;
}


/*
 * Player callback.
 */
static pj_status_t play_cb( /* in */  void *user_data,
			    /* in */  pj_uint32_t timestamp,
			    /* out */ void *output,
			    /* out */ unsigned size)
{
    pjmedia_conf *conf = user_data;
    pj_int16_t *output_buf = output;
    pj_int32_t highest_level = 0;
    int highest_index = -1;
    unsigned sources = 0;
    unsigned i, j;
    
    PJ_UNUSED_ARG(timestamp);

    /* Clear temporary buffer. */
    pj_memset(output_buf, 0, size);

    /* Get frames from ports. */
    for (i=0; i<conf->max_ports; ++i) {
	struct conf_port *conf_port = conf->port[i];
	pj_int32_t level;
	pj_bool_t silence;

	if (!conf_port)
	    continue;

	conf_port->port->get_frame(conf->play_buf, conf->samples_cnt);
	silence = pjmedia_vad_detect_silence(conf_port->vad,
					     conf->play_buf, 
					     conf->samples_cnt,
					     &level);
	if (!silence) {
	    if (level > highest_level) {
		highest_index = i;
		highest_level = level;
	    }

	    ++sources;

	    for (j=0; j<conf->samples_cnt; ++j) {
		output_buf[j] = (pj_int16_t)(output_buf[j] + conf->play_buf[j]);
	    }
	}
    }

    /* Calculate average signal. */
    if (sources) {
	for (j=0; j<conf->samples_cnt; ++j) {
	    output_buf[j] = (pj_int16_t)(output_buf[j] / sources);
	}
    }

    /* Broadcast to conference member. */
    for (i=0; i<conf->max_ports; ++i) {
	struct conf_port *conf_port = conf->port[i];

	if (!conf_port)
	    continue;

	if (!conf_port->is_member)
	    continue;

	conf_port->port->put_frame(output_buf, conf->samples_cnt);
    }

    return PJ_SUCCESS;
}

/*
 * Recorder callback.
 */
static pj_status_t rec_cb(  /* in */  void *user_data,
			    /* in */  pj_uint32_t timestamp,
			    /* in */  const void *input,
			    /* in*/   unsigned size)
{
    pjmedia_conf *conf = user_data;
    unsigned i;

    PJ_UNUSED_ARG(timestamp);
    PJ_UNUSED_ARG(size);

    for (i=0; i<conf->max_ports; ++i) {
	struct conf_port *conf_port = conf->port[i];

	if (!conf_port)
	    continue;

	if (!conf_port->online)
	    continue;

	conf_port->port->put_frame(input, conf->samples_cnt);
    }
    
    return PJ_SUCCESS;
}

