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
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>

#if defined(PJSUA_MEDIA_HAS_PJMEDIA) && PJSUA_MEDIA_HAS_PJMEDIA != 0

#define THIS_FILE               "pjsua_aud.c"

#define SIGNATURE	PJMEDIA_SIG_CLASS_PORT_AUD('A','P')

#define NORMAL_LEVEL		128

 /* These are settings to control the adaptivity of changes in the
 * signal level of the ports, so that sudden change in signal level
 * in the port does not cause misaligned signal (which causes noise).
 */
#define ATTACK_A			(pjsua_var.media_cfg.clock_rate / pjsua_var.mconf_cfg.samples_per_frame)
#define ATTACK_B			1
#define DECAY_A				0
#define DECAY_B				1

#define SIMPLE_AGC(last, target) \
	if (target >= last) \
		target = (ATTACK_A*(last+1)+ATTACK_B*target)/(ATTACK_A+ATTACK_B); \
	else \
		target = (DECAY_A*last+DECAY_B*target)/(DECAY_A+DECAY_B)

#define MAX_LEVEL			(32767)
#define MIN_LEVEL			(-32768)

#define IS_OVERFLOW(s)		(((s) > MAX_LEVEL) || ((s) < MIN_LEVEL))

/*****************************************************************************
 *
 * Prototypes
 */
/* Open sound dev */
static pj_status_t open_snd_dev(pjmedia_snd_port_param *param);
/* Close existing sound device */
static void close_snd_dev(void);
/* Create audio device param */
static pj_status_t create_aud_param(pjmedia_aud_param *param,
                                    pjmedia_aud_dev_index capture_dev,
                                    pjmedia_aud_dev_index playback_dev,
                                    unsigned clock_rate,
                                    unsigned channel_count,
                                    unsigned samples_per_frame,
                                    unsigned bits_per_sample,
                                    pj_bool_t use_default_settings);
static pj_status_t mport_put_frame(pjmedia_port *this_port, pjmedia_frame *frame);
static pj_status_t mport_get_frame(pjmedia_port *this_port, pjmedia_frame *frame);
static pj_status_t mport_on_destroy(pjmedia_port *this_port);

/*****************************************************************************
 *
 * Call API that are closely tied to PJMEDIA
 */
/*
 * Check if call has an active media session.
 */
PJ_DEF(pj_bool_t) pjsua_call_has_media(pjsua_call_id call_id)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
                     PJ_EINVAL);
    return call->audio_idx >= 0 && call->media[call->audio_idx].strm.a.stream;
}


/*
 * Get the conference port identification associated with the call.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_call_get_conf_port(pjsua_call_id call_id)
{
    pjsua_call *call;
    pjsua_conf_port_id port_id = PJSUA_INVALID_ID;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
                     PJ_EINVAL);

    /* Use PJSUA_LOCK() instead of acquire_call():
     *  https://github.com/pjsip/pjproject/issues/1371
     */
    PJSUA_LOCK();

    if (!pjsua_call_is_active(call_id))
        goto on_return;

    call = &pjsua_var.calls[call_id];
	port_id = (call->conf_slot != PJSUA_INVALID_ID) ? call->conf_slot : (call->audio_idx >= 0) ? call->media[call->audio_idx].strm.a.conf_slot : PJSUA_INVALID_ID;
    if (port_id != PJSUA_INVALID_ID)
    {
        goto on_return;
    }
    for (int mi = 0;(unsigned) mi < call->med_cnt; ++mi) {
        pjsua_call_media* call_med = &call->media[mi];
        if(call_med->strm.a.conf_slot == PJSUA_INVALID_ID || mi== call->audio_idx)
        {
            continue;
        }
        call->conf_slot = call_med->strm.a.conf_slot;
        call->audio_idx= mi;
        port_id = call->conf_slot;
    }
on_return:
    PJSUA_UNLOCK();

    return port_id;
}


/*
 * Modify audio stream's codec parameters.
 */
PJ_DEF(pj_status_t)
pjsua_call_aud_stream_modify_codec_param(pjsua_call_id call_id,
                                         int med_idx,
                                         const pjmedia_codec_param *param)
{
    pjsua_call *call;
    pjsua_call_media *call_med;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls &&
                     param, PJ_EINVAL);

    PJSUA_LOCK();

    /* Verify media index */
    call = &pjsua_var.calls[call_id];
    if (med_idx == -1) {
        med_idx = call->audio_idx;
    }
    if (med_idx < 0 || med_idx >= (int)call->med_cnt) {
        PJSUA_UNLOCK();
        return PJ_EINVAL;
    }

    /* Verify if the media is audio */
    call_med = &call->media[med_idx];
    if (call_med->type != PJMEDIA_TYPE_AUDIO || !call_med->strm.a.stream) {
        PJSUA_UNLOCK();
        return PJ_EINVALIDOP;
    }

    status = pjmedia_stream_modify_codec_param(call_med->strm.a.stream,
                                               param);

    PJSUA_UNLOCK();
    return status;
}


/*
 * Get media stream info for the specified media index.
 */
PJ_DEF(pj_status_t) pjsua_call_get_stream_info( pjsua_call_id call_id,
                                                unsigned med_idx,
                                                pjsua_stream_info *psi)
{
    pjsua_call *call;
    pjsua_call_media *call_med;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(psi, PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (med_idx >= call->med_cnt) {
        PJSUA_UNLOCK();
        return PJ_EINVAL;
    }

    call_med = &call->media[med_idx];
    psi->type = call_med->type;
    switch (call_med->type) {
    case PJMEDIA_TYPE_AUDIO:
        status = pjmedia_stream_get_info(call_med->strm.a.stream,
                                         &psi->info.aud);
        break;
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    case PJMEDIA_TYPE_VIDEO:
        status = pjmedia_vid_stream_get_info(call_med->strm.v.stream,
                                             &psi->info.vid);
        break;
#endif
    default:
        status = PJMEDIA_EINVALIMEDIATYPE;
        break;
    }

    PJSUA_UNLOCK();
    return status;
}


/*
 *  Get media stream statistic for the specified media index.
 */
PJ_DEF(pj_status_t) pjsua_call_get_stream_stat( pjsua_call_id call_id,
                                                unsigned med_idx,
                                                pjsua_stream_stat *stat)
{
    pjsua_call *call;
    pjsua_call_media *call_med;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(stat, PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (med_idx >= call->med_cnt) {
        PJSUA_UNLOCK();
        return PJ_EINVAL;
    }

    call_med = &call->media[med_idx];
    switch (call_med->type) {
    case PJMEDIA_TYPE_AUDIO:
        status = pjmedia_stream_get_stat(call_med->strm.a.stream,
                                         &stat->rtcp);
        if (status == PJ_SUCCESS)
            status = pjmedia_stream_get_stat_jbuf(call_med->strm.a.stream,
                                                  &stat->jbuf);
        break;
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    case PJMEDIA_TYPE_VIDEO:
        status = pjmedia_vid_stream_get_stat(call_med->strm.v.stream,
                                             &stat->rtcp);
        if (status == PJ_SUCCESS)
            status = pjmedia_vid_stream_get_stat_jbuf(call_med->strm.v.stream,
                                                  &stat->jbuf);
        break;
#endif
    default:
        status = PJMEDIA_EINVALIMEDIATYPE;
        break;
    }

    PJSUA_UNLOCK();
    return status;
}

/*
 * Send DTMF digits to remote using RFC 2833 payload formats.
 */
PJ_DEF(pj_status_t) pjsua_call_dial_dtmf( pjsua_call_id call_id,
                                          const pj_str_t *digits)
{
    pjsua_call *call;
    pjsip_dialog *dlg = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
                     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Call %d dialing DTMF %.*s",
                         call_id, (int)digits->slen, digits->ptr));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_dial_dtmf()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
        goto on_return;

    if (!pjsua_call_has_media(call_id)) {
        PJ_LOG(3,(THIS_FILE, "Media is not established yet!"));
        status = PJ_EINVALIDOP;
        goto on_return;
    }

    status = pjmedia_stream_dial_dtmf(
                call->media[call->audio_idx].strm.a.stream, digits);

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
}

PJ_DEF(pj_status_t) pjsua_call_get_queued_dtmf_digits(pjsua_call_id call_id,
	unsigned *digits)
{
	pjsua_call *call;
	pjsip_dialog *dlg = NULL;
	pj_status_t status;

	PJ_ASSERT_RETURN(digits, PJ_EINVAL);
	*digits = 0U;
	PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
			PJ_EINVAL);

	pj_log_push_indent();

	status = acquire_call("pjsua_call_get_queued_dtmf_digits()", call_id, &call, &dlg);
	if (status != PJ_SUCCESS)
		goto on_return;

	if (!pjsua_call_has_media(call_id)) {
		PJ_LOG(3,(THIS_FILE, "Media is not established yet!"));
		status = PJ_EINVALIDOP;
		goto on_return;
	}

	status = pjmedia_get_queued_dtmf_digits(
		call->media[call->audio_idx].strm.a.stream, digits);

on_return:
	if (dlg) pjsip_dlg_dec_lock(dlg);
	pj_log_pop_indent();
	return status;
}


/*****************************************************************************
 *
 * Audio media with PJMEDIA backend
 */

static void mport_rec_frame(pjsua_mport_data *data, pjmedia_frame *frame)
{
	pj_size_t size, avl, count;
	pj_bool_t is_silence = PJ_FALSE;
	pj_size_t silence_samples = 0;
	pj_int16_t *buf = (pj_int16_t *)frame->buf;

	if (data->record_data.status != PJSUA_REC_RUNNING)
		return;

	if (frame->type == PJMEDIA_FRAME_TYPE_NONE)
	{
		count = size = pjsua_var.mconf_cfg.samples_per_frame;
		buf = (pj_int16_t *)_alloca(sizeof(*buf) * size);
		pj_bzero(buf, sizeof(*buf) * size);
	}
	else
	{
		count = size = (frame->size >> 1);
	}

	pj_assert(data->record_data.buffer != NULL);

	if (data->record_data.max_samples)
	{
		pj_assert(data->record_data.max_samples > data->record_data.samples_recorded);
		avl = (pj_size_t)((pj_uint64_t)data->record_data.max_samples - data->record_data.samples_recorded);
		if (count > avl)
			count = avl;
	}
	if (data->record_data.max_duration)
	{
		pj_assert(data->record_data.max_duration > data->record_data.samples_seen);
		avl = data->record_data.max_duration - (pj_size_t)data->record_data.samples_seen;
		if (count > avl)
			count = avl;
	}
	if ((data->record_data.max_silence || data->record_data.eliminate_silence) && data->record_data.vad)
	{
		is_silence = pjmedia_silence_det_detect(data->record_data.vad, (const pj_int16_t *)buf, size, NULL);
		if (is_silence)
		{
			if (!data->record_data.is_silence)
			{
				data->record_data.is_silence = PJ_TRUE;
				data->record_data.vad_timestamp = frame->timestamp;
			}
			else
			{
				pj_assert(frame->timestamp.u64 > data->record_data.vad_timestamp.u64);
			}
			silence_samples = (pj_size_t)(frame->timestamp.u64 - data->record_data.vad_timestamp.u64);
			if (data->record_data.eliminate_silence)
			{
				if ((silence_samples + count) >= data->record_data.eliminate_silence)
				{
					count = 0;
				}
				else
				{
					avl = (data->record_data.eliminate_silence - silence_samples);
					if (count > avl)
						count = avl;
				}
			}
			if (data->record_data.max_silence)
			{
				pj_assert(data->record_data.max_silence > silence_samples);
				avl = silence_samples - data->record_data.max_silence;
				if (count > avl)
					count = avl;
			}
		}
		else if (data->record_data.is_silence)
		{
			data->record_data.is_silence = PJ_FALSE;
			data->record_data.vad_timestamp = frame->timestamp;
		}
	}

	pj_assert(data->record_data.buffer->capacity >= data->record_data.buffer->len);
	avl = (data->record_data.buffer->capacity - data->record_data.buffer->len);
	if (count > avl)
	{
		if (!data->record_data.overrun)
		{
			PJ_LOG(3, (THIS_FILE, "Record buffer overrun for %s: avl=%lu, size=%lu", data->base.info.name.ptr, avl, size));
			data->record_data.overrun = PJ_TRUE;
		}
		if ((data->base.info.fmt.type == PJMEDIA_TYPE_AUDIO) &&
			(data->base.info.fmt.detail_type == PJMEDIA_FORMAT_DETAIL_AUDIO) &&
			(data->base.info.fmt.det.aud.channel_count > 1))
		{
			// Take account of block size and ensure that only complete blocks are buffered
			count = avl % data->base.info.fmt.det.aud.channel_count;
			if (!count)
				count = avl;
			else
				count = avl - count;
		}
		else
		{
			count = avl;
		}
	}
	if (count)
	{
		pjmedia_circ_buf_write(data->record_data.buffer, buf, (unsigned int)count);
		data->record_data.samples_recorded += count;
		avl -= count;
	}
	data->record_data.samples_seen += size;

	do {
		if (data->record_data.max_samples && (data->record_data.samples_recorded >= data->record_data.max_samples))
		{
			data->record_data.status = PJSUA_REC_STOPPING;
			data->record_data.er = PJSUA_REC_ER_MAX_SAMPLES;
			break;
		}
		if (data->record_data.max_duration && (data->record_data.samples_seen >= data->record_data.max_duration))
		{
			data->record_data.status = PJSUA_REC_STOPPING;
			data->record_data.er = PJSUA_REC_ER_MAX_DURATION;
			break;
		}
		if (data->record_data.max_silence && ((silence_samples + size) >= data->record_data.max_silence))
		{
			data->record_data.status = PJSUA_REC_STOPPING;
			data->record_data.er = PJSUA_REC_ER_MAX_SILENCE;
			break;
		}
	} while (0);

	// Notify the application to collect the buffered data when the available space
	// falls below the data threshold or if we've encountered a termination
	// condition
	if (!data->record_data.signaled &&
		((avl < data->record_data.threshold) || (data->record_data.status == PJSUA_REC_STOPPING)))
	{
		data->record_data.signaled = PJ_TRUE;
		pj_event_set(data->record_data.event);
	}

	if (data->record_data.status == PJSUA_REC_STOPPING)
	{
		if (data->record_data.rec_output)
			pjmedia_conf_configure_port(pjsua_var.mconf, data->slot, PJMEDIA_PORT_NO_CHANGE, PJMEDIA_PORT_ENABLE);
		else
			pjmedia_conf_configure_port(pjsua_var.mconf, data->slot, PJMEDIA_PORT_ENABLE, PJMEDIA_PORT_NO_CHANGE);
	}
}

static pj_status_t mport_put_frame(pjmedia_port *this_port, pjmedia_frame *frame)
{
	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVALIDOP);

	pjsua_mport_data *data = (pjsua_mport_data*) this_port;
	if ((data->record_data.status == PJSUA_REC_RUNNING) &&
		!data->record_data.rec_output)
	{
		pj_enter_critical_section();
		mport_rec_frame(data, frame);
		pj_leave_critical_section();
	}

	if ((frame->type == PJMEDIA_FRAME_TYPE_AUDIO) &&
		(frame->size > 0U) &&
		(data->listener_cnt > 0U))
	{
		const pj_size_t samples = frame->size >> 1;
		pj_assert(samples == pjsua_var.mconf_cfg.samples_per_frame);
		pj_enter_critical_section();
		pj_assert(data->listeners != NULL);
		for (register pj_uint32_t i = 0; i < data->listener_cnt; ++i)
		{
			const pjsua_mport_id id = data->listeners[i];
			pjsua_mport_data *listener;
			pj_int32_t *mix_buf;
			pj_int16_t *buf;
			pj_size_t j;
			pj_assert((id >= 0) && (id < (pjsua_mport_id)pjsua_var.media_cfg.max_media_ports));
			listener = &pjsua_var.mport[id];
			mix_buf = listener->mix_buf;
			buf = (pj_int16_t *)frame->buf;
			if (listener->mix_cnt++ > 0)
			{
				for (j = 0; j < samples; ++j)
				{
					*mix_buf += *buf++;
					if (IS_OVERFLOW(*mix_buf))
					{
						int tmp_adj = (MAX_LEVEL << 7) / *mix_buf;
						if (tmp_adj < 0)
							tmp_adj = -tmp_adj;
						if (tmp_adj < listener->mix_adj)
							listener->mix_adj = tmp_adj;
					}
					mix_buf++;
				}
			}
			else
			{
				for (j = 0; j < samples; ++j)
				{
					*mix_buf++ = *buf++;
				}
			}
		}
		pj_leave_critical_section();
	}

	return PJ_SUCCESS;
}

static pj_status_t mport_get_frame(pjmedia_port *this_port, pjmedia_frame *frame)
{
	pjsua_mport_data *data;
	pj_size_t i, size, avl, count;

	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVALIDOP);

	data = (pjsua_mport_data*) this_port;

	size = pjsua_var.mconf_cfg.samples_per_frame;

	frame->timestamp.u64 = data->play_data.timestamp.u64;
	data->play_data.timestamp.u64 += size;

	pj_enter_critical_section();

	if (data->play_data.status <= PJSUA_REP_IDLE)
	{
		frame->size = 0;
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
	}
	else if (data->play_data.status == PJSUA_REP_CONFERENCING)
	{
		if (data->mix_cnt)
		{
			pj_int16_t *buf = (pj_int16_t *)frame->buf;
			SIMPLE_AGC(data->last_mix_adj, data->mix_adj);
			data->last_mix_adj = data->mix_adj;
			pj_assert(data->mix_buf != NULL);
			if (data->mix_adj != NORMAL_LEVEL)
			{
				for (i = 0; i < size; ++i)
				{
					pj_int32_t s = data->mix_buf[i];
					s = (s * data->mix_adj) >> 7;
					if (s > MAX_LEVEL) s = MAX_LEVEL;
					else if (s < MIN_LEVEL) s = MIN_LEVEL;
					*buf++ = (pj_int16_t)s;
				}
				data->mix_adj = NORMAL_LEVEL;
			}
			else
			{
				for (i = 0; i < size; ++i)
				{
					*buf++ = (pj_int16_t)data->mix_buf[i];
				}
			}
			data->mix_cnt = 0U;
			frame->size = size << 1;
			frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
		}
		else
		{
			frame->size = 0;
			frame->type = PJMEDIA_FRAME_TYPE_NONE;
		}
	}
	else
	{
		pj_assert(data->play_data.buffer != NULL);
		avl = data->play_data.buffer->len;
		count = size;
		if (count > avl)
		{
			if (!data->play_data.underrun && (avl || data->play_data.samples_played) && (data->play_data.status == PJSUA_REP_RUNNING))
			{
				PJ_LOG(3, (THIS_FILE, "Replay buffer underrun for %s: avl=%lu, size=%lu", this_port->info.name.ptr, avl, size));
				data->play_data.underrun = PJ_TRUE;
			}
			count = avl;
		}
		if (count)
		{
			pjmedia_circ_buf_read(data->play_data.buffer, (pj_int16_t*)frame->buf, (unsigned int)count);
			avl -= count;
			data->play_data.samples_played += count;
		}

		// Pad with zeroes if necessary
		if (count < size)
			pj_bzero((pj_int16_t*)(frame->buf) + count, (size - count) << 1);

		// Notify the application when the remaining data falls below the
		// threshold or if the replay has completed
		if (!data->play_data.signaled &&
			(((data->play_data.status == PJSUA_REP_STOPPING) && !avl) ||
			((data->play_data.status == PJSUA_REP_RUNNING) && (avl < data->play_data.threshold))))
		{
			data->play_data.signaled = PJ_TRUE;
			pj_event_set(data->play_data.event);
		}

		frame->size = size << 1;
		frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	}

	if ((data->record_data.status == PJSUA_REC_RUNNING) &&
		data->record_data.rec_output)
		mport_rec_frame(data, frame);

	pj_leave_critical_section();

	return PJ_SUCCESS;
}


static pj_status_t mport_on_destroy(pjmedia_port *this_port)
{
	pjsua_mport_data *data;

	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVALIDOP);

	data = (pjsua_mport_data*) this_port;

	pj_enter_critical_section();

	if (data->record_data.buffer)
		pjmedia_circ_buf_reset(data->record_data.buffer);
	if (data->record_data.status == PJSUA_REC_RUNNING)
		data->record_data.status = PJSUA_REC_STOPPING;
	if ((data->record_data.status != PJSUA_REC_IDLE) && !data->record_data.signaled && data->record_data.event)
	{
		data->record_data.signaled = PJ_TRUE;
		pj_event_set(data->record_data.event);
	}

	if (data->play_data.buffer)
		pjmedia_circ_buf_reset(data->play_data.buffer);
	if (data->play_data.status == PJSUA_REP_RUNNING)
		data->play_data.status = PJSUA_REP_STOPPING;
	if ((data->play_data.status == PJSUA_REP_STOPPING) && !data->play_data.signaled && data->play_data.event)
	{
		data->play_data.signaled = PJ_TRUE;
		pj_event_set(data->play_data.event);
	}

	pj_leave_critical_section();

	if (data->play_data.status == PJSUA_REP_CONFERENCING)
		pjsua_mport_conf_stop((pjsua_mport_id)(data - pjsua_var.mport));

	return PJ_SUCCESS;
}

static pj_status_t init_mport(pjsua_mport_id id)
{
	pj_status_t status;
	char buf[PJ_MAX_OBJ_NAME];
	pj_str_t name;
	register pjsua_mport_data *p = &pjsua_var.mport[id];
	//pj_bzero(p, sizeof(*p));
	p->slot = PJSUA_INVALID_ID;
	_snprintf(buf, sizeof(buf), "mp%d", id);
	buf[sizeof(buf)-1] = '\0';
	pj_strdup2_with_null(pjsua_var.pool, &name, buf);
	status = pjmedia_port_info_init(&p->base.info, &name, SIGNATURE, pjsua_var.media_cfg.clock_rate,
		pjsua_var.mconf_cfg.channel_count, pjsua_var.mconf_cfg.bits_per_sample, pjsua_var.mconf_cfg.samples_per_frame);
	if (status == PJ_SUCCESS)
	{
		p->base.port_data.ldata = id;
		p->base.put_frame = &mport_put_frame;
		p->base.get_frame = &mport_get_frame;
		p->base.on_destroy = &mport_on_destroy;
	}
	return status;
}

static void deinit_mport(pjsua_mport_id id)
{
	pjsua_mport_free(id);
	register pjsua_mport_data *p = &pjsua_var.mport[id];
	pjmedia_port_destroy(&p->base);
	p->base.info.signature = 0;
}

/* Init pjmedia audio subsystem */
pj_status_t pjsua_aud_subsys_init()
{
    pj_str_t codec_id = {NULL, 0};
    unsigned opt;
    pjmedia_audio_codec_config codec_cfg;
    pj_status_t status;

    /* To suppress warning about unused var when all codecs are disabled */
    PJ_UNUSED_ARG(codec_id);

    /*
     * Register all codecs
     */
    pjmedia_audio_codec_config_default(&codec_cfg);
    codec_cfg.speex.quality = pjsua_var.media_cfg.quality;
    codec_cfg.speex.complexity = -1;
    codec_cfg.ilbc.mode = pjsua_var.media_cfg.ilbc_mode;

#if PJMEDIA_HAS_PASSTHROUGH_CODECS
    /* Register passthrough codecs */
    {
        unsigned aud_idx;
        unsigned ext_fmt_cnt = 0;
        pjmedia_format ext_fmts[32];

        /* List extended formats supported by audio devices */
        for (aud_idx = 0; aud_idx < pjmedia_aud_dev_count(); ++aud_idx) {
            pjmedia_aud_dev_info aud_info;
            unsigned i;

            status = pjmedia_aud_dev_get_info(aud_idx, &aud_info);
            if (status != PJ_SUCCESS) {
                pjsua_perror(THIS_FILE, "Error querying audio device info",
                             status);
                goto on_error;
            }

            /* Collect extended formats supported by this audio device */
            for (i = 0; i < aud_info.ext_fmt_cnt; ++i) {
                unsigned j;
                pj_bool_t is_listed = PJ_FALSE;

                /* See if this extended format is already in the list */
                for (j = 0; j < ext_fmt_cnt && !is_listed; ++j) {
                    if (ext_fmts[j].id == aud_info.ext_fmt[i].id &&
                        ext_fmts[j].det.aud.avg_bps ==
                        aud_info.ext_fmt[i].det.aud.avg_bps)
                    {
                        is_listed = PJ_TRUE;
                    }
                }

                /* Put this format into the list, if it is not in the list */
                if (!is_listed)
                    ext_fmts[ext_fmt_cnt++] = aud_info.ext_fmt[i];

                pj_assert(ext_fmt_cnt <= PJ_ARRAY_SIZE(ext_fmts));
            }
        }

        /* Init the passthrough codec with supported formats only */
        codec_cfg.passthrough.setting.fmt_cnt = ext_fmt_cnt;
        codec_cfg.passthrough.setting.fmts = ext_fmts;
        codec_cfg.passthrough.setting.ilbc_mode =
            pjsua_var.media_cfg.ilbc_mode;
    }
#endif /* PJMEDIA_HAS_PASSTHROUGH_CODECS */

    /* Register all codecs */
    status = pjmedia_codec_register_audio_codecs(pjsua_var.med_endpt,
                                                 &codec_cfg);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Error registering codecs", status);
        goto on_error;
    }

    /* Set speex/16000 to higher priority*/
    codec_id = pj_str("speex/16000");
    pjmedia_codec_mgr_set_codec_priority(
        pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
        &codec_id, PJMEDIA_CODEC_PRIO_NORMAL+2);

    /* Set speex/8000 to next higher priority*/
    codec_id = pj_str("speex/8000");
    pjmedia_codec_mgr_set_codec_priority(
        pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
        &codec_id, PJMEDIA_CODEC_PRIO_NORMAL+1);

    /* Disable ALL L16 codecs */
    codec_id = pj_str("L16");
    pjmedia_codec_mgr_set_codec_priority(
        pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
        &codec_id, PJMEDIA_CODEC_PRIO_DISABLED);


    /* Save additional conference bridge parameters for future
     * reference.
     */
    pjsua_var.mconf_cfg.channel_count = pjsua_var.media_cfg.channel_count;
    pjsua_var.mconf_cfg.bits_per_sample = 16;
    pjsua_var.mconf_cfg.samples_per_frame = pjsua_var.media_cfg.clock_rate *
                                            pjsua_var.mconf_cfg.channel_count *
                                            pjsua_var.media_cfg.audio_frame_ptime /
                                            1000;

    /* Init options for conference bridge. */
    opt = PJMEDIA_CONF_NO_DEVICE;
    if (pjsua_var.media_cfg.quality >= 3 &&
        pjsua_var.media_cfg.quality <= 4)
    {
        opt |= PJMEDIA_CONF_SMALL_FILTER;
    }
    else if (pjsua_var.media_cfg.quality < 3) {
        opt |= PJMEDIA_CONF_USE_LINEAR;
    }

    /* Init conference bridge. */
    status = pjmedia_conf_create(pjsua_var.pool,
                                 pjsua_var.media_cfg.max_media_ports,
                                 pjsua_var.media_cfg.clock_rate,
                                 pjsua_var.mconf_cfg.channel_count,
                                 pjsua_var.mconf_cfg.samples_per_frame,
                                 pjsua_var.mconf_cfg.bits_per_sample,
                                 opt, &pjsua_var.mconf);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Error creating conference bridge",
                     status);
        goto on_error;
    }

    /* Are we using the audio switchboard (a.k.a APS-Direct)? */
    pjsua_var.is_mswitch = pjmedia_conf_get_master_port(pjsua_var.mconf)
                            ->info.signature == PJMEDIA_CONF_SWITCH_SIGNATURE;

    /* Create null port just in case user wants to use null sound. */
    status = pjmedia_null_port_create(pjsua_var.pool,
                                      pjsua_var.media_cfg.clock_rate,
                                      pjsua_var.mconf_cfg.channel_count,
                                      pjsua_var.mconf_cfg.samples_per_frame,
                                      pjsua_var.mconf_cfg.bits_per_sample,
                                      &pjsua_var.null_port);
	if (status != PJ_SUCCESS)
	{
		pjsua_perror(THIS_FILE, "Error creating null port", status);
		goto on_error;
	}

	/* Initialise the media ports */
	pjsua_var.mport_cnt = 0U;
	pjsua_var.mport_id = -1;
	pjsua_var.mport = (pjsua_mport_data*)pj_pool_zalloc(pjsua_var.pool, sizeof(pjsua_mport_data) * pjsua_var.media_cfg.max_media_ports);
	if (!pjsua_var.mport)
	{
		status = PJ_ENOMEM;
		pjsua_perror(THIS_FILE, "Error allocating media ports", status);
		goto on_error;
	}

	/* Init media port array. */
	for (unsigned i = 0u; i < pjsua_var.media_cfg.max_media_ports; ++i)
	{
		status = init_mport(i);
		if (status != PJ_SUCCESS) {
			pjsua_perror(THIS_FILE, "Error initialising media port", status);
			goto on_error;
		}
	}

    return status;

on_error:
    return status;
}

/* Check if sound device is idle. */
void pjsua_check_snd_dev_idle()
{
    unsigned call_cnt;

    /* Check if the sound device auto-close feature is disabled. */
    if (pjsua_var.media_cfg.snd_auto_close_time < 0)
        return;

    /* Check if the sound device is currently closed. */
    if (!pjsua_var.snd_is_on)
        return;

    /* Get the call count, we shouldn't close the sound device when there is
     * any calls active.
     */
    call_cnt = pjsua_call_get_count();

    /* When this function is called from pjsua_media_channel_deinit() upon
     * disconnecting call, actually the call count hasn't been updated/
     * decreased. So we put additional check here, if there is only one
     * call and it's in DISCONNECTED state, there is actually no active
     * call.
     */
    if (call_cnt == 1) {
        pjsua_call_id call_id;
        pj_status_t status;

        status = pjsua_enum_calls(&call_id, &call_cnt);
        if (status == PJ_SUCCESS && call_cnt > 0 &&
            !pjsua_call_is_active(call_id))
        {
            call_cnt = 0;
        }
    }

    /* Activate sound device auto-close timer if sound device is idle.
     * It is idle when there is no port connection in the bridge and
     * there is no active call.
     */
    if (pjsua_var.snd_idle_timer.id == PJ_FALSE &&
        call_cnt == 0 &&
        pjmedia_conf_get_connect_count(pjsua_var.mconf) == 0)
    {
        pj_time_val delay;

        delay.msec = 0;
        delay.sec = pjsua_var.media_cfg.snd_auto_close_time;

        pjsua_var.snd_idle_timer.id = PJ_TRUE;
        pjsip_endpt_schedule_timer(pjsua_var.endpt, &pjsua_var.snd_idle_timer,
                                   &delay);
    }
}

/* Timer callback to close sound device */
static void close_snd_timer_cb( pj_timer_heap_t *th,
                                pj_timer_entry *entry)
{
    PJ_UNUSED_ARG(th);

    PJSUA_LOCK();
    if (entry->id) {
        PJ_LOG(4,(THIS_FILE,"Closing sound device after idle for %d second(s)",
                  pjsua_var.media_cfg.snd_auto_close_time));

        entry->id = PJ_FALSE;

        close_snd_dev();
    }
    PJSUA_UNLOCK();
}

pj_status_t pjsua_aud_subsys_start(void)
{
    pj_status_t status = PJ_SUCCESS;

    pj_timer_entry_init(&pjsua_var.snd_idle_timer, PJ_FALSE, NULL,
                        &close_snd_timer_cb);

    pjsua_check_snd_dev_idle();
    return status;
}

pj_status_t pjsua_aud_subsys_destroy()
{
    unsigned i;

	if (pjsua_var.mport != NULL)
	{
		for (i = 0U; i < pjsua_var.media_cfg.max_media_ports; ++i)
			deinit_mport(i);
		pjsua_var.mport = NULL;
	}
	pjsua_var.mport_cnt = 0U;

    close_snd_dev();

    /* Destroy file players */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.player); ++i) {
        if (pjsua_var.player[i].port) {
            PJ_LOG(2,(THIS_FILE, "Destructor for player id=%d "
                      "is not called", i));
            pjsua_player_destroy(i);
        }
    }

    /* Destroy file recorders */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.recorder); ++i) {
        if (pjsua_var.recorder[i].port) {
            PJ_LOG(2,(THIS_FILE, "Destructor for recorder id=%d "
                      "is not called", i));
            pjsua_recorder_destroy(i);
        }
    }

    if (pjsua_var.mconf) {
        pjmedia_conf_destroy(pjsua_var.mconf);
        pjsua_var.mconf = NULL;
    }

    if (pjsua_var.null_port) {
        pjmedia_port_destroy(pjsua_var.null_port);
        pjsua_var.null_port = NULL;
    }

    return PJ_SUCCESS;
}

void remove_conf_port(pjsua_conf_port_id* id)
{
    if (pjsua_var.mconf) {
        if (pjsua_conf_remove_port(*id) == PJ_SUCCESS)
        {
            PJ_LOG(4, (THIS_FILE, "pjsua_conf_remove_port done id: %d", *id));
        }
        else
        {
            PJ_LOG(2, (THIS_FILE, "pjsua_conf_remove_port failed"));
        }
    }
    *id = PJSUA_INVALID_ID;
}

void pjsua_aud_stop_stream(pjsua_call_media *call_med)
{
    pjmedia_stream *strm = call_med->strm.a.stream;
    pjmedia_rtcp_stat stat;

    if (strm) {
        pjmedia_stream_get_info(strm, &call_med->prev_aud_si);

        /* Unsubscribe from stream events */
        pjmedia_event_unsubscribe(NULL, &call_media_on_event, call_med, strm);

        pjmedia_stream_send_rtcp_bye(strm);
        pjsua_call* call = call_med->call;
        if(call_med->strm.a.conf_slot != PJSUA_INVALID_ID )
        {
            if (call->conf_slot == call_med->strm.a.conf_slot)
                call->conf_slot = PJSUA_INVALID_ID;
            remove_conf_port(&call_med->strm.a.conf_slot);
        }
        else
        {
            PJ_LOG(4, (THIS_FILE, "pjsua_aud_stop_stream::stream is invalid"));
        }

        /* Don't check for direction and transmitted packets count as we
         * assume that RTP timestamp remains increasing when outgoing
         * direction is disabled/paused.
         */
        //if ((call_med->dir & PJMEDIA_DIR_ENCODING) &&
        //    (pjmedia_stream_get_stat(strm, &stat) == PJ_SUCCESS) &&
        //    stat.tx.pkt)
        if (pjmedia_stream_get_stat(strm, &stat) == PJ_SUCCESS)
        {
            /* Save RTP timestamp & sequence, so when media session is
             * restarted, those values will be restored as the initial
             * RTP timestamp & sequence of the new media session. So in
             * the same call session, RTP timestamp and sequence are
             * guaranteed to be contigue.
             */
            call_med->rtp_tx_seq_ts_set = 1 | (1 << 1);
            call_med->rtp_tx_seq = stat.rtp_tx_last_seq;
            call_med->rtp_tx_ts = stat.rtp_tx_last_ts;
        }

        if (!call_med->call->hanging_up &&
            pjsua_var.ua_cfg.cb.on_stream_destroyed)
        {
            pjsua_var.ua_cfg.cb.on_stream_destroyed(call_med->call->index,
                                                    strm, call_med->idx);
        }

        if (call_med->strm.a.media_port) {
            if (call_med->strm.a.destroy_port)
                pjmedia_port_destroy(call_med->strm.a.media_port);
            call_med->strm.a.media_port = NULL;
        }
        pjmedia_stream_destroy(strm);
        call_med->strm.a.stream = NULL;
    }

    pjsua_check_snd_dev_idle();
}

/*
 * DTMF callback from the stream.
 */
static void dtmf_callback(pjmedia_stream *strm, void *user_data,
                          int digit)
{
    pjsua_call_id call_id;

    PJ_UNUSED_ARG(strm);

    call_id = (pjsua_call_id)(pj_ssize_t)user_data;
    if (pjsua_var.calls[call_id].hanging_up)
        return;

    pj_log_push_indent();

    if (pjsua_var.ua_cfg.cb.on_dtmf_digit2) {
        pjsua_dtmf_info info;

        info.method = PJSUA_DTMF_METHOD_RFC2833;
        info.digit = digit;
        info.duration = PJSUA_UNKNOWN_DTMF_DURATION;
        (*pjsua_var.ua_cfg.cb.on_dtmf_digit2)(call_id, &info);
    } else if (pjsua_var.ua_cfg.cb.on_dtmf_digit) {
        /* For discussions about call mutex protection related to this
         * callback, please see ticket #460:
         *      https://github.com/pjsip/pjproject/issues/460#comment:4
         */    
        (*pjsua_var.ua_cfg.cb.on_dtmf_digit)(call_id, digit);
    }

    pj_log_pop_indent();
}

/*
 * DTMF callback from the stream.
 */
static void dtmf_event_callback(pjmedia_stream *strm, void *user_data,
                                const pjmedia_stream_dtmf_event *event)
{
    pjsua_call_id call_id;
    pjsua_dtmf_event evt;

    PJ_UNUSED_ARG(strm);

    call_id = (pjsua_call_id)(pj_ssize_t)user_data;
    if (pjsua_var.calls[call_id].hanging_up)
        return;

    pj_log_push_indent();

    if (pjsua_var.ua_cfg.cb.on_dtmf_event) {
        evt.method = PJSUA_DTMF_METHOD_RFC2833;
        evt.timestamp = event->timestamp;
        evt.digit = event->digit;
        evt.duration = event->duration;
        evt.flags = event->flags;
        (*pjsua_var.ua_cfg.cb.on_dtmf_event)(call_id, &evt);
    }

    pj_log_pop_indent();
}
pj_bool_t compare_port_signature(pjsua_conf_port_id conf_slot, pj_uint32_t signature)
{
    if (conf_slot == PJSUA_INVALID_ID)
    {
        return PJ_FALSE;
    }
    pjmedia_port_info port_info;
    if(pjmedia_conf_get_media_port_info(pjsua_var.mconf, conf_slot, &port_info)!=PJ_SUCCESS)
    {
        return PJ_FALSE;
    }
    return port_info.signature == signature;
}
/* Internal function: update audio channel after SDP negotiation.
 * Warning: do not use temporary/flip-flop pool, e.g: inv->pool_prov,
 *          for creating stream, etc, as after SDP negotiation and when
 *          the SDP media is not changed, the stream should remain running
 *          while the temporary/flip-flop pool may be released.
 */
pj_status_t pjsua_aud_channel_update(pjsua_call_media *call_med,
                                     pj_pool_t *tmp_pool,
                                     pjmedia_stream_info *si,
                                     const pjmedia_sdp_session *local_sdp,
                                     const pjmedia_sdp_session *remote_sdp)
{
    pjsua_call *call = call_med->call;
    unsigned strm_idx = call_med->idx;
    pj_status_t status = PJ_SUCCESS;

    PJ_UNUSED_ARG(tmp_pool);
    PJ_UNUSED_ARG(local_sdp);
    PJ_UNUSED_ARG(remote_sdp);

    PJ_LOG(4,(THIS_FILE,"Audio channel update for index %d for call %d...",
		call_med->idx, call_med->call->index));
    pj_log_push_indent();

    si->rtcp_sdes_bye_disabled = pjsua_var.media_cfg.no_rtcp_sdes_bye;

    /* Check if no media is active */
    if (local_sdp->media[strm_idx]->desc.port != 0) {

        /* Optionally, application may modify other stream settings here
         * (such as jitter buffer parameters, codec ptime, etc.)
         */
        si->jb_init = pjsua_var.media_cfg.jb_init;
        si->jb_min_pre = pjsua_var.media_cfg.jb_min_pre;
        si->jb_max_pre = pjsua_var.media_cfg.jb_max_pre;
        si->jb_max = pjsua_var.media_cfg.jb_max;
        si->jb_discard_algo = pjsua_var.media_cfg.jb_discard_algo;

        /* Set SSRC and CNAME */
        si->ssrc = call_med->ssrc;
        si->cname = call->cname;

        /* Set RTP timestamp & sequence, normally these value are intialized
         * automatically when stream session created, but for some cases (e.g:
         * call reinvite, call update) timestamp and sequence need to be kept
         * contigue.
         */
        si->rtp_ts = call_med->rtp_tx_ts;
        si->rtp_seq = call_med->rtp_tx_seq;
        si->rtp_seq_ts_set = call_med->rtp_tx_seq_ts_set;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
        /* Enable/disable stream keep-alive and NAT hole punch. */
        si->use_ka = pjsua_var.acc[call->acc_id].cfg.use_stream_ka;

        si->ka_cfg = pjsua_var.acc[call->acc_id].cfg.stream_ka_cfg;
#endif

        if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_stream_precreate) {
            pjsua_on_stream_precreate_param prm;
            prm.stream_idx = strm_idx;
            prm.stream_info.type = PJMEDIA_TYPE_AUDIO;
            prm.stream_info.info.aud = *si;
            (*pjsua_var.ua_cfg.cb.on_stream_precreate)(call->index, &prm);

            /* Copy back only the fields which are allowed to be changed. */
            si->jb_init = prm.stream_info.info.aud.jb_init;
            si->jb_min_pre = prm.stream_info.info.aud.jb_min_pre;
            si->jb_max_pre = prm.stream_info.info.aud.jb_max_pre;
            si->jb_max = prm.stream_info.info.aud.jb_max;
            si->jb_discard_algo = prm.stream_info.info.aud.jb_discard_algo;
#if defined(PJMEDIA_STREAM_ENABLE_KA) && (PJMEDIA_STREAM_ENABLE_KA != 0)
            si->use_ka = prm.stream_info.info.aud.use_ka;
#endif
            si->rtcp_sdes_bye_disabled = prm.stream_info.info.aud.rtcp_sdes_bye_disabled;
        }

        /* Create session based on session info. */
        status = pjmedia_stream_create(pjsua_var.med_endpt, NULL, si,
                                       call_med->tp, NULL,
                                       &call_med->strm.a.stream);
        if (status != PJ_SUCCESS) {
            goto on_return;
        }

        /* Start stream */
        status = pjmedia_stream_start(call_med->strm.a.stream);
        if (status != PJ_SUCCESS) {
            goto on_return;
        }

        if (call_med->prev_state == PJSUA_CALL_MEDIA_NONE)
            pjmedia_stream_send_rtcp_sdes(call_med->strm.a.stream);

        /* If DTMF callback is installed by application, install our
         * callback to the session.
         */
        if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_dtmf_event) {
            pjmedia_stream_set_dtmf_event_callback(call_med->strm.a.stream,
                                              &dtmf_event_callback,
                                              (void*)(pj_ssize_t)(call->index));
        } else if (!call->hanging_up &&
                   (pjsua_var.ua_cfg.cb.on_dtmf_digit || 
                    pjsua_var.ua_cfg.cb.on_dtmf_digit2))
        {
            pjmedia_stream_set_dtmf_callback(call_med->strm.a.stream,
                                             &dtmf_callback,
                                             (void*)(pj_ssize_t)(call->index));
        }

        /* Get the port interface of the first stream in the session.
         * We need the port interface to add to the conference bridge.
         */
        pjmedia_stream_get_port(call_med->strm.a.stream,
                                &call_med->strm.a.media_port);

        /* Notify application about stream creation.
         * Note: application may modify media_port to point to different
         * media port
         */
        if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_stream_created2) {
            pjsua_on_stream_created_param prm;
            
            prm.stream = call_med->strm.a.stream;
            prm.stream_idx = strm_idx;
            prm.destroy_port = PJ_FALSE;
            prm.port = call_med->strm.a.media_port;
            (*pjsua_var.ua_cfg.cb.on_stream_created2)(call->index, &prm);
            
            call_med->strm.a.destroy_port = prm.destroy_port;
            call_med->strm.a.media_port = prm.port;

        } else if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_stream_created)
        {
            (*pjsua_var.ua_cfg.cb.on_stream_created)(call->index,
                                                  call_med->strm.a.stream,
                                                  strm_idx,
                                                  &call_med->strm.a.media_port);
        }

        if ((call->audio_idx == -1) || ((unsigned)call->audio_idx == strm_idx))
		{
			if (call_med->strm.a.conf_slot == PJSUA_INVALID_ID)
			{
            /*
             * Add the stream to conference bridge.
             */
                char tmp[PJSIP_MAX_URL_SIZE];
                pj_str_t port_name;

                port_name.ptr = tmp;
                port_name.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
                                                 call->inv->dlg->remote.info->uri,
                                                 tmp, sizeof(tmp));
                if (port_name.slen < 1) {
                    port_name = pj_str("call");
                }          
                if (compare_port_signature(call->conf_slot, PJMEDIA_SIG_PORT_NULL))
                {
                    status = pjmedia_conf_replace_port(pjsua_var.mconf,
                        call->inv->pool,
                        call_med->strm.a.media_port,
                        (unsigned)call->conf_slot);
                    if (status != PJ_SUCCESS)
                        goto on_return;
                    call_med->strm.a.conf_slot= call->conf_slot;
                    PJ_LOG(4, (THIS_FILE, "pjsua_aud_channel_update::pjmedia_conf_replace_port [call->conf_slot:%d]", call->conf_slot));
                }
                else
                {
                    status = pjmedia_conf_add_port(pjsua_var.mconf,
                        call->inv->pool,
                        call_med->strm.a.media_port,
                        &port_name,
                        (unsigned*)&call_med->strm.a.conf_slot);
                    if (status != PJ_SUCCESS)
                        goto on_return;
                    PJ_LOG(4, (THIS_FILE, "pjsua_aud_channel_update::pjmedia_conf_add_port [call_med->strm.a.conf_slot:%d]", call_med->strm.a.conf_slot));
                }
                    if (call->conf_slot == PJSUA_INVALID_ID)
                        call->conf_slot = call_med->strm.a.conf_slot;
                
			}
			else
			{
				/*
				 * Replace the old/null port in the conference bridge.
				 */
				status = pjmedia_conf_replace_port(pjsua_var.mconf,
					call->inv->pool,
					call_med->strm.a.media_port,
					(unsigned)call_med->strm.a.conf_slot);
				if (status != PJ_SUCCESS)
					goto on_return;
                PJ_LOG(4, (THIS_FILE, "pjsua_aud_channel_update::pjmedia_conf_replace_port [call_med->strm.a.conf_slot:%d]", call_med->strm.a.conf_slot));
            }
			if (call->audio_idx == -1)
				call->audio_idx = (int)strm_idx;
			call->conf_idx = call->audio_idx;
		}
		else
		{
			PJ_LOG(2, (THIS_FILE, "Ignoring audio channel update for index %d for call %d because the selected index is %d!",
				call_med->idx, call_med->call->index, call->audio_idx));
				goto on_return;
		}


        /* Subscribe to stream events */
        pjmedia_event_subscribe(NULL, &call_media_on_event, call_med,
                                call_med->strm.a.stream);
    }

on_return:
    pj_log_pop_indent();
    if (status != PJ_SUCCESS)
        PJ_LOG(2, (THIS_FILE, "pjsua_aud_channel_update failed"));
    return status;
}

PJ_DEF(void) pjsua_snd_dev_param_default(pjsua_snd_dev_param *prm)
{
    pj_bzero(prm, sizeof(*prm));
    prm->capture_dev = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
    prm->playback_dev = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;
}

PJ_DEF(void) pjsua_conf_connect_param_default(pjsua_conf_connect_param *prm)
{
    pj_bzero(prm, sizeof(*prm));
    prm->level = 1.0;
}

/*
 * Get maxinum number of conference ports.
 */
PJ_DEF(unsigned) pjsua_conf_get_max_ports(void)
{
    return pjsua_var.media_cfg.max_media_ports;
}


/*
 * Get current number of active ports in the bridge.
 */
PJ_DEF(unsigned) pjsua_conf_get_active_ports(void)
{
	unsigned count = pjsua_var.media_cfg.max_media_ports;
    pj_status_t status;

	status = pjmedia_conf_enum_ports(pjsua_var.mconf, NULL, &count);
    if (status != PJ_SUCCESS)
        count = 0;

    return count;
}


/*
 * Enumerate all conference ports.
 */
PJ_DEF(pj_status_t) pjsua_enum_conf_ports(pjsua_conf_port_id id[],
                                          unsigned *count)
{
    return pjmedia_conf_enum_ports(pjsua_var.mconf, (unsigned*)id, count);
}


/*
 * Get information about the specified conference port
 */
PJ_DEF(pj_status_t) pjsua_conf_get_port_info( pjsua_conf_port_id id,
                                              pjsua_conf_port_info *info)
{
    pjmedia_conf_port_info cinfo;
    unsigned i;
    pj_status_t status;

    PJ_ASSERT_RETURN(id >= 0, PJ_EINVAL);

    status = pjmedia_conf_get_port_info( pjsua_var.mconf, id, &cinfo);
    if (status != PJ_SUCCESS)
        return status;

    pj_bzero(info, sizeof(*info));
    info->slot_id = id;
    info->name = cinfo.name;
    pjmedia_format_copy(&info->format, &cinfo.format);
    info->clock_rate = cinfo.clock_rate;
    info->channel_count = cinfo.channel_count;
    info->samples_per_frame = cinfo.samples_per_frame;
    info->bits_per_sample = cinfo.bits_per_sample;
    info->tx_level_adj = ((float)cinfo.tx_adj_level) / 128 + 1;
    info->rx_level_adj = ((float)cinfo.rx_adj_level) / 128 + 1;

    /* Build array of listeners */
    info->listener_cnt = cinfo.listener_cnt;
    for (i=0; i<cinfo.listener_cnt && i < PJ_ARRAY_SIZE(info->listeners); ++i) {
        info->listeners[i] = cinfo.listener_slots[i];
    }

    return PJ_SUCCESS;
}


/*
 * Add arbitrary media port to PJSUA's conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_conf_add_port( pj_pool_t *pool,
                                         pjmedia_port *port,
                                         pjsua_conf_port_id *p_id)
{
    pj_status_t status;

    status = pjmedia_conf_add_port(pjsua_var.mconf, pool,
                                   port, NULL, (unsigned*)p_id);
    if (status != PJ_SUCCESS) {
        if (p_id)
            *p_id = PJSUA_INVALID_ID;
    }

    return status;
}


/*
 * Remove arbitrary slot from the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_conf_remove_port(pjsua_conf_port_id id)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(id >= 0, PJ_EINVAL);

    status = pjmedia_conf_remove_port(pjsua_var.mconf, (unsigned)id);
    pjsua_check_snd_dev_idle();

    return status;
}


/*
 * Establish unidirectional media flow from souce to sink.
 */
PJ_DEF(pj_status_t) pjsua_conf_connect( pjsua_conf_port_id source,
                                        pjsua_conf_port_id sink)
{
    pjsua_conf_connect_param prm;

    pjsua_conf_connect_param_default(&prm);
    return pjsua_conf_connect2(source, sink, &prm);
}
                                        
/*
 * Establish unidirectional media flow from souce to sink, with signal
 * level adjustment.
 */
PJ_DEF(pj_status_t) pjsua_conf_connect2( pjsua_conf_port_id source,
                                         pjsua_conf_port_id sink,
                                         const pjsua_conf_connect_param *prm)
{
    pj_status_t status = PJ_SUCCESS;

    PJ_LOG(4,(THIS_FILE, "%s connect: %d --> %d",
              (pjsua_var.is_mswitch ? "Switch" : "Conf"),
              source, sink));

    PJ_ASSERT_RETURN(source >= 0 && sink >= 0, PJ_EINVAL);

    pj_log_push_indent();

    PJSUA_LOCK();

    /* If sound device idle timer is active, cancel it first. */
    if (pjsua_var.snd_idle_timer.id) {
        pjsip_endpt_cancel_timer(pjsua_var.endpt, &pjsua_var.snd_idle_timer);
        pjsua_var.snd_idle_timer.id = PJ_FALSE;
    }


    /* For audio switchboard (i.e. APS-Direct):
     * Check if sound device need to be reopened, i.e: its attributes
     * (format, clock rate, channel count) must match to peer's.
     * Note that sound device can be reopened only if it doesn't have
     * any connection.
     */
    if (pjsua_var.is_mswitch) {
        pjmedia_conf_port_info port0_info;
        pjmedia_conf_port_info peer_info;
        unsigned peer_id;
        pj_bool_t need_reopen = PJ_FALSE;

        peer_id = (source!=0)? source : sink;
        status = pjmedia_conf_get_port_info(pjsua_var.mconf, peer_id,
                                            &peer_info);
        pj_assert(status == PJ_SUCCESS);

        status = pjmedia_conf_get_port_info(pjsua_var.mconf, 0, &port0_info);
        pj_assert(status == PJ_SUCCESS);

        /* Check if sound device is instantiated. */
        need_reopen = (pjsua_var.snd_port==NULL && pjsua_var.null_snd==NULL &&
                      !pjsua_var.no_snd);

        /* Check if sound device need to reopen because it needs to modify
         * settings to match its peer. Sound device must be idle in this case
         * though.
         */
        if (!need_reopen &&
            port0_info.listener_cnt==0 && port0_info.transmitter_cnt==0)
        {
            need_reopen = (peer_info.format.id != port0_info.format.id ||
                           peer_info.format.det.aud.avg_bps !=
                                   port0_info.format.det.aud.avg_bps ||
                           peer_info.clock_rate != port0_info.clock_rate ||
                           peer_info.channel_count!=port0_info.channel_count);
        }

        if (need_reopen) {
            if (pjsua_var.cap_dev != PJSUA_SND_NULL_DEV) {
                pjmedia_snd_port_param param;

                pjmedia_snd_port_param_default(&param);
                param.ec_options = pjsua_var.media_cfg.ec_options;

                /* Create parameter based on peer info */
                status = create_aud_param(&param.base, pjsua_var.cap_dev,
                                          pjsua_var.play_dev,
                                          peer_info.clock_rate,
                                          peer_info.channel_count,
                                          peer_info.samples_per_frame,
                                          peer_info.bits_per_sample,
                                          PJ_FALSE);
                if (status != PJ_SUCCESS) {
                    pjsua_perror(THIS_FILE, "Error opening sound device",
                                 status);
                    goto on_return;
                }

                /* And peer format */
                if (peer_info.format.id != PJMEDIA_FORMAT_PCM) {
                    param.base.flags |= PJMEDIA_AUD_DEV_CAP_EXT_FORMAT;
                    param.base.ext_fmt = peer_info.format;
                }

                param.options = 0;
                status = open_snd_dev(&param);
                if (status != PJ_SUCCESS) {
                    pjsua_perror(THIS_FILE, "Error opening sound device",
                                 status);
                    goto on_return;
                }
            } else {
                /* Null-audio */
                status = pjsua_set_snd_dev(pjsua_var.cap_dev,
                                           pjsua_var.play_dev);
                if (status != PJ_SUCCESS) {
                    pjsua_perror(THIS_FILE, "Error opening sound device",
                                 status);
                    goto on_return;
                }
            }
        } else if (pjsua_var.no_snd) {
            if (!pjsua_var.snd_is_on) {
                pjsua_var.snd_is_on = PJ_TRUE;
                /* Notify app */
                if (pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
                    (*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(1);
                }
            }
        }

    } else {
        /* The bridge version */

        /* Create sound port if none is instantiated */
        if (pjsua_var.snd_port==NULL && pjsua_var.null_snd==NULL &&
            !pjsua_var.no_snd)
        {
            status = pjsua_set_snd_dev(pjsua_var.cap_dev, pjsua_var.play_dev);
            if (status != PJ_SUCCESS) {
                pjsua_perror(THIS_FILE, "Error opening sound device", status);
                goto on_return;
            }
        } else if (pjsua_var.no_snd && !pjsua_var.snd_is_on) {
            pjsua_var.snd_is_on = PJ_TRUE;
            /* Notify app */
            if (pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
                (*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(1);
            }
        }
    }

on_return:
    PJSUA_UNLOCK();

    if (status == PJ_SUCCESS) {
        pjsua_conf_connect_param cc_param;
        
        if (!prm)
            pjsua_conf_connect_param_default(&cc_param);
        else
            pj_memcpy(&cc_param, prm, sizeof(cc_param));
        status = pjmedia_conf_connect_port(pjsua_var.mconf, source, sink, 
                                           (int)((cc_param.level-1) * 128));
    }

    pj_log_pop_indent();
    return status;
}


/*
 * Disconnect media flow from the source to destination port.
 */
PJ_DEF(pj_status_t) pjsua_conf_disconnect( pjsua_conf_port_id source,
                                           pjsua_conf_port_id sink)
{
    pj_status_t status;

    PJ_LOG(4,(THIS_FILE, "%s disconnect: %d -x- %d",
              (pjsua_var.is_mswitch ? "Switch" : "Conf"),
              source, sink));

    PJ_ASSERT_RETURN(source >= 0 && sink >= 0, PJ_EINVAL);
    pj_log_push_indent();

    status = pjmedia_conf_disconnect_port(pjsua_var.mconf, source, sink);
    pjsua_check_snd_dev_idle();

    pj_log_pop_indent();
    return status;
}


/*
 * Adjust the signal level to be transmitted from the bridge to the
 * specified port by making it louder or quieter.
 */
PJ_DEF(pj_status_t) pjsua_conf_adjust_tx_level(pjsua_conf_port_id slot,
                                               float level)
{
    PJ_ASSERT_RETURN(slot >= 0, PJ_EINVAL);

    return pjmedia_conf_adjust_tx_level(pjsua_var.mconf, slot,
                                        (int)((level-1) * 128));
}

/*
 * Adjust the signal level to be received from the specified port (to
 * the bridge) by making it louder or quieter.
 */
PJ_DEF(pj_status_t) pjsua_conf_adjust_rx_level(pjsua_conf_port_id slot,
                                               float level)
{
    PJ_ASSERT_RETURN(slot >= 0, PJ_EINVAL);

    return pjmedia_conf_adjust_rx_level(pjsua_var.mconf, slot,
                                        (int)((level-1) * 128));
}


/*
 * Get last signal level transmitted to or received from the specified port.
 */
PJ_DEF(pj_status_t) pjsua_conf_get_signal_level(pjsua_conf_port_id slot,
                                                unsigned *tx_level,
                                                unsigned *rx_level)
{
    PJ_ASSERT_RETURN(slot >= 0, PJ_EINVAL);

    return pjmedia_conf_get_signal_level(pjsua_var.mconf, slot,
                                         tx_level, rx_level);
}

/*****************************************************************************
 * File player.
 */

static char* get_basename(const char *path, unsigned len)
{
    char *p = ((char*)path) + len;

    if (len==0)
        return p;

    for (--p; p!=path && *p!='/' && *p!='\\'; ) --p;

    return (p==path) ? p : p+1;
}


/*
 * Create a file player, and automatically connect this player to
 * the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_player_create( const pj_str_t *filename,
                                         unsigned options,
                                         pjsua_player_id *p_id)
{
    unsigned slot, file_id;
    char path[PJ_MAXPATH];
    pj_pool_t *pool = NULL;
    pjmedia_port *port;
    pj_status_t status = PJ_SUCCESS;

    if (pjsua_var.player_cnt >= PJ_ARRAY_SIZE(pjsua_var.player))
        return PJ_ETOOMANY;

    if (filename->slen >= PJ_MAXPATH)
        return PJ_ENAMETOOLONG;

    PJ_LOG(4,(THIS_FILE, "Creating file player: %.*s..",
              (int)filename->slen, filename->ptr));
    pj_log_push_indent();

    PJSUA_LOCK();

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var.player); ++file_id) {
        if (pjsua_var.player[file_id].port == NULL)
            break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var.player)) {
        /* This is unexpected */
        pj_assert(0);
        status = PJ_EBUG;
        goto on_error;
    }

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';

    pool = pjsua_pool_create(get_basename(path, (unsigned)filename->slen), 1000, 
                             1000);
    if (!pool) {
        status = PJ_ENOMEM;
        goto on_error;
    }

    status = pjmedia_wav_player_port_create(
                                    pool, path,
                                    pjsua_var.mconf_cfg.samples_per_frame *
                                    1000 / pjsua_var.media_cfg.channel_count /
                                    pjsua_var.media_cfg.clock_rate,
                                    options, 0, &port);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Unable to open file for playback", status);
        goto on_error;
    }

    status = pjmedia_conf_add_port(pjsua_var.mconf, pool,
                                   port, filename, &slot);
    if (status != PJ_SUCCESS) {
        pjmedia_port_destroy(port);
        pjsua_perror(THIS_FILE, "Unable to add file to conference bridge",
                     status);
        goto on_error;
    }

    pjsua_var.player[file_id].type = 0;
    pjsua_var.player[file_id].pool = pool;
    pjsua_var.player[file_id].port = port;
    pjsua_var.player[file_id].slot = slot;

    if (p_id) *p_id = file_id;

    ++pjsua_var.player_cnt;

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Player created, id=%d, slot=%d", file_id, slot));

    pj_log_pop_indent();
    return PJ_SUCCESS;

on_error:
    PJSUA_UNLOCK();
    if (pool) pj_pool_release(pool);
    pj_log_pop_indent();
    return status;
}


/*
 * Create a file playlist media port, and automatically add the port
 * to the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_playlist_create( const pj_str_t file_names[],
                                           unsigned file_count,
                                           const pj_str_t *label,
                                           unsigned options,
                                           pjsua_player_id *p_id)
{
    unsigned slot, file_id, ptime;
    pj_pool_t *pool = NULL;
    pjmedia_port *port;
    pj_status_t status = PJ_SUCCESS;

    if (pjsua_var.player_cnt >= PJ_ARRAY_SIZE(pjsua_var.player))
        return PJ_ETOOMANY;

    PJ_LOG(4,(THIS_FILE, "Creating playlist with %d file(s)..", file_count));
    pj_log_push_indent();

    PJSUA_LOCK();

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var.player); ++file_id) {
        if (pjsua_var.player[file_id].port == NULL)
            break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var.player)) {
        /* This is unexpected */
        pj_assert(0);
        status = PJ_EBUG;
        goto on_error;
    }


    ptime = pjsua_var.mconf_cfg.samples_per_frame * 1000 /
            pjsua_var.media_cfg.clock_rate;

    pool = pjsua_pool_create("playlist", 1000, 1000);
    if (!pool) {
        status = PJ_ENOMEM;
        goto on_error;
    }

    status = pjmedia_wav_playlist_create(pool, label,
                                         file_names, file_count,
                                         ptime, options, 0, &port);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Unable to create playlist", status);
        goto on_error;
    }

    status = pjmedia_conf_add_port(pjsua_var.mconf, pool,
                                   port, &port->info.name, &slot);
    if (status != PJ_SUCCESS) {
        pjmedia_port_destroy(port);
        pjsua_perror(THIS_FILE, "Unable to add port", status);
        goto on_error;
    }

    pjsua_var.player[file_id].type = 1;
    pjsua_var.player[file_id].pool = pool;
    pjsua_var.player[file_id].port = port;
    pjsua_var.player[file_id].slot = slot;

    if (p_id) *p_id = file_id;

    ++pjsua_var.player_cnt;

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Playlist created, id=%d, slot=%d", file_id, slot));

    pj_log_pop_indent();

    return PJ_SUCCESS;

on_error:
    PJSUA_UNLOCK();
    if (pool) pj_pool_release(pool);
    pj_log_pop_indent();

    return status;
}


/*
 * Get conference port ID associated with player.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_player_get_conf_port(pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player),PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);

    return pjsua_var.player[id].slot;
}

/*
 * Get the media port for the player.
 */
PJ_DEF(pj_status_t) pjsua_player_get_port( pjsua_player_id id,
                                           pjmedia_port **p_port)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player),PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(p_port != NULL, PJ_EINVAL);

    *p_port = pjsua_var.player[id].port;

    return PJ_SUCCESS;
}

/*
 * Get player info.
 */
PJ_DEF(pj_status_t) pjsua_player_get_info(pjsua_player_id id,
                                          pjmedia_wav_player_info *info)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player),
                     -PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].type == 0, PJ_EINVAL);

    return pjmedia_wav_player_get_info(pjsua_var.player[id].port, info);
}

/*
 * Get playback position.
 */
PJ_DEF(pj_ssize_t) pjsua_player_get_pos( pjsua_player_id id )
{
    pj_ssize_t pos_bytes;
    pjmedia_wav_player_info info;
    pj_status_t status;

    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player),
                     -PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, -PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].type == 0, -PJ_EINVAL);

    pos_bytes = pjmedia_wav_player_port_get_pos(pjsua_var.player[id].port);
    if (pos_bytes < 0)
        return pos_bytes;

    status = pjmedia_wav_player_get_info(pjsua_var.player[id].port, &info);
    if (status != PJ_SUCCESS)
        return -status;

    return pos_bytes / (info.payload_bits_per_sample / 8);
}

/*
 * Set playback position.
 */
PJ_DEF(pj_status_t) pjsua_player_set_pos( pjsua_player_id id,
                                          pj_uint32_t samples)
{
    pjmedia_wav_player_info info;
    pj_uint32_t pos_bytes;
    pj_status_t status;

    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player),PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].type == 0, PJ_EINVAL);

    status = pjmedia_wav_player_get_info(pjsua_var.player[id].port, &info);
    if (status != PJ_SUCCESS)
        return status;

    pos_bytes = samples * (info.payload_bits_per_sample / 8);
    return pjmedia_wav_player_port_set_pos(pjsua_var.player[id].port,
                                           pos_bytes);
}


/*
 * Close the file, remove the player from the bridge, and free
 * resources associated with the file player.
 */
PJ_DEF(pj_status_t) pjsua_player_destroy(pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player),PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Destroying player %d..", id));
    pj_log_push_indent();

    PJSUA_LOCK();

    if (pjsua_var.player[id].port) {
        pjsua_conf_remove_port(pjsua_var.player[id].slot);
        pjmedia_port_destroy(pjsua_var.player[id].port);
        pjsua_var.player[id].port = NULL;
        pjsua_var.player[id].slot = 0xFFFF;
        pj_pool_release(pjsua_var.player[id].pool);
        pjsua_var.player[id].pool = NULL;
        pjsua_var.player_cnt--;
    }

    PJSUA_UNLOCK();
    pj_log_pop_indent();

    return PJ_SUCCESS;
}


/*****************************************************************************
 * File recorder.
 */

/*
 * Create a file recorder, and automatically connect this recorder to
 * the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_recorder_create( const pj_str_t *filename,
                                           unsigned enc_type,
                                           void *enc_param,
                                           pj_ssize_t max_size,
                                           unsigned options,
                                           pjsua_recorder_id *p_id)
{
    enum Format
    {
        FMT_UNKNOWN,
        FMT_WAV,
        FMT_MP3,
    };
    unsigned slot, file_id;
    char path[PJ_MAXPATH];
    pj_str_t ext;
    int file_format;
    pj_pool_t *pool = NULL;
    pjmedia_port *port;
    pj_status_t status = PJ_SUCCESS;

    /* Filename must present */
    PJ_ASSERT_RETURN(filename != NULL, PJ_EINVAL);

    /* Don't support max_size at present */
    PJ_ASSERT_RETURN(max_size == 0 || max_size == -1, PJ_EINVAL);

    /* Don't support encoding type at present */
    PJ_ASSERT_RETURN(enc_type == 0, PJ_EINVAL);

    if (filename->slen >= PJ_MAXPATH)
        return PJ_ENAMETOOLONG;
    if (filename->slen < 4)
        return PJ_EINVALIDOP;

    PJ_LOG(4,(THIS_FILE, "Creating recorder %.*s..",
              (int)filename->slen, filename->ptr));
    pj_log_push_indent();

    if (pjsua_var.rec_cnt >= PJ_ARRAY_SIZE(pjsua_var.recorder)) {
        pj_log_pop_indent();
        return PJ_ETOOMANY;
    }

    /* Determine the file format */
    ext.ptr = filename->ptr + filename->slen - 4;
    ext.slen = 4;

    if (pj_stricmp2(&ext, ".wav") == 0)
        file_format = FMT_WAV;
    else if (pj_stricmp2(&ext, ".mp3") == 0)
        file_format = FMT_MP3;
    else {
        PJ_LOG(1,(THIS_FILE, "pjsua_recorder_create() error: unable to "
                             "determine file format for %.*s",
                             (int)filename->slen, filename->ptr));
        pj_log_pop_indent();
        return PJ_ENOTSUP;
    }

    PJSUA_LOCK();

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var.recorder); ++file_id) {
        if (pjsua_var.recorder[file_id].port == NULL)
            break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var.recorder)) {
        /* This is unexpected */
        pj_assert(0);
        status = PJ_EBUG;
        goto on_return;
    }

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';

    pool = pjsua_pool_create(get_basename(path, (unsigned)filename->slen), 1000, 
                             1000);
    if (!pool) {
        status = PJ_ENOMEM;
        goto on_return;
    }

    if (file_format == FMT_WAV) {
        status = pjmedia_wav_writer_port_create(pool, path,
                                                pjsua_var.media_cfg.clock_rate,
                                                pjsua_var.mconf_cfg.channel_count,
                                                pjsua_var.mconf_cfg.samples_per_frame,
                                                pjsua_var.mconf_cfg.bits_per_sample,
                                                options, 0, &port);
    } else {
        PJ_UNUSED_ARG(enc_param);
        port = NULL;
        status = PJ_ENOTSUP;
    }

    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Unable to open file for recording", status);
        goto on_return;
    }

    status = pjmedia_conf_add_port(pjsua_var.mconf, pool,
                                   port, filename, &slot);
    if (status != PJ_SUCCESS) {
        pjmedia_port_destroy(port);
        goto on_return;
    }

    pjsua_var.recorder[file_id].port = port;
    pjsua_var.recorder[file_id].slot = slot;
    pjsua_var.recorder[file_id].pool = pool;

    if (p_id) *p_id = file_id;

    ++pjsua_var.rec_cnt;

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Recorder created, id=%d, slot=%d", file_id, slot));

    pj_log_pop_indent();
    return PJ_SUCCESS;

on_return:
    PJSUA_UNLOCK();
    if (pool) pj_pool_release(pool);
    pj_log_pop_indent();
    return status;
}


/*
 * Get conference port associated with recorder.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_recorder_get_conf_port(pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.recorder),
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.recorder[id].port != NULL, PJ_EINVAL);

    return pjsua_var.recorder[id].slot;
}

/*
 * Get the media port for the recorder.
 */
PJ_DEF(pj_status_t) pjsua_recorder_get_port( pjsua_recorder_id id,
                                             pjmedia_port **p_port)
{
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.recorder),
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.recorder[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(p_port != NULL, PJ_EINVAL);

    *p_port = pjsua_var.recorder[id].port;
    return PJ_SUCCESS;
}

/*
 * Destroy recorder (this will complete recording).
 */
PJ_DEF(pj_status_t) pjsua_recorder_destroy(pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.recorder),
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.recorder[id].port != NULL, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Destroying recorder %d..", id));
    pj_log_push_indent();

    PJSUA_LOCK();

    if (pjsua_var.recorder[id].port) {
        pjsua_conf_remove_port(pjsua_var.recorder[id].slot);
        pjmedia_port_destroy(pjsua_var.recorder[id].port);
        pjsua_var.recorder[id].port = NULL;
        pjsua_var.recorder[id].slot = 0xFFFF;
        pj_pool_release(pjsua_var.recorder[id].pool);
        pjsua_var.recorder[id].pool = NULL;
        pjsua_var.rec_cnt--;
    }

    PJSUA_UNLOCK();
    pj_log_pop_indent();

    return PJ_SUCCESS;
}

/*****************************************************************************
 * Media port
 */

PJ_DEF(pj_status_t) pjsua_mport_alloc(pjmedia_dir dir,
	pj_bool_t enable_vad, pj_size_t record_buffer_size,
	pj_size_t record_data_threshold, pj_size_t play_buffer_size,
	pj_size_t play_data_threshold, pjsua_mport_id *p_id)
{
	char name[PJ_MAX_OBJ_NAME];

	if ((dir != PJMEDIA_DIR_CAPTURE) && (dir != PJMEDIA_DIR_PLAYBACK) &&
		(dir != PJMEDIA_DIR_CAPTURE_PLAYBACK))
		return PJ_EINVAL;

	PJSUA_LOCK();

	if (pjsua_var.mport_cnt >= pjsua_var.media_cfg.max_media_ports)
	{
		PJSUA_UNLOCK();
		return PJ_ETOOMANY;
	}

	unsigned int id;
	for (id = 0; id<pjsua_var.media_cfg.max_media_ports; ++id)
	{
		register unsigned int tmp = id + pjsua_var.mport_id + 1;
		if (tmp >= pjsua_var.media_cfg.max_media_ports)
			tmp -= pjsua_var.media_cfg.max_media_ports;
		if (pjsua_var.mport[tmp].slot == PJSUA_INVALID_ID)
		{
			pjsua_var.mport_id = id = tmp;
			break;
		}
	}
	if (id == pjsua_var.media_cfg.max_media_ports)
	{
		/* This is unexpected */
		pj_assert(0);
		PJSUA_UNLOCK();
		return PJ_EBUG;
	}

	pj_log_push_indent();

	pj_status_t status = PJ_SUCCESS;
	register pjsua_mport_data *const p = &pjsua_var.mport[id];
	p->pool = pjsua_pool_create(p->base.info.name.ptr, 1000, 1000);
	if (p->pool == NULL)
	{
		status = PJ_ENOMEM;
		goto on_error;
	}

	name[sizeof(name) - 1] = '\0';

	p->listener_cnt = p->participant_cnt = 0U;
	p->listeners = p->participants = NULL;
	p->mix_buf = NULL;
	p->last_mix_adj = p->mix_adj = NORMAL_LEVEL;

	p->record_data.vad = NULL;
	p->record_data.buffer_size = 0U;
	p->record_data.buffer = NULL;
	p->record_data.event = NULL;
	p->record_data.signaled = PJ_FALSE;
	p->record_data.status = PJSUA_REC_IDLE;
	if (dir & PJMEDIA_DIR_CAPTURE)
	{
		if (enable_vad)
		{
			status = pjmedia_silence_det_create(p->pool, pjsua_var.media_cfg.clock_rate, pjsua_var.mconf_cfg.samples_per_frame, &p->record_data.vad);
			if (status != PJ_SUCCESS)
				goto on_error;
			status = pjmedia_silence_det_set_name(p->record_data.vad, p->base.info.name.ptr);
		}
		if (!record_buffer_size)
		{
			record_buffer_size = pjsua_var.media_cfg.mport_record_buffer_size;
			if (!record_buffer_size)
				record_buffer_size = 1250;
		}
		if (record_buffer_size < 40)
			record_buffer_size = 40;
		else if (record_buffer_size > 5000)
			record_buffer_size = 5000;
		record_buffer_size = ((record_buffer_size + (pjsua_var.media_cfg.audio_frame_ptime - 1)) / pjsua_var.media_cfg.audio_frame_ptime);
		p->record_data.buffer_size = record_buffer_size * pjsua_var.mconf_cfg.samples_per_frame;
		_snprintf(name, sizeof(name)-1, "mp_rec%d", id);
		status = pj_event_create(p->pool, name, PJ_TRUE, PJ_FALSE, &p->record_data.event);
		if (status != PJ_SUCCESS)
			goto on_error;
		if (!record_data_threshold)
			record_data_threshold = 250;
		record_data_threshold = ((record_data_threshold + (pjsua_var.media_cfg.audio_frame_ptime - 1)) / pjsua_var.media_cfg.audio_frame_ptime);
		if (record_data_threshold > record_buffer_size)
			record_data_threshold = record_buffer_size;
		p->record_data.threshold = record_data_threshold * pjsua_var.mconf_cfg.samples_per_frame;
		_snprintf(name, sizeof(name)-1, "mp_rgn%d", id);
		status = pj_event_create(p->pool, name, PJ_TRUE, PJ_FALSE, &p->recogntion_data.event);
		if (status != PJ_SUCCESS)
			goto on_error;
	}

	p->play_data.buffer_size = 0U;
	p->play_data.timestamp.u64 = 0;
	p->play_data.buffer = NULL;
	p->play_data.event = NULL;
	p->play_data.signaled = PJ_FALSE;
	p->play_data.status = PJSUA_REP_IDLE;
	if (dir & PJMEDIA_DIR_PLAYBACK)
	{
		if (!play_buffer_size)
		{
			play_buffer_size = pjsua_var.media_cfg.mport_replay_buffer_size;
			if (!play_buffer_size)
				play_buffer_size = 1250;
		}
		if (play_buffer_size < 40)
			play_buffer_size = 40;
		else if (play_buffer_size > 5000)
			play_buffer_size = 5000;
		play_buffer_size = ((play_buffer_size + (pjsua_var.media_cfg.audio_frame_ptime - 1)) / pjsua_var.media_cfg.audio_frame_ptime);
		p->play_data.buffer_size = play_buffer_size * pjsua_var.mconf_cfg.samples_per_frame;
		_snprintf(name, sizeof(name)-1, "mp_pla%d", id);
		status = pj_event_create(p->pool, name, PJ_TRUE, PJ_FALSE, &p->play_data.event);
		if (status != PJ_SUCCESS)
			goto on_error;
		if (!play_data_threshold)
			play_data_threshold = 250;
		play_data_threshold = ((play_data_threshold + (pjsua_var.media_cfg.audio_frame_ptime - 1)) / pjsua_var.media_cfg.audio_frame_ptime);
		if (play_data_threshold > play_buffer_size)
			play_data_threshold = play_buffer_size;
		p->play_data.threshold = play_data_threshold * pjsua_var.mconf_cfg.samples_per_frame;
	}

	status = pjsua_conf_add_port(p->pool, &p->base, &p->slot);
	if (status != PJ_SUCCESS)
	{
		pjsua_perror(THIS_FILE, "Unable to add media port to conference bridge", status);
		goto on_error;
	}

	p->base.info.dir = dir;

	++pjsua_var.mport_cnt;

	PJSUA_UNLOCK();

	PJ_LOG(4,(THIS_FILE, "Media port %d allocated: slot=%d", id, p->slot));

	if (p_id)
		*p_id = id;

	pj_log_pop_indent();
	return PJ_SUCCESS;

on_error:
	p->listeners = p->participants = NULL;
	p->mix_buf = NULL;
	if (p->recogntion_data.event != NULL)
	{
		pj_event_destroy(p->recogntion_data.event);
		p->recogntion_data.event = NULL;
	}
	if (p->record_data.event != NULL)
	{
		pj_event_destroy(p->record_data.event);
		p->record_data.event = NULL;
	}
	p->record_data.vad = NULL;
	p->record_data.buffer = NULL;
	if (p->play_data.event != NULL)
	{
		pj_event_destroy(p->play_data.event);
		p->play_data.event = NULL;
	}
	p->play_data.buffer = NULL;
	if (p->pool != NULL)
	{
		pj_pool_release(p->pool);
		p->pool = NULL;
	}
	p->base.info.dir = PJMEDIA_DIR_NONE;
	p->slot = PJSUA_INVALID_ID;
	PJSUA_UNLOCK();
	pj_log_pop_indent();
	return status;
}

PJ_DEF(pj_status_t) pjsua_mport_free(pjsua_mport_id id)
{
	if ((id < 0) || ((unsigned int)id >= pjsua_var.media_cfg.max_media_ports))
	{
		pj_assert(0);
		return PJ_EINVAL;
	}

	PJSUA_LOCK();

	pjsua_mport_data *p = &pjsua_var.mport[id];

	if (!p->pool)
	{
		PJSUA_UNLOCK();
		return PJ_SUCCESS;
	}

	pjsua_mport_conf_stop(id);
	while (p->listener_cnt > 0)
		pjsua_mport_conf_remove(p->listeners[p->listener_cnt - 1], id);

	pj_enter_critical_section();

	p->listeners = p->participants = NULL;
	p->mix_buf = NULL;

	if (p->recogntion_data.event_cnt > 0U)
	{
		if (p->recogntion_data.signaled)
		{
			p->recogntion_data.signaled = PJ_FALSE;
			pj_event_reset(p->recogntion_data.event);
		}
		p->recogntion_data.event_cnt = 0U;
	}
	if (p->record_data.buffer)
	{
		pjmedia_circ_buf_reset(p->record_data.buffer);
		p->record_data.buffer = NULL;
	}
	if (p->record_data.status != PJSUA_REC_IDLE)
	{
		if ((p->record_data.event != NULL) &&
			!p->record_data.signaled)
		{
			p->record_data.signaled = PJ_TRUE;
			pj_event_set(p->record_data.event);
		}
		p->record_data.status = PJSUA_REC_IDLE;
	}
	p->record_data.vad = NULL;

	if (p->play_data.buffer)
	{
		pjmedia_circ_buf_reset(p->play_data.buffer);
		p->play_data.buffer = NULL;
	}
	if (p->play_data.status != PJSUA_REP_IDLE)
	{
		if ((p->play_data.status != PJSUA_REP_CONFERENCING) &&
			(p->play_data.event != NULL) &&
			!p->play_data.signaled)
		{
			p->play_data.signaled = PJ_TRUE;
			pj_event_set(p->play_data.event);
		}
		p->play_data.status = PJSUA_REP_IDLE;
	}

	pj_leave_critical_section();

	if (p->slot != PJSUA_INVALID_ID)
	{
		pjsua_conf_remove_port(p->slot);
		p->slot = PJSUA_INVALID_ID;
	}

	if (p->recogntion_data.event != NULL)
	{
		pj_event_destroy(p->recogntion_data.event);
		p->recogntion_data.event = NULL;
		p->recogntion_data.signaled = PJ_FALSE;
	}

	if (p->record_data.event != NULL)
	{
		pj_event_destroy(p->record_data.event);
		p->record_data.event = NULL;
		p->record_data.signaled = PJ_FALSE;
	}

	if (p->play_data.event != NULL)
	{
		pj_event_destroy(p->play_data.event);
		p->play_data.event = NULL;
		p->play_data.signaled = PJ_FALSE;
	}

	if (p->pool != NULL)
	{
		pj_pool_release(p->pool);
		p->pool = NULL;
	}

	p->base.info.dir = PJMEDIA_DIR_NONE;

	pj_assert(pjsua_var.mport_cnt > 0);
	--pjsua_var.mport_cnt;

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Media port %d freed", id));

	return PJ_SUCCESS;
}

PJ_DEF(pjsua_conf_port_id) pjsua_mport_get_conf_port(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,PJSUA_INVALID_ID);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJSUA_INVALID_ID);

	return pjsua_var.mport[id].slot;
}

PJ_DEF(pj_status_t) pjsua_mport_get_port( pjsua_mport_id id, pjmedia_port **p_port)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(p_port != NULL, PJ_EINVAL);

	*p_port = &pjsua_var.mport[id].base;

	return PJ_SUCCESS;
}

PJ_DEF(pjmedia_dir) pjsua_mport_get_dir(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJMEDIA_DIR_NONE);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJMEDIA_DIR_NONE);

	return pjsua_var.mport[id].base.info.dir;
}

PJ_DEF(pj_event_t *) pjsua_mport_get_play_event(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,NULL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, NULL);

	return pjsua_var.mport[id].play_data.event;
}

PJ_DEF(pj_event_t *) pjsua_mport_get_record_event(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,NULL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, NULL);

	return pjsua_var.mport[id].record_data.event;
}

PJ_DEF(pj_event_t *) pjsua_mport_get_recognition_event(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,NULL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, NULL);

	return pjsua_var.mport[id].recogntion_data.event;
}

static void add_replay_data(pjmedia_format_id fmt_id,
	pjmedia_circ_buf *buffer,
	const void *data,
	pj_size_t size,
	pj_size_t *count)
{
	pj_int16_t *reg1, *reg2;
	unsigned reg1cnt, reg2cnt;
	pj_size_t avl = buffer->capacity - buffer->len;
	if (avl)
	{
		if (fmt_id == PJMEDIA_FORMAT_PCM)
		{
			size = size >> 1;
			if (size > avl)
				size = avl;
			pjmedia_circ_buf_write(buffer, (pj_int16_t*)data, (unsigned int)size);
			size = size << 1;
		}
		else
		{
			if (size > avl)
				size = avl;
			pjmedia_circ_buf_get_write_regions(buffer, &reg1, &reg1cnt, &reg2, &reg2cnt);
			if (reg1cnt >= size)
			{
				switch (fmt_id)
				{
				case PJMEDIA_FORMAT_PCMA:
					pjmedia_alaw_decode(reg1, data, size);
					break;
				case PJMEDIA_FORMAT_PCMU:
					pjmedia_ulaw_decode(reg1, data, size);
					break;
				default:
					pj_assert(0);
					break;
				}
			}
			else
			{
				switch (fmt_id)
				{
				case PJMEDIA_FORMAT_PCMA:
					pjmedia_alaw_decode(reg1, data, reg1cnt);
					pjmedia_alaw_decode(reg2, (const pj_uint8_t*)data + reg1cnt, size - reg1cnt);
					break;
				case PJMEDIA_FORMAT_PCMU:
					pjmedia_ulaw_decode(reg1, data, reg1cnt);
					pjmedia_ulaw_decode(reg2, (const pj_uint8_t*)data + reg1cnt, size - reg1cnt);
					break;
				default:
					pj_assert(0);
					break;
				}
			}
			pjmedia_circ_buf_adv_write_ptr(buffer, (unsigned int)size);
		}
		*count = size;
	}
}

PJ_DEF(pj_status_t) pjsua_mport_play_start(
	pjsua_mport_id id,
	const pjmedia_format *fmt,
	const void *data,
	pj_size_t size,
	pj_size_t *count)
{
	pjsua_mport_replay_data *p;

	PJ_ASSERT_RETURN(count != NULL, PJ_EINVAL);

	*count = 0;

	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(fmt != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	p = &pjsua_var.mport[id].play_data;
	if (!(pjsua_var.mport[id].base.info.dir & PJMEDIA_DIR_PLAYBACK))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}
	if (p->status != PJSUA_REP_IDLE)
	{
		PJSUA_UNLOCK();
		return PJ_EBUSY;
	}
	if (((fmt->id != PJMEDIA_FORMAT_PCM) && (fmt->id != PJMEDIA_FORMAT_PCMA) && (fmt->id != PJMEDIA_FORMAT_PCMU)) ||
		(fmt->det.aud.clock_rate != pjsua_var.media_cfg.clock_rate) ||
		(fmt->det.aud.channel_count != pjsua_var.media_cfg.channel_count))
	{
		PJSUA_UNLOCK();
		return PJ_ENOTSUP;
	}
	if (!p->buffer)
	{
		pj_status_t status = pjmedia_circ_buf_create(pjsua_var.mport[id].pool, (unsigned int)p->buffer_size, &p->buffer);
		if (status != PJ_SUCCESS)
		{
			PJSUA_UNLOCK();
			return status;
		}
	}

	pjmedia_circ_buf_reset(p->buffer);
	if ((data != NULL) && size)
		add_replay_data(fmt->id, p->buffer, data, size, count);
	p->samples_played = 0;
	p->underrun = PJ_FALSE;
	if (p->buffer->len < p->threshold)
	{
		pj_event_set(p->event);
		p->signaled = PJ_TRUE;
	}
	else
	{
		pj_event_reset(p->event);
		p->signaled = PJ_FALSE;
	}
	p->fmt_id = fmt->id;
	p->status = PJSUA_REP_RUNNING;

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Started replay on media port %d", id));

	pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_NO_CHANGE, PJMEDIA_PORT_ENABLE_ALWAYS);

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_play_status(pjsua_mport_id id,
	pjs_mport_play_info *info)
{
	pjsua_mport_replay_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(info != NULL, PJ_EINVAL);

	pj_bzero(info, sizeof(*info));

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].play_data;

	if ((p->status != PJSUA_REP_RUNNING) && (p->status != PJSUA_REP_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	info->samples_played = p->samples_played;
	info->underrun = p->underrun;
	if (info->underrun)
		p->underrun = PJ_FALSE;
	if (p->status == PJSUA_REP_STOPPING)
	{
		if (!p->buffer->len)
		{
			if (p->signaled)
			{
				pj_event_reset(p->event);
				p->signaled = PJ_FALSE;
			}
			info->completed = PJ_TRUE;
			p->status = PJSUA_REP_IDLE;

			PJ_LOG(4, (THIS_FILE, "Replay completed on media port %d - %I64u samples were played",
				id, p->samples_played));
		}
	}
	else
	{
		info->free_buffer_size = p->buffer->capacity - p->buffer->len;
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_play_put_data(pjsua_mport_id id,
	const void *data,
	pj_size_t size,
	pj_size_t *count)
{
	pjsua_mport_replay_data *p;

	PJ_ASSERT_RETURN(count != NULL, PJ_EINVAL);

	*count = 0;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].play_data;

	if (p->status != PJSUA_REP_RUNNING)
	{
		pj_status_t status = (p->status == PJSUA_REP_STOPPING) ? PJ_EEOF : PJ_ENOTFOUND;
		pj_leave_critical_section();
		return status;
	}

	if ((data != NULL) && size)
	{
		add_replay_data(p->fmt_id, p->buffer, data, size, count);
		if (p->buffer->len >= p->threshold)
		{
			if (p->signaled)
			{
				pj_event_reset(p->event);
				p->signaled = PJ_FALSE;
			}
		}
		else
		{
			if (!p->signaled)
			{
				pj_event_set(p->event);
				p->signaled = PJ_TRUE;
			}
		}
	}
	else
	{
		if (p->buffer->len)
		{
			if (p->signaled)
			{
				pj_event_reset(p->event);
				p->signaled = PJ_FALSE;
			}
		}
		else
		{
			if (!p->signaled)
			{
				pj_event_set(p->event);
				p->signaled = PJ_TRUE;
			}
		}
		p->status = PJSUA_REP_STOPPING;
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_play_stop(pjsua_mport_id id, pj_bool_t discard)
{
	pjsua_mport_replay_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].play_data;

	if ((p->status != PJSUA_REP_RUNNING) && (p->status != PJSUA_REP_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	if (discard)
		p->buffer->len = 0;
	if (p->buffer->len)
	{
		if (p->signaled)
		{
			pj_event_reset(p->event);
			p->signaled = PJ_FALSE;
		}
	}
	else
	{
		if (!p->signaled)
		{
			pj_event_set(p->event);
			p->signaled = PJ_TRUE;
		}
	}
	if (p->status == PJSUA_REP_RUNNING)
		p->status = PJSUA_REP_STOPPING;

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_record_start(pjsua_mport_id id,
	const pjmedia_format *fmt,
	pj_bool_t rec_output,
	pj_size_t max_duration,
	pj_size_t max_samples,
	pj_size_t max_silence,
	pj_size_t eliminate_silence)
{
	pjsua_mport_record_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(fmt != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	p = &pjsua_var.mport[id].record_data;
	if (!(pjsua_var.mport[id].base.info.dir & PJMEDIA_DIR_CAPTURE))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}
	if (rec_output && !(pjsua_var.mport[id].base.info.dir & PJMEDIA_DIR_PLAYBACK))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}
	if (p->status != PJSUA_REC_IDLE)
	{
		PJSUA_UNLOCK();
		return PJ_EBUSY;
	}
	if (((fmt->id != PJMEDIA_FORMAT_PCM) && (fmt->id != PJMEDIA_FORMAT_PCMA) && (fmt->id != PJMEDIA_FORMAT_PCMU)) ||
		(fmt->det.aud.clock_rate != pjsua_var.media_cfg.clock_rate) ||
		(fmt->det.aud.channel_count != pjsua_var.media_cfg.channel_count))
	{
		PJSUA_UNLOCK();
		return PJ_ENOTSUP;
	}
	if (!p->buffer)
	{
		pj_status_t status = pjmedia_circ_buf_create(pjsua_var.mport[id].pool, (unsigned int)p->buffer_size, &p->buffer);
		if (status != PJ_SUCCESS)
		{
			PJSUA_UNLOCK();
			return status;
		}
	}

	// Convert all the time based arguments to the corresponding sample counts
	// and ensure that they are truncated to complete blocks if necessary
	if (max_duration)
	{
		max_duration = max_duration * fmt->det.aud.clock_rate / 1000;
		max_duration -= (max_duration % fmt->det.aud.channel_count);
	}
	if (max_silence)
	{
		max_silence = max_silence * fmt->det.aud.clock_rate / 1000;
		max_silence -= (max_silence % fmt->det.aud.channel_count);
	}
	if (eliminate_silence)
	{
		eliminate_silence = eliminate_silence * fmt->det.aud.clock_rate / 1000;
		eliminate_silence -= (eliminate_silence % fmt->det.aud.channel_count);
	}

	if (p->signaled)
	{
		pj_event_reset(p->event);
		p->signaled = PJ_FALSE;
	}
	pjmedia_circ_buf_reset(p->buffer);
	p->samples_seen = 0;
	p->samples_recorded = 0;
	p->vad_timestamp.u64 = 0;
	p->is_silence = PJ_FALSE;
	p->overrun = PJ_FALSE;
	p->er = PJSUA_REC_ER_NONE;
	p->fmt_id = fmt->id;
	p->rec_output = rec_output;
	p->max_samples = max_samples;
	p->max_duration = max_duration;
	p->max_silence = max_silence;
	p->eliminate_silence = eliminate_silence;
	p->status = PJSUA_REC_RUNNING;

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Started recording on media port %d; event=%p", id, p->event));

	if (rec_output)
		pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_NO_CHANGE, PJMEDIA_PORT_ENABLE_ALWAYS);
	else
		pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_ENABLE_ALWAYS, PJMEDIA_PORT_NO_CHANGE);

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_record_status(pjsua_mport_id id,
	pjs_mport_record_info *info)
{
	pjsua_mport_record_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(info != NULL, PJ_EINVAL);

	pj_bzero(info, sizeof(*info));

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].record_data;

	if ((p->status != PJSUA_REP_RUNNING) && (p->status != PJSUA_REP_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	info->samples_recorded = p->samples_recorded;
	info->overrun = p->overrun;
	if (info->overrun)
		p->overrun = PJ_FALSE;
	info->samples_available = p->buffer->len;
	info->end_reason = p->er;
	if ((p->status == PJSUA_REC_STOPPING) && !info->samples_available)
	{
		if (p->signaled)
		{
			pj_event_reset(p->event);
			p->signaled = PJ_FALSE;
		}
		info->completed = PJ_TRUE;
		p->status = PJSUA_REC_IDLE;

		PJ_LOG(4, (THIS_FILE, "Recording completed on media port %d - %I64u samples were recorded",
			id, p->samples_recorded));
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

static void get_record_data(pjmedia_format_id fmt_id,
	pjmedia_circ_buf *buffer,
	void *data,
	pj_size_t size,	/* Size of the data buffer in bytes - not samples! */
	pj_size_t *count)
{
	pj_int16_t *reg1, *reg2;
	unsigned reg1cnt, reg2cnt;
	if (buffer->len)
	{
		if (fmt_id == PJMEDIA_FORMAT_PCM)
		{
			if (size == 1U)
			{
				pj_int16_t buf;
				pjmedia_circ_buf_read(buffer, &buf, 1U);
				memcpy(data, &buf, 1U);
			}
			else
			{
				unsigned int samples = (unsigned int)(size >> 1);
				if (samples > buffer->len)
				{
					samples = buffer->len;
					size = samples << 1;
				}
				pjmedia_circ_buf_read(buffer, (pj_int16_t*)data, samples);
			}
		}
		else
		{
			if (size > buffer->len)
				size = buffer->len;
			pjmedia_circ_buf_get_read_regions(buffer, &reg1, &reg1cnt, &reg2, &reg2cnt);
			if (reg1cnt >= size)
			{
				switch (fmt_id)
				{
				case PJMEDIA_FORMAT_PCMA:
					pjmedia_alaw_encode((pj_uint8_t*)data, reg1, size);
					break;
				case PJMEDIA_FORMAT_PCMU:
					pjmedia_ulaw_encode((pj_uint8_t*)data, reg1, size);
					break;
				default:
					pj_assert(0);
					break;
				}
			}
			else
			{
				switch (fmt_id)
				{
				case PJMEDIA_FORMAT_PCMA:
					pjmedia_alaw_encode((pj_uint8_t*)data, reg1, reg1cnt);
					pjmedia_alaw_encode((pj_uint8_t*)data + reg1cnt, reg2, size - reg1cnt);
					break;
				case PJMEDIA_FORMAT_PCMU:
					pjmedia_ulaw_encode((pj_uint8_t*)data, reg1, reg1cnt);
					pjmedia_ulaw_encode((pj_uint8_t*)data + reg1cnt, reg2, size - reg1cnt);
					break;
				default:
					pj_assert(0);
					break;
				}
			}
			pjmedia_circ_buf_adv_read_ptr(buffer, (unsigned int)size);
		}
		*count = size;
	}
}

PJ_DEF(pj_status_t) pjsua_mport_record_get_data(pjsua_mport_id id,
	void *buffer,
	pj_size_t size,
	pj_size_t *count)
{
	pjsua_mport_record_data *p;

	PJ_ASSERT_RETURN(count != NULL, PJ_EINVAL);

	*count = 0;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(buffer && size, PJ_EINVAL);

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].record_data;

	if ((p->status != PJSUA_REC_RUNNING) && (p->status != PJSUA_REC_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	get_record_data(p->fmt_id, p->buffer, buffer, size, count);
	if ((p->status == PJSUA_REC_RUNNING) &&
		((p->buffer->capacity - p->buffer->len) >= p->threshold))
	{
		if (p->signaled)
		{
			pj_event_reset(p->event);
			p->signaled = PJ_FALSE;
		}
	}
	else
	{
		if (!p->signaled)
		{
			pj_event_set(p->event);
			p->signaled = PJ_TRUE;
		}
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_record_stop(pjsua_mport_id id, pj_bool_t discard)
{
	pjsua_mport_record_data *p;
	int rec_output = -1;
	pj_event_t* signaled_event = NULL;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);

	PJ_LOG(4, (THIS_FILE, "Stopping recording on media port %d%s...", id, discard ? " and discarding buffered data" : ""));

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].record_data;

	if ((p->status != PJSUA_REC_RUNNING) && (p->status != PJSUA_REC_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	if (discard)
		p->buffer->len = 0;
	if (p->status == PJSUA_REC_RUNNING)
	{
		p->er = PJSUA_REC_ER_STOP;
		p->status = PJSUA_REC_STOPPING;
		rec_output = p->rec_output;
	}
	if (!p->signaled)
	{
		signaled_event = p->event;
		pj_event_set(p->event);
		p->signaled = PJ_TRUE;
	}

	pj_leave_critical_section();

	if (rec_output >= 0)
	{
		if (rec_output)
			pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_NO_CHANGE, PJMEDIA_PORT_ENABLE);
		else
			pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_ENABLE, PJMEDIA_PORT_NO_CHANGE);
	}

	PJ_LOG(4, (THIS_FILE, "rec_output=%d, event=%p", rec_output, signaled_event));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_conf_start(pjsua_mport_id id)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	if (!(p->base.info.dir & PJMEDIA_DIR_PLAYBACK))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}
	if (p->play_data.status != PJSUA_REP_IDLE)
	{
		PJSUA_UNLOCK();
		return PJ_EBUSY;
	}
	if (!p->participants)
	{
		p->participants = (pjsua_mport_id*)pj_pool_zalloc(p->pool, sizeof(pjsua_mport_id) * PJSUA_MAX_CONF_PORTS);
		if (!p->participants)
		{
			PJSUA_UNLOCK();
			return PJ_ENOMEM;
		}
		for (register int i = 0; i < PJSUA_MAX_CONF_PORTS; ++i)
			p->participants[i] = PJSUA_INVALID_ID;
	}
	if (!p->mix_buf)
	{
		p->mix_buf = (pj_int32_t*)pj_pool_zalloc(p->pool, pjsua_var.mconf_cfg.samples_per_frame * sizeof(p->mix_buf[0]));
		if (!p->mix_buf)
		{
			PJSUA_UNLOCK();
			return PJ_ENOMEM;
		}
		p->last_mix_adj = NORMAL_LEVEL;
	}

	p->mix_cnt = 0U;
	p->mix_adj = NORMAL_LEVEL;
	p->play_data.status = PJSUA_REP_CONFERENCING;

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Started conference on media port %d", id));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_conf_add(pjsua_mport_id id, pjsua_mport_id pid)
{
	register pjsua_mport_data *p, *pp;
	register int i;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(pid >= 0 && (unsigned int)pid<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	pp = &pjsua_var.mport[pid];
	PJ_ASSERT_RETURN(pp->pool != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	if (p->play_data.status != PJSUA_REP_CONFERENCING)
	{
		PJSUA_UNLOCK();
		return PJ_ENOTFOUND;
	}

	for (i = (int)p->participant_cnt - 1; i >= 0; --i)
	{
		if (p->participants[i] == pid)
		{
			PJSUA_UNLOCK();
			return PJ_SUCCESS;
		}
	}

	if (p->participant_cnt >= PJSUA_MAX_CONF_PORTS)
	{
			PJSUA_UNLOCK();
			return PJ_ETOOMANY;
	}

	if (!pp->listeners)
	{
		pp->listeners = (pjsua_mport_id*)pj_pool_zalloc(pp->pool, sizeof(pjsua_mport_id) * pjsua_var.media_cfg.max_media_ports);
		if (!pp->listeners)
		{
			PJSUA_UNLOCK();
			return PJ_ENOMEM;
		}
		for (i = 0; i < (int)pjsua_var.media_cfg.max_media_ports; ++i)
			pp->listeners[i] = PJSUA_INVALID_ID;
	}

	pj_enter_critical_section();

	p->participants[p->participant_cnt++] = pid;

	for (i = (int)pp->listener_cnt - 1; i >= 0; --i)
	{
		if (pp->listeners[i] == id)
			break;
	}
	if (i < 0)
	{
		pj_assert(pp->listener_cnt < pjsua_var.media_cfg.max_media_ports);
		pp->listeners[pp->listener_cnt++] = id;
	}

	pj_leave_critical_section();

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Added media port %d to conference on media port %d", pid, id));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_conf_remove(pjsua_mport_id id, pjsua_mport_id pid)
{
	register pjsua_mport_data *p, *pp;
	register int i, j;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(pid >= 0 && (unsigned int)pid<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	pp = &pjsua_var.mport[pid];
	PJ_ASSERT_RETURN(pp->pool != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	if (p->play_data.status != PJSUA_REP_CONFERENCING)
	{
		PJSUA_UNLOCK();
		return PJ_ENOTFOUND;
	}

	pj_enter_critical_section();

	for (i = (int)(p->participant_cnt) - 1; i >= 0; --i)
	{
		if (p->participants[i] != pid)
			continue;
		for (j = (int)(pp->listener_cnt) - 1; j >= 0; --j)
		{
			if (pp->listeners[j] == id)
			{
				pp->listener_cnt--;
				if (j == (int)pp->listener_cnt)
				{
					pp->listeners[j] = PJSUA_INVALID_ID;
				}
				else
				{
					pp->listeners[j] = pp->listeners[pp->listener_cnt];
					pp->listeners[pp->listener_cnt] = PJSUA_INVALID_ID;
				}
				break;
			}
		}
		p->participant_cnt--;
		if (i == (int)p->participant_cnt)
		{
			p->participants[i] = PJSUA_INVALID_ID;
		}
		else
		{
			p->participants[i] = p->participants[p->participant_cnt];
			p->participants[p->participant_cnt] = PJSUA_INVALID_ID;
		}
		break;
	}

	pj_leave_critical_section();

	PJSUA_UNLOCK();

	if (i < 0)
		return PJ_ENOTFOUND;

	PJ_LOG(4, (THIS_FILE, "Removed media port %d from conference on media port %d", pid, id));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_conf_stop(pjsua_mport_id id)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	if (p->play_data.status != PJSUA_REP_CONFERENCING)
	{
		PJSUA_UNLOCK();
		return PJ_ENOTFOUND;
	}

	pj_enter_critical_section();

	for (register int i = (int)(p->participant_cnt) - 1; i >= 0; --i)
	{
		register pjsua_mport_data *pp = &pjsua_var.mport[p->participants[i]];
		for (register int j = (int)(pp->listener_cnt) - 1; j >= 0; --j)
		{
			if (pp->listeners[j] == id)
			{
				pp->listener_cnt--;
				if (j == (int)pp->listener_cnt)
				{
					pp->listeners[j] = PJSUA_INVALID_ID;
				}
				else
				{
					pp->listeners[j] = pp->listeners[pp->listener_cnt];
					pp->listeners[pp->listener_cnt] = PJSUA_INVALID_ID;
				}
				break;
			}
		}
		p->participants[i] = PJSUA_INVALID_ID;
		p->participant_cnt--;
	}

	p->play_data.status = PJSUA_REP_IDLE;

	pj_leave_critical_section();

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Stopped conference on media port %d", id));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_listen_for(pjsua_mport_id id, pjs_listen_for_parms* params)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	if (params && (params->types == PJSUA_RCG_NONE))
		params = NULL;

	PJSUA_LOCK();

	if (!(p->base.info.dir & PJMEDIA_DIR_CAPTURE))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}

	pj_enter_critical_section();

	if (params)
		p->recogntion_data.params = *params;
	else
		p->recogntion_data.params.types = PJSUA_RCG_NONE;

	pj_leave_critical_section();

	PJSUA_UNLOCK();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_get_recognised(pjsua_mport_id id, pjs_recognition_info* info)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(info != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	pj_enter_critical_section();

	if (p->recogntion_data.event_cnt > 0U)
	{
		*info = p->recogntion_data.events[0U];
		pj_array_erase(p->recogntion_data.events, sizeof(p->recogntion_data.events[0]), p->recogntion_data.event_cnt, 0U);
		if (!--p->recogntion_data.event_cnt)
		{
			if (p->recogntion_data.signaled)
			{
				pj_event_reset(p->recogntion_data.event);
				p->recogntion_data.signaled = PJ_FALSE;
			}
		}
	}
	else
	{
		info->type = PJSUA_RCG_NONE;
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_discard_recognised(pjsua_mport_id id)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	pj_enter_critical_section();

	if (p->recogntion_data.event_cnt > 0U)
	{
		p->recogntion_data.event_cnt = 0U;
		if (p->recogntion_data.signaled)
		{
			pj_event_reset(p->recogntion_data.event);
			p->recogntion_data.signaled = PJ_FALSE;
		}
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

static pj_status_t pjsua_mport_add_recognition_event(pjsua_mport_id id, const pjs_recognition_info* ri)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(ri != NULL && ri->type != PJSUA_RCG_NONE, PJ_EINVAL);
	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	register pj_status_t pje = PJ_SUCCESS;

	pj_enter_critical_section();

	if (!(ri->type & p->recogntion_data.params.types))
	{
		pje = PJ_EIGNORED;
	}
	else
	{
		if ((ri->type & PJSUA_RCG_DTMF) &&
			((p->recogntion_data.params.types & PJSUA_RCG_DTMF) == PJSUA_RCG_DTMF))
			p->recogntion_data.params.types &=
				~(!(ri->type & PJSUA_RCG_DTMF_RFC2833) ? PJSUA_RCG_DTMF_RFC2833 : PJSUA_RCG_DTMF_TONE);
		if (p->recogntion_data.event_cnt >= PJ_ARRAY_SIZE(p->recogntion_data.events))
		{
			pje = PJ_ETOOMANY;
		}
		else
		{
			p->recogntion_data.events[p->recogntion_data.event_cnt++] = *ri;
			if (!p->recogntion_data.signaled)
			{
				pj_event_set(p->recogntion_data.event);
				p->recogntion_data.signaled = PJ_TRUE;
			}
		}
	}

	pj_leave_critical_section();

	return pje;
}

PJ_DEF(pj_status_t) pjsua_mport_add_rfc2833_dtmf_digit(pjsua_mport_id id, char digit, unsigned timestamp, unsigned duration)
{
	pjs_recognition_info ri;
	ri.type = PJSUA_RCG_DTMF_RFC2833;
	ri.timestamp = timestamp;
	ri.param0 = (unsigned)(pj_uint8_t)digit;
	ri.param1 = duration;
	return pjsua_mport_add_recognition_event(id, &ri);
}

/*****************************************************************************
 * Sound devices.
 */

/*
 * Enum sound devices.
 */

PJ_DEF(pj_status_t) pjsua_enum_aud_devs( pjmedia_aud_dev_info info[],
                                         unsigned *count)
{
    unsigned i, dev_count;

    dev_count = pjmedia_aud_dev_count();

    if (dev_count > *count) dev_count = *count;

    for (i=0; i<dev_count; ++i) {
        pj_status_t status;

        status = pjmedia_aud_dev_get_info(i, &info[i]);
        if (status != PJ_SUCCESS)
            return status;
    }

    *count = dev_count;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsua_enum_snd_devs( pjmedia_snd_dev_info info[],
                                         unsigned *count)
{
    unsigned i, dev_count;

    dev_count = pjmedia_aud_dev_count();

    if (dev_count > *count) dev_count = *count;
    pj_bzero(info, dev_count * sizeof(pjmedia_snd_dev_info));

    for (i=0; i<dev_count; ++i) {
        pjmedia_aud_dev_info ai;
        pj_status_t status;

        status = pjmedia_aud_dev_get_info(i, &ai);
        if (status != PJ_SUCCESS)
            return status;

        strncpy(info[i].name, ai.name, sizeof(info[i].name));
        info[i].name[sizeof(info[i].name)-1] = '\0';
        info[i].input_count = ai.input_count;
        info[i].output_count = ai.output_count;
        info[i].default_samples_per_sec = ai.default_samples_per_sec;
    }

    *count = dev_count;

    return PJ_SUCCESS;
}

/* Create audio device parameter to open the device */
static pj_status_t create_aud_param(pjmedia_aud_param *param,
                                    pjmedia_aud_dev_index capture_dev,
                                    pjmedia_aud_dev_index playback_dev,
                                    unsigned clock_rate,
                                    unsigned channel_count,
                                    unsigned samples_per_frame,
                                    unsigned bits_per_sample,
                                    pj_bool_t use_default_settings)
{
    pj_status_t status;
    pj_bool_t speaker_only = (pjsua_var.snd_mode & PJSUA_SND_DEV_SPEAKER_ONLY);

    /* Normalize device ID with new convention about default device ID */
    if (playback_dev == PJMEDIA_AUD_DEFAULT_CAPTURE_DEV)
        playback_dev = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

    /* Create default parameters for the device */
    status = pjmedia_aud_dev_default_param((speaker_only? playback_dev:
                                            capture_dev), param);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Error retrieving default audio "
                                "device parameters", status);
        return status;
    }
    param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    param->rec_id = capture_dev;
    param->play_id = playback_dev;
    param->clock_rate = clock_rate;
    param->channel_count = channel_count;
    param->samples_per_frame = samples_per_frame;
    param->bits_per_sample = bits_per_sample;

    if (use_default_settings) {
        /* Reset the sound device settings. */
        pjsua_var.aud_open_cnt = 0;
    } else {
        /* Update the setting with user preference */
#define update_param(cap, field)                            \
        if (pjsua_var.aud_param.flags & cap) {              \
            param->flags |= cap;                            \
            param->field = pjsua_var.aud_param.field;       \
        }
        update_param(PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, input_vol);
        update_param(PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, output_vol);
        update_param(PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE, input_route);
        update_param(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE, output_route);
#undef update_param
    }

    /* Latency settings */
    param->flags |= (PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
                     PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY);
    param->input_latency_ms = pjsua_var.media_cfg.snd_rec_latency;
    param->output_latency_ms = pjsua_var.media_cfg.snd_play_latency;

    /* EC settings */
    if (pjsua_var.media_cfg.ec_tail_len) {
        param->flags |= (PJMEDIA_AUD_DEV_CAP_EC | PJMEDIA_AUD_DEV_CAP_EC_TAIL);
        param->ec_enabled = PJ_TRUE;
        param->ec_tail_ms = pjsua_var.media_cfg.ec_tail_len;
    } else {
        param->flags &= ~(PJMEDIA_AUD_DEV_CAP_EC|PJMEDIA_AUD_DEV_CAP_EC_TAIL);
    }

    /* VAD settings */
    if (pjsua_var.media_cfg.no_vad) {
        param->flags &= ~PJMEDIA_AUD_DEV_CAP_VAD;
    } else {
        param->flags |= PJMEDIA_AUD_DEV_CAP_VAD;
        param->vad_enabled = PJ_TRUE;
    }

    return PJ_SUCCESS;
}

/* Internal: the first time the audio device is opened (during app
 *   startup), retrieve the audio settings such as volume level
 *   so that aud_get_settings() will work.
 */
static pj_status_t update_initial_aud_param()
{
    pjmedia_aud_stream *strm;
    pjmedia_aud_param param;
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua_var.snd_port != NULL, PJ_EBUG);

    strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);

    status = pjmedia_aud_stream_get_param(strm, &param);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Error audio stream "
                                "device parameters", status);
        return status;
    }
    pjsua_var.aud_param.flags = 0;

#define update_saved_param(cap, field)  \
        if (param.flags & cap) { \
            pjsua_var.aud_param.flags |= cap; \
            pjsua_var.aud_param.field = param.field; \
        }

    update_saved_param(PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, input_vol);
    update_saved_param(PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, output_vol);
    update_saved_param(PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE, input_route);
    update_saved_param(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE, output_route);
#undef update_saved_param

    return PJ_SUCCESS;
}

/* Get format name */
static const char *get_fmt_name(pj_uint32_t id)
{
    static char name[8];

    if (id == PJMEDIA_FORMAT_L16)
        return "PCM";
    pj_memcpy(name, &id, 4);
    name[4] = '\0';
    return name;
}

static pj_status_t on_aud_prev_play_frame(void *user_data, pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(user_data);
    (*pjsua_var.media_cfg.on_aud_prev_play_frame)(frame);
    return PJ_SUCCESS;
}

static pj_status_t on_aud_prev_rec_frame(void *user_data, pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(user_data);
    (*pjsua_var.media_cfg.on_aud_prev_rec_frame)(frame);
    return PJ_SUCCESS;
}

/* Open sound device with the setting. */
static pj_status_t open_snd_dev(pjmedia_snd_port_param *param)
{
    pjmedia_port *conf_port;
    pj_status_t status;
    pj_bool_t speaker_only = (pjsua_var.snd_mode & PJSUA_SND_DEV_SPEAKER_ONLY);

    PJ_ASSERT_RETURN(param, PJ_EINVAL);

    /* Check if NULL sound device is used */
    if (PJSUA_SND_NULL_DEV==param->base.rec_id ||
        PJSUA_SND_NULL_DEV==param->base.play_id)
    {
        return pjsua_set_null_snd_dev();
    }

    /* Close existing sound port */
    close_snd_dev();

    /* Save the device IDs */
    pjsua_var.cap_dev = param->base.rec_id;
    pjsua_var.play_dev = param->base.play_id;

    /* Notify app */
    if (pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
        (*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(1);
    }

    /* Create memory pool for sound device. */
    pjsua_var.snd_pool = pjsua_pool_create("pjsua_snd", 4000, 4000);
    PJ_ASSERT_RETURN(pjsua_var.snd_pool, PJ_ENOMEM);

    /* Setup preview callbacks, if configured */
    if (pjsua_var.media_cfg.on_aud_prev_play_frame)
        param->on_play_frame = &on_aud_prev_play_frame;
    if (pjsua_var.media_cfg.on_aud_prev_rec_frame)
        param->on_rec_frame = &on_aud_prev_rec_frame;

    PJ_LOG(4,(THIS_FILE, "Opening sound device (%s) %s@%d/%d/%dms",
              speaker_only?"speaker only":"speaker + mic",
              get_fmt_name(param->base.ext_fmt.id),
              param->base.clock_rate, param->base.channel_count,
              param->base.samples_per_frame / param->base.channel_count *
              1000 / param->base.clock_rate));
    pj_log_push_indent();

    if (speaker_only) {
        pjmedia_snd_port_param cp_param;
        int dev_id = param->base.play_id;

        /* Normalize dev_id */
        if (dev_id < 0)
            dev_id = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

        pjmedia_snd_port_param_default(&cp_param);
        pj_memcpy(&cp_param.base, &param->base, sizeof(cp_param.base));
        cp_param.base.dir = PJMEDIA_DIR_PLAYBACK;
        cp_param.base.play_id = dev_id;

        status = pjmedia_snd_port_create2(pjsua_var.snd_pool, &cp_param,
                                          &pjsua_var.snd_port);

    } else {
        status = pjmedia_snd_port_create2(pjsua_var.snd_pool,
                                          param, &pjsua_var.snd_port);
    }

    if (status != PJ_SUCCESS)
        goto on_error;

    /* Get the port0 of the conference bridge. */
    conf_port = pjmedia_conf_get_master_port(pjsua_var.mconf);
    pj_assert(conf_port != NULL);

    /* For conference bridge, resample if necessary if the bridge's
     * clock rate is different than the sound device's clock rate.
     */
    if (!pjsua_var.is_mswitch &&
        param->base.ext_fmt.id == PJMEDIA_FORMAT_PCM &&
        PJMEDIA_PIA_SRATE(&conf_port->info) != param->base.clock_rate)
    {
        pjmedia_port *resample_port;
        unsigned resample_opt = 0;

        if (pjsua_var.media_cfg.quality >= 3 &&
            pjsua_var.media_cfg.quality <= 4)
        {
            resample_opt |= PJMEDIA_RESAMPLE_USE_SMALL_FILTER;
        }
        else if (pjsua_var.media_cfg.quality < 3) {
            resample_opt |= PJMEDIA_RESAMPLE_USE_LINEAR;
        }

        status = pjmedia_resample_port_create(pjsua_var.snd_pool,
                                              conf_port,
                                              param->base.clock_rate,
                                              resample_opt,
                                              &resample_port);
        if (status != PJ_SUCCESS) {
            char errmsg[PJ_ERR_MSG_SIZE];
            pj_strerror(status, errmsg, sizeof(errmsg));
            PJ_LOG(4, (THIS_FILE,
                       "Error creating resample port: %s",
                       errmsg));
            close_snd_dev();
            goto on_error;
        }

        conf_port = resample_port;
    }

    /* Otherwise for audio switchboard, the switch's port0 setting is
     * derived from the sound device setting, so update the setting.
     */
    if (pjsua_var.is_mswitch) {
        if (param->base.flags & PJMEDIA_AUD_DEV_CAP_EXT_FORMAT) {
            conf_port->info.fmt = param->base.ext_fmt;
        } else {
            unsigned bps, ptime_usec;
            bps = param->base.clock_rate * param->base.bits_per_sample;
            ptime_usec = param->base.samples_per_frame /
                         param->base.channel_count * 1000000 /
                         param->base.clock_rate;
            pjmedia_format_init_audio(&conf_port->info.fmt,
                                      PJMEDIA_FORMAT_PCM,
                                      param->base.clock_rate,
                                      param->base.channel_count,
                                      param->base.bits_per_sample,
                                      ptime_usec,
                                      bps, bps);
        }
    }


    /* Connect sound port to the bridge */
    status = pjmedia_snd_port_connect(pjsua_var.snd_port,
                                      conf_port );
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Unable to connect conference port to "
                                "sound device", status);
        pjmedia_snd_port_destroy(pjsua_var.snd_port);
        pjsua_var.snd_port = NULL;
        goto on_error;
    }

    /* Update sound device name. */
    if (!speaker_only) {
        pjmedia_aud_dev_info rec_info;
        pjmedia_aud_stream *strm;
        pjmedia_aud_param si;
        pj_str_t tmp;

        strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);
        status = pjmedia_aud_stream_get_param(strm, &si);
        if (status == PJ_SUCCESS)
            status = pjmedia_aud_dev_get_info(si.rec_id, &rec_info);

        if (status==PJ_SUCCESS) {
            if (param->base.clock_rate != pjsua_var.media_cfg.clock_rate) {
                char tmp_buf[128];
                int tmp_buf_len;

                tmp_buf_len = pj_ansi_snprintf(tmp_buf, sizeof(tmp_buf),
                                               "%s (%dKHz)",
                                               rec_info.name,
                                               param->base.clock_rate/1000);
                if (tmp_buf_len < 1 || tmp_buf_len >= (int)sizeof(tmp_buf))
                    tmp_buf_len = sizeof(tmp_buf) - 1;
                pj_strset(&tmp, tmp_buf, tmp_buf_len);
                pjmedia_conf_set_port0_name(pjsua_var.mconf, &tmp);
            } else {
                pjmedia_conf_set_port0_name(pjsua_var.mconf,
                                            pj_cstr(&tmp, rec_info.name));
            }
        }

        /* Any error is not major, let it through */
        status = PJ_SUCCESS;
    }

    /* If this is the first time the audio device is open, retrieve some
     * settings from the device (such as volume settings) so that the
     * pjsua_snd_get_setting() work.
     */
    if (pjsua_var.aud_open_cnt == 0) {
        update_initial_aud_param();
        ++pjsua_var.aud_open_cnt;
    }

    pjsua_var.snd_is_on = PJ_TRUE;

    /* Subscribe to audio device events */
    pjmedia_event_subscribe(NULL, &on_media_event, NULL,
                    pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port));

    pj_log_pop_indent();
    return PJ_SUCCESS;

on_error:
    pj_log_pop_indent();
    return status;
}


/* Close existing sound device */
static void close_snd_dev(void)
{
    pj_log_push_indent();

    /* Notify app */
    if (pjsua_var.snd_is_on && pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
        (*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(0);
    }

    /* Close sound device */
    if (pjsua_var.snd_port) {
        pjmedia_aud_dev_info cap_info, play_info;
        pjmedia_aud_stream *strm;
        pjmedia_aud_param param;

        strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);
        pjmedia_aud_stream_get_param(strm, &param);

        if (param.rec_id == PJSUA_SND_NO_DEV ||
            pjmedia_aud_dev_get_info(param.rec_id, &cap_info) != PJ_SUCCESS)
        {
            cap_info.name[0] = '\0';
        }
        if (pjmedia_aud_dev_get_info(param.play_id, &play_info) != PJ_SUCCESS)
            play_info.name[0] = '\0';

        PJ_LOG(4,(THIS_FILE, "Closing %s sound playback device and "
                             "%s sound capture device",
                             play_info.name, cap_info.name));

        /* Unsubscribe from audio device events */
        pjmedia_event_unsubscribe(NULL, &on_media_event, NULL, strm);

        pjmedia_snd_port_disconnect(pjsua_var.snd_port);
        pjmedia_snd_port_destroy(pjsua_var.snd_port);
        pjsua_var.snd_port = NULL;
    }

    /* Close null sound device */
    if (pjsua_var.null_snd) {
        PJ_LOG(4,(THIS_FILE, "Closing null sound device.."));
        pjmedia_master_port_destroy(pjsua_var.null_snd, PJ_FALSE);
        pjsua_var.null_snd = NULL;
    }

    if (pjsua_var.snd_pool)
        pj_pool_release(pjsua_var.snd_pool);

    pjsua_var.snd_pool = NULL;
    pjsua_var.snd_is_on = PJ_FALSE;

    pj_log_pop_indent();
}


PJ_DEF(pj_status_t) pjsua_set_snd_dev(int capture_dev,
                                      int playback_dev)
{
    pjsua_snd_dev_param param;

    pjsua_snd_dev_param_default(&param);
    pjsua_get_snd_dev2(&param);
    param.capture_dev = capture_dev;
    param.playback_dev = playback_dev;

    /* Always open the sound device. */
    param.mode &= ~PJSUA_SND_DEV_NO_IMMEDIATE_OPEN;

    return pjsua_set_snd_dev2(&param);
}


PJ_DEF(pj_status_t) pjsua_get_snd_dev2(pjsua_snd_dev_param *snd_param)
{
    PJ_ASSERT_RETURN(snd_param, PJ_EINVAL);

    PJSUA_LOCK();
    snd_param->capture_dev = pjsua_var.cap_dev;
    snd_param->playback_dev = pjsua_var.play_dev;
    snd_param->mode = pjsua_var.snd_mode;
    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Select or change sound device. Application may call this function at
 * any time to replace current sound device.
 */
PJ_DEF(pj_status_t) pjsua_set_snd_dev2(const pjsua_snd_dev_param *snd_param)
{
    unsigned alt_cr_cnt = 1;
    unsigned alt_cr[] = {0, 44100, 48000, 32000, 16000, 8000};
    unsigned i;
    pj_status_t status = -1;
    unsigned orig_snd_dev_mode = pjsua_var.snd_mode;

    PJ_ASSERT_RETURN(snd_param, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Set sound device: capture=%d, playback=%d, mode=%d, "
              "use_default_settings=%d",
              snd_param->capture_dev, snd_param->playback_dev,
              snd_param->mode, snd_param->use_default_settings));

    pj_log_push_indent();

    PJSUA_LOCK();

    /* Check if there are no changes in sound device settings */
    if (pjsua_var.cap_dev == snd_param->capture_dev &&
        pjsua_var.play_dev == snd_param->playback_dev &&
        pjsua_var.snd_mode == snd_param->mode)
    {
        /* If sound device is already opened, just print log and return.
         * Also if PJSUA_SND_DEV_NO_IMMEDIATE_OPEN is set.
         */
        if (pjsua_var.snd_is_on || (snd_param->mode &
                                    PJSUA_SND_DEV_NO_IMMEDIATE_OPEN))
        {
            PJ_LOG(4,(THIS_FILE,"No changes in capture and playback devices"));
            PJSUA_UNLOCK();
            pj_log_pop_indent();
            return PJ_SUCCESS;
        }
    }
    
    /* No sound */
    if (snd_param->capture_dev == PJSUA_SND_NO_DEV &&
        snd_param->playback_dev == PJSUA_SND_NO_DEV)
    {
        PJSUA_UNLOCK();
        PJ_LOG(4, (THIS_FILE, "No sound device, mode setting is ignored"));
        if (!pjsua_var.no_snd)
            pjsua_set_no_snd_dev();
        pj_log_pop_indent();
        return status;
    }

    /* Null-sound */
    if (snd_param->capture_dev == PJSUA_SND_NULL_DEV && 
        snd_param->playback_dev == PJSUA_SND_NULL_DEV) 
    {
        PJSUA_UNLOCK();
        PJ_LOG(4, (THIS_FILE, "Null sound device, mode setting is ignored"));
        status = pjsua_set_null_snd_dev();
        pj_log_pop_indent();
        return status;
    }

    pjsua_var.snd_mode = snd_param->mode;

    /* Just update the IDs if app does not want to open now and currently
     * audio is off.
     */
    if (!pjsua_var.snd_is_on &&
        (snd_param->mode & PJSUA_SND_DEV_NO_IMMEDIATE_OPEN))
    {
        pjsua_var.cap_dev = snd_param->capture_dev;
        pjsua_var.play_dev = snd_param->playback_dev;
        pjsua_var.no_snd = PJ_FALSE;

        PJSUA_UNLOCK(); 
        pj_log_pop_indent();
        return PJ_SUCCESS;
    }

    /* Set default clock rate */
    alt_cr[0] = pjsua_var.media_cfg.snd_clock_rate;
    if (alt_cr[0] == 0)
        alt_cr[0] = pjsua_var.media_cfg.clock_rate;

    /* Allow retrying of different clock rate if we're using conference
     * bridge (meaning audio format is always PCM), otherwise lock on
     * to one clock rate.
     */
    if (pjsua_var.is_mswitch) {
        alt_cr_cnt = 1;
    } else {
        alt_cr_cnt = PJ_ARRAY_SIZE(alt_cr);
    }

    /* Attempts to open the sound device with different clock rates */
    for (i=0; i<alt_cr_cnt; ++i) {
        pjmedia_snd_port_param param;
        unsigned samples_per_frame;

        /* Create the default audio param */
        samples_per_frame = alt_cr[i] *
                            pjsua_var.media_cfg.audio_frame_ptime *
                            pjsua_var.media_cfg.channel_count / 1000;
        pjmedia_snd_port_param_default(&param);
        param.ec_options = pjsua_var.media_cfg.ec_options;
        status = create_aud_param(&param.base, snd_param->capture_dev, 
                                  snd_param->playback_dev, 
                                  alt_cr[i], pjsua_var.media_cfg.channel_count,
                                  samples_per_frame, 16,
                                  snd_param->use_default_settings);
        if (status != PJ_SUCCESS)
            goto on_error;

        /* Open! */
        param.options = 0;
        status = open_snd_dev(&param);
        if (status == PJ_SUCCESS)
            break;
    }

    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Unable to open sound device", status);
        goto on_error;
    }

    pjsua_var.no_snd = PJ_FALSE;
    pjsua_var.snd_is_on = PJ_TRUE;

    PJSUA_UNLOCK();
    pj_log_pop_indent();
    return PJ_SUCCESS;

on_error:
    pjsua_var.snd_mode = orig_snd_dev_mode;
    PJSUA_UNLOCK();
    pj_log_pop_indent();
    return status;
}


/*
 * Get currently active sound devices. If sound devices has not been created
 * (for example when pjsua_start() is not called), it is possible that
 * the function returns PJ_SUCCESS with -1 as device IDs.
 */
PJ_DEF(pj_status_t) pjsua_get_snd_dev(int *capture_dev,
                                      int *playback_dev)
{
    PJSUA_LOCK();

    if (capture_dev) {
        *capture_dev = pjsua_var.cap_dev;
    }
    if (playback_dev) {
        *playback_dev = pjsua_var.play_dev;
    }

    PJSUA_UNLOCK();
    return PJ_SUCCESS;
}


/*
 * Use null sound device.
 */
PJ_DEF(pj_status_t) pjsua_set_null_snd_dev(void)
{
    pjmedia_port *conf_port;
    pj_status_t status;

    PJ_LOG(4,(THIS_FILE, "Setting null sound device.."));
    pj_log_push_indent();

    PJSUA_LOCK();

    /* Close existing sound device */
    close_snd_dev();

    pjsua_var.cap_dev = PJSUA_SND_NULL_DEV;
    pjsua_var.play_dev = PJSUA_SND_NULL_DEV;

    /* Notify app */
    if (pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
        (*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(1);
    }

    /* Create memory pool for sound device. */
    pjsua_var.snd_pool = pjsua_pool_create("pjsua_snd", 4000, 4000);
    PJ_ASSERT_RETURN(pjsua_var.snd_pool, PJ_ENOMEM);

    PJ_LOG(4,(THIS_FILE, "Opening null sound device.."));

    /* Get the port0 of the conference bridge. */
    conf_port = pjmedia_conf_get_master_port(pjsua_var.mconf);
    pj_assert(conf_port != NULL);

    /* Create master port, connecting port0 of the conference bridge to
     * a null port.
     */
    status = pjmedia_master_port_create(pjsua_var.snd_pool, pjsua_var.null_port,
                                        conf_port, 0, &pjsua_var.null_snd);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Unable to create null sound device",
                     status);
        PJSUA_UNLOCK();
        pj_log_pop_indent();
        return status;
    }

    /* Start the master port */
    status = pjmedia_master_port_start(pjsua_var.null_snd);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    pjsua_var.no_snd = PJ_FALSE;
    pjsua_var.snd_is_on = PJ_TRUE;

    PJSUA_UNLOCK();
    pj_log_pop_indent();
    return PJ_SUCCESS;
}



/*
 * Use no device!
 */
PJ_DEF(pjmedia_port*) pjsua_set_no_snd_dev(void)
{
    PJSUA_LOCK();

    /* Close existing sound device */
    close_snd_dev();
    pjsua_var.no_snd = PJ_TRUE;
    pjsua_var.cap_dev = PJSUA_SND_NO_DEV;
    pjsua_var.play_dev = PJSUA_SND_NO_DEV;

    PJSUA_UNLOCK();

    return pjmedia_conf_get_master_port(pjsua_var.mconf);
}


/*
 * Configure the AEC settings of the sound port.
 */
PJ_DEF(pj_status_t) pjsua_set_ec(unsigned tail_ms, unsigned options)
{
    pj_status_t status = PJ_SUCCESS;

    PJSUA_LOCK();

    pjsua_var.media_cfg.ec_tail_len = tail_ms;
    pjsua_var.media_cfg.ec_options = options;

    if (pjsua_var.snd_port)
        status = pjmedia_snd_port_set_ec(pjsua_var.snd_port, pjsua_var.pool,
                                         tail_ms, options);

    PJSUA_UNLOCK();
    return status;
}


/*
 * Get current AEC tail length.
 */
PJ_DEF(pj_status_t) pjsua_get_ec_tail(unsigned *p_tail_ms)
{
    *p_tail_ms = pjsua_var.media_cfg.ec_tail_len;
    return PJ_SUCCESS;
}


/*
 * Get echo canceller statistics.
 */
PJ_DEF(pj_status_t) pjsua_get_ec_stat(pjmedia_echo_stat *p_stat)
{
    if (pjsua_var.snd_port) {
        return pjmedia_snd_port_get_ec_stat(pjsua_var.snd_port, p_stat);
    } else {
        return PJ_ENOTFOUND;
    }
}


/*
 * Check whether the sound device is currently active.
 */
PJ_DEF(pj_bool_t) pjsua_snd_is_active(void)
{
    return pjsua_var.snd_port != NULL;
}


/*
 * Configure sound device setting to the sound device being used.
 */
PJ_DEF(pj_status_t) pjsua_snd_set_setting( pjmedia_aud_dev_cap cap,
                                           const void *pval,
                                           pj_bool_t keep)
{
    pj_status_t status;

    /* Check if we are allowed to set the cap */
    if ((cap & pjsua_var.aud_svmask) == 0) {
        return PJMEDIA_EAUD_INVCAP;
    }

    PJSUA_LOCK();

    /* If sound is active, set it immediately */
    if (pjsua_snd_is_active()) {
        pjmedia_aud_stream *strm;

        strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);
        status = pjmedia_aud_stream_set_cap(strm, cap, pval);
    } else {
        status = PJ_SUCCESS;
    }

    if (status != PJ_SUCCESS) {
        PJSUA_UNLOCK();
        return status;
    }

    /* Save in internal param for later device open */
    if (keep) {
        status = pjmedia_aud_param_set_cap(&pjsua_var.aud_param,
                                           cap, pval);
    }

    PJSUA_UNLOCK();
    return status;
}

/*
 * Retrieve a sound device setting.
 */
PJ_DEF(pj_status_t) pjsua_snd_get_setting( pjmedia_aud_dev_cap cap,
                                           void *pval)
{
    pj_status_t status;

    PJSUA_LOCK();

    /* If sound device has never been opened before, open it to
     * retrieve the initial setting from the device (e.g. audio
     * volume)
     */
    if (pjsua_var.aud_open_cnt==0) {
        PJ_LOG(4,(THIS_FILE, "Opening sound device to get initial settings"));
        pjsua_set_snd_dev(pjsua_var.cap_dev, pjsua_var.play_dev);
        close_snd_dev();
    }

    if (pjsua_snd_is_active()) {
        /* Sound is active, retrieve from device directly */
        pjmedia_aud_stream *strm;

        strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);
        status = pjmedia_aud_stream_get_cap(strm, cap, pval);
    } else {
        /* Otherwise retrieve from internal param */
        status = pjmedia_aud_param_get_cap(&pjsua_var.aud_param,
                                           cap, pval);
    }

    PJSUA_UNLOCK();
    return status;
}


/*
 * Extra sound device
 */
struct pjsua_ext_snd_dev
{
    pj_pool_t           *pool;
    pjmedia_port        *splitcomb;
    pjmedia_port        *rev_port;
    pjmedia_snd_port    *snd_port;
    pjsua_conf_port_id   port_id;
};


/*
 * Create an extra sound device and register it to conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_ext_snd_dev_create( pjmedia_snd_port_param *param,
                                              pjsua_ext_snd_dev **p_snd)
{
    pjsua_ext_snd_dev *snd = NULL;
    pj_pool_t *pool;
    pj_status_t status;

    PJ_ASSERT_RETURN(param && p_snd, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->base.channel_count == 1, PJMEDIA_ENCCHANNEL);

    pool = pjsua_pool_create("extsnd%p", 512, 512);
    if (!pool)
        return PJ_ENOMEM;

    snd = PJ_POOL_ZALLOC_T(pool, pjsua_ext_snd_dev);
    if (!snd) {
        pj_pool_release(pool);
        return PJ_ENOMEM;
    }

    snd->pool = pool;
    snd->port_id = PJSUA_INVALID_ID;

    /* Create mono splitter/combiner */
    status = pjmedia_splitcomb_create(
                                    pool, 
                                    param->base.clock_rate,
                                    param->base.channel_count,
                                    param->base.samples_per_frame,
                                    param->base.bits_per_sample,
                                    0,  /* options */
                                    &snd->splitcomb);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Create reverse channel */
    status = pjmedia_splitcomb_create_rev_channel(
                                    pool,
                                    snd->splitcomb,
                                    0   /* channel #1 */,
                                    0   /* options */,
                                    &snd->rev_port);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* And register it to conference bridge */
    status = pjsua_conf_add_port(pool, snd->rev_port, &snd->port_id);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Create sound device */
    status = pjmedia_snd_port_create2(pool, param, &snd->snd_port);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Connect the splitter to the sound device */
    status = pjmedia_snd_port_connect(snd->snd_port, snd->splitcomb);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Finally */
    *p_snd = snd;
    PJ_LOG(4,(THIS_FILE, "Extra sound device created"));

on_return:
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Failed creating extra sound device", status);
        pjsua_ext_snd_dev_destroy(snd);
    }

    return status;
}


/*
 * Destroy an extra sound device and unregister it from conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_ext_snd_dev_destroy(pjsua_ext_snd_dev *snd)
{
    PJ_ASSERT_RETURN(snd, PJ_EINVAL);

    /* Unregister from the conference bridge */
    if (snd->port_id != PJSUA_INVALID_ID) {
        pjsua_conf_remove_port(snd->port_id);
        snd->port_id = PJSUA_INVALID_ID;
    }

    /* Destroy all components */
    if (snd->snd_port) {
        pjmedia_snd_port_disconnect(snd->snd_port);
        pjmedia_snd_port_destroy(snd->snd_port);
        snd->snd_port = NULL;
    }
    if (snd->rev_port) {
        pjmedia_port_destroy(snd->rev_port);
        snd->rev_port = NULL;
    }
    if (snd->splitcomb) {
        pjmedia_port_destroy(snd->splitcomb);
        snd->splitcomb = NULL;
    }

    /* Finally */
    pj_pool_safe_release(&snd->pool);

    PJ_LOG(4,(THIS_FILE, "Extra sound device destroyed"));

    return PJ_SUCCESS;
}


/*
 * Get sound port instance of an extra sound device.
 */
PJ_DEF(pjmedia_snd_port*) pjsua_ext_snd_dev_get_snd_port(
                                            pjsua_ext_snd_dev *snd)
{
    PJ_ASSERT_RETURN(snd, NULL);
    return snd->snd_port;
}

/*
 * Get conference port ID of an extra sound device.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_ext_snd_dev_get_conf_port(
                                            pjsua_ext_snd_dev *snd)
{
    PJ_ASSERT_RETURN(snd, PJSUA_INVALID_ID);
    return snd->port_id;
}


#endif /* PJSUA_MEDIA_HAS_PJMEDIA */
