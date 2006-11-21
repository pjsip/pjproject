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
#include <pjmedia/splitcomb.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>


#define SIGNATURE	    PJMEDIA_PORT_SIGNATURE('S', 'p', 'C', 'b')
#define SIGNATURE_PORT	    PJMEDIA_PORT_SIGNATURE('S', 'p', 'C', 'P')
#define THIS_FILE	    "splitcomb.c"
#define TMP_SAMP_TYPE	    pj_int16_t
#define MAX_BUF_CNT	    32

#if 0
#   define TRACE_UP_(x)	PJ_LOG(5,x)
#   define TRACE_DN_(x)	PJ_LOG(5,x)
#else
#   define TRACE_UP_(x)
#   define TRACE_DN_(x)
#endif

#if 1
#   define LOG_UP_(x)	PJ_LOG(5,x)
#   define LOG_DN_(x)	PJ_LOG(5,x)
#else
#   define LOG_UP_(x)
#   define LOG_DN_(x)
#endif

/*
 * This structure describes the splitter/combiner.
 */
struct splitcomb
{
    pjmedia_port      base;

    unsigned	      options;

    /* Array of ports, one for each channel */
    struct {
	pjmedia_port *port;
	pj_bool_t     reversed;
    } *port_desc;

    /* Temporary buffers needed to extract mono frame from
     * multichannel frame. We could use stack for this, but this
     * way it should be safer for devices with small stack size.
     */
    TMP_SAMP_TYPE    *get_buf;
    TMP_SAMP_TYPE    *put_buf;
};


/*
 * This structure describes reverse port.
 */
struct reverse_port
{
    pjmedia_port     base;
    struct splitcomb*parent;
    unsigned	     ch_num;

    /* A reverse port need a temporary buffer to store frame
     * (because of the different phase, see splitcomb.h for details). 
     * Since we can not expect get_frame() and put_frame() to be
     * called evenly one after another, we use circular buffers to
     * accomodate the "jitter".
     */
    unsigned	     buf_cnt;

    /* Downstream is the direction when get_frame() is called to the
     * splitter/combiner port.
     */
    unsigned	     dn_read_pos, dn_write_pos, 
		     dn_overflow_pos, dn_underflow_pos;
    pj_int16_t	    *dnstream_buf[MAX_BUF_CNT];

    /* Upstream is the direction when put_frame() is called to the
     * splitter/combiner port.
     */
    unsigned	     up_read_pos, up_write_pos, 
		     up_overflow_pos, up_underflow_pos;
    pj_int16_t	    *upstream_buf[MAX_BUF_CNT];
};


/*
 * Prototypes.
 */
static pj_status_t put_frame(pjmedia_port *this_port, 
			     const pjmedia_frame *frame);
static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame);
static pj_status_t on_destroy(pjmedia_port *this_port);

static pj_status_t rport_put_frame(pjmedia_port *this_port, 
				   const pjmedia_frame *frame);
static pj_status_t rport_get_frame(pjmedia_port *this_port, 
				   pjmedia_frame *frame);
static pj_status_t rport_on_destroy(pjmedia_port *this_port);


/*
 * Create the splitter/combiner.
 */
PJ_DEF(pj_status_t) pjmedia_splitcomb_create( pj_pool_t *pool,
					      unsigned clock_rate,
					      unsigned channel_count,
					      unsigned samples_per_frame,
					      unsigned bits_per_sample,
					      unsigned options,
					      pjmedia_port **p_splitcomb)
{
    const pj_str_t name = pj_str("splitcomb");
    struct splitcomb *sc;

    /* Sanity check */
    PJ_ASSERT_RETURN(pool && clock_rate && channel_count &&
		     samples_per_frame && bits_per_sample &&
		     p_splitcomb, PJ_EINVAL);

    /* Only supports 16 bits per sample */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    *p_splitcomb = NULL;

    /* Create the splitter/combiner structure */
    sc = pj_pool_zalloc(pool, sizeof(struct splitcomb));
    PJ_ASSERT_RETURN(sc != NULL, PJ_ENOMEM);

    /* Create temporary buffers */
    sc->get_buf = pj_pool_alloc(pool, samples_per_frame * 
				      sizeof(TMP_SAMP_TYPE) /
				      channel_count);
    PJ_ASSERT_RETURN(sc->get_buf, PJ_ENOMEM);

    sc->put_buf = pj_pool_alloc(pool, samples_per_frame * 
				      sizeof(TMP_SAMP_TYPE) /
				      channel_count);
    PJ_ASSERT_RETURN(sc->put_buf, PJ_ENOMEM);


    /* Save options */
    sc->options = options;

    /* Initialize port */
    pjmedia_port_info_init(&sc->base.info, &name, SIGNATURE, clock_rate,
			   channel_count, bits_per_sample, samples_per_frame);

    sc->base.put_frame = &put_frame;
    sc->base.get_frame = &get_frame;
    sc->base.on_destroy = &on_destroy;

    /* Init ports array */
    sc->port_desc = pj_pool_zalloc(pool, channel_count*sizeof(*sc->port_desc));

    /* Done for now */
    *p_splitcomb = &sc->base;

    return PJ_SUCCESS;
}


/*
 * Attach media port with the same phase as the splitter/combiner.
 */
PJ_DEF(pj_status_t) pjmedia_splitcomb_set_channel( pjmedia_port *splitcomb,
						   unsigned ch_num,
						   unsigned options,
						   pjmedia_port *port)
{
    struct splitcomb *sc = (struct splitcomb*) splitcomb;

    /* Sanity check */
    PJ_ASSERT_RETURN(splitcomb && port, PJ_EINVAL);

    /* Make sure this is really a splitcomb port */
    PJ_ASSERT_RETURN(sc->base.info.signature == SIGNATURE, PJ_EINVAL);

    /* Check the channel number */
    PJ_ASSERT_RETURN(ch_num < sc->base.info.channel_count, PJ_EINVAL);

    /* options is unused for now */
    PJ_UNUSED_ARG(options);

    sc->port_desc[ch_num].port = port;
    sc->port_desc[ch_num].reversed = PJ_FALSE;

    return PJ_SUCCESS;
}


/*
 * Create reverse phase port for the specified channel.
 */
PJ_DEF(pj_status_t) 
pjmedia_splitcomb_create_rev_channel( pj_pool_t *pool,
				      pjmedia_port *splitcomb,
				      unsigned ch_num,
				      unsigned options,
				      pjmedia_port **p_chport)
{
    const pj_str_t name = pj_str("splitcomb-ch");
    struct splitcomb *sc = (struct splitcomb*) splitcomb;
    struct reverse_port *rport;
    unsigned i;
    pjmedia_port *port;

    /* Sanity check */
    PJ_ASSERT_RETURN(pool && splitcomb, PJ_EINVAL);

    /* Make sure this is really a splitcomb port */
    PJ_ASSERT_RETURN(sc->base.info.signature == SIGNATURE, PJ_EINVAL);

    /* Check the channel number */
    PJ_ASSERT_RETURN(ch_num < sc->base.info.channel_count, PJ_EINVAL);

    /* options is unused for now */
    PJ_UNUSED_ARG(options);

    /* Create the port */
    rport = pj_pool_zalloc(pool, sizeof(struct reverse_port));
    rport->parent = sc;
    rport->ch_num = ch_num;

    /* Initialize port info... */
    port = &rport->base;
    pjmedia_port_info_init(&port->info, &name, SIGNATURE_PORT, 
			   splitcomb->info.clock_rate, 1, 
			   splitcomb->info.bits_per_sample, 
			   splitcomb->info.samples_per_frame / 
				   splitcomb->info.channel_count);

    /* ... and the callbacks */
    port->put_frame = &rport_put_frame;
    port->get_frame = &rport_get_frame;
    port->on_destroy = &rport_on_destroy;


    rport->buf_cnt = options & 0xFF;
    if (rport->buf_cnt == 0)
	rport->buf_cnt = MAX_BUF_CNT;

    /* Create put buffers */
    for (i=0; i<rport->buf_cnt; ++i) {
	rport->dnstream_buf[i] = pj_pool_zalloc(pool, port->info.bytes_per_frame);
	PJ_ASSERT_RETURN(rport->dnstream_buf[i], PJ_ENOMEM);
    }
    rport->dn_write_pos = rport->buf_cnt/2;

    /* Create get buffers */
    for (i=0; i<rport->buf_cnt; ++i) {
	rport->upstream_buf[i] = pj_pool_zalloc(pool, 
						port->info.bytes_per_frame);
	PJ_ASSERT_RETURN(rport->upstream_buf[i], PJ_ENOMEM);
    }
    rport->up_write_pos = rport->buf_cnt/2;


    /* Save port in the splitcomb */
    sc->port_desc[ch_num].port = &rport->base;
    sc->port_desc[ch_num].reversed = PJ_TRUE;


    /* Done */
    *p_chport = port;
    return PJ_SUCCESS;
}


/* 
 * Extract one mono frame from a multichannel frame. 
 */
static void extract_mono_frame( const pj_int16_t *in,
			        pj_int16_t *out,
				unsigned ch,
				unsigned ch_cnt,
				unsigned samples_count)
{
    unsigned i;

    in += ch;
    for (i=0; i<samples_count; ++i) {
	*out++ = *in;
	in += ch_cnt;
    }
}


/* 
 * Put one mono frame into a multichannel frame 
 */
static void store_mono_frame( const pj_int16_t *in,
			      pj_int16_t *out,
			      unsigned ch,
			      unsigned ch_cnt,
			      unsigned samples_count)
{
    unsigned i;

    out += ch;
    for (i=0; i<samples_count; ++i) {
	*out = *in++;
	out += ch_cnt;
    }
}


/*
 * "Write" a multichannel frame. This would split the multichannel frame
 * into individual mono channel, and write it to the appropriate port.
 */
static pj_status_t put_frame(pjmedia_port *this_port, 
			     const pjmedia_frame *frame)
{
    struct splitcomb *sc = (struct splitcomb*) this_port;
    unsigned ch;

    /* Handle null frame */
    if (frame->type == PJMEDIA_FRAME_TYPE_NONE) {
	for (ch=0; ch < this_port->info.channel_count; ++ch) {
	    pjmedia_port *port = sc->port_desc[ch].port;

	    if (!port) continue;

	    pjmedia_port_put_frame(port, frame);
	}
	return PJ_SUCCESS;
    }

    /* Not sure how we would handle partial frame, so better reject
     * it for now.
     */
    PJ_ASSERT_RETURN(frame->size == this_port->info.bytes_per_frame,
		     PJ_EINVAL);

    /* 
     * Write mono frame into each channels 
     */
    for (ch=0; ch < this_port->info.channel_count; ++ch) {
	pjmedia_port *port = sc->port_desc[ch].port;

	if (!port)
	    continue;

	if (!sc->port_desc[ch].reversed) {
	    /* Write to normal port */
	    pjmedia_frame mono_frame;

	    /* Extract the mono frame */
	    extract_mono_frame(frame->buf, sc->put_buf, ch, 
			       this_port->info.channel_count, 
			       frame->size * 8 / 
				 this_port->info.bits_per_sample /
				 this_port->info.channel_count);

	    mono_frame.buf = sc->put_buf;
	    mono_frame.size = frame->size / this_port->info.channel_count;
	    mono_frame.type = frame->type;
	    mono_frame.timestamp.u64 = frame->timestamp.u64;

	    /* Write */
	    pjmedia_port_put_frame(port, &mono_frame);

	} else {
	    /* Write to reversed phase port */
	    struct reverse_port *rport = (struct reverse_port*)port;
	    
	    if (rport->dn_write_pos == rport->dn_read_pos) {

		/* Only report overflow if the frame is constantly read
		 * by the 'consumer' of the reverse port.
		 * It is possible that nobody reads the buffer, so causing
		 * overflow to happen rapidly, and writing log message this
		 * way does not seem to be wise.
		 */
		if (rport->dn_read_pos != rport->dn_overflow_pos) {
		    rport->dn_overflow_pos = rport->dn_read_pos;
		    LOG_DN_((THIS_FILE, "Overflow in downstream direction"));
		}

		/* Adjust write position */
		rport->dn_write_pos = 
		    (rport->dn_write_pos + rport->buf_cnt/2) % 
		    rport->buf_cnt;
	    }

	    /* Extract mono-frame and put it in downstream buffer */
	    extract_mono_frame(frame->buf, 
			       rport->dnstream_buf[rport->dn_write_pos],
			       ch, this_port->info.channel_count, 
			       frame->size * 8 / 
				 this_port->info.bits_per_sample /
				 this_port->info.channel_count);

	    rport->dn_write_pos = (rport->dn_write_pos + 1) %
			           rport->buf_cnt;
	}
    }

    return PJ_SUCCESS;
}


/*
 * Get a multichannel frame.
 * This will get mono channel frame from each port and put the
 * mono frame into the multichannel frame.
 */
static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame)
{
    struct splitcomb *sc = (struct splitcomb*) this_port;
    unsigned ch;
    pj_bool_t has_frame = PJ_FALSE;

    /* Clear output frame */
    pjmedia_zero_samples(frame->buf, this_port->info.samples_per_frame);

    /* Read frame from each port */
    for (ch=0; ch < this_port->info.channel_count; ++ch) {
	pjmedia_port *port = sc->port_desc[ch].port;
	pjmedia_frame mono_frame;
	pj_status_t status;

	if (!port)
	    continue;

	/* Read from the port */
	if (sc->port_desc[ch].reversed == PJ_FALSE) {
	    /* Read from normal port */
	    mono_frame.buf = sc->get_buf;
	    mono_frame.size = port->info.bytes_per_frame;
	    mono_frame.timestamp.u64 = frame->timestamp.u64;

	    status = pjmedia_port_get_frame(port, &mono_frame);
	    if (status != PJ_SUCCESS || 
		mono_frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
	    {
		continue;
	    }

	    /* Combine the mono frame into multichannel frame */
	    store_mono_frame(mono_frame.buf, frame->buf, ch,
			     this_port->info.channel_count,
			     mono_frame.size * 8 /
				this_port->info.bits_per_sample);

	    frame->timestamp.u64 = mono_frame.timestamp.u64;

	} else {
	    /* Read from temporary buffer for reverse port */
	    struct reverse_port *rport = (struct reverse_port*)port;

	    /* Check for underflows */
	    if (rport->up_read_pos == rport->up_write_pos) {

		/* Only report underflow if the buffer is constantly filled
		 * up at the other side.
		 * It is possible that nobody writes the buffer, so causing
		 * underflow to happen rapidly, and writing log message this
		 * way does not seem to be wise.
		 */
		if (rport->up_write_pos != rport->up_underflow_pos) {
		    rport->up_underflow_pos = rport->up_write_pos;
		    LOG_UP_((THIS_FILE, "Underflow in upstream direction"));
		}

		/* Adjust read position */
		rport->up_read_pos = 
		    (rport->up_write_pos - rport->buf_cnt/2) %
		    rport->buf_cnt;
	    }

	    TRACE_UP_((THIS_FILE, "Upstream read at buffer pos %d", 
		       rport->up_read_pos));

	    /* Combine the mono frame into multichannel frame */
	    store_mono_frame(rport->upstream_buf[rport->up_read_pos], 
			     frame->buf, ch,
			     this_port->info.channel_count,
			     port->info.samples_per_frame);

	    rport->up_read_pos = (rport->up_read_pos + 1) %
				   rport->buf_cnt;
	}


	has_frame = PJ_TRUE;
    }

    /* Return NO_FRAME is we don't get any frames from downstream ports */
    if (has_frame) {
	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame->size = this_port->info.bytes_per_frame;
    } else
	frame->type = PJMEDIA_FRAME_TYPE_NONE;

    return PJ_SUCCESS;
}


static pj_status_t on_destroy(pjmedia_port *this_port)
{
    /* Nothing to do */
    PJ_UNUSED_ARG(this_port);

    return PJ_SUCCESS;
}


/*
 * Get a mono frame from a reversed phase channel.
 */
static pj_status_t rport_put_frame(pjmedia_port *this_port, 
				   const pjmedia_frame *frame)
{
    struct reverse_port *rport = (struct reverse_port*) this_port;
    unsigned count;

    pj_assert(frame->size <= rport->base.info.bytes_per_frame);

    /* Check for overflows */
    if (rport->up_write_pos == rport->up_read_pos) {

	/* Only report overflow if the frame is constantly read
	 * at the other end of the buffer (the multichannel side).
	 * It is possible that nobody reads the buffer, so causing
	 * overflow to happen rapidly, and writing log message this
	 * way does not seem to be wise.
	 */
	if (rport->up_read_pos != rport->up_overflow_pos) {
	    rport->up_overflow_pos = rport->up_read_pos;
	    LOG_UP_((THIS_FILE, "Overflow in upstream direction"));
	}

	/* Adjust the write position */
	rport->up_write_pos = (rport->up_read_pos + rport->buf_cnt/2) %
			       rport->buf_cnt;
    }

    /* Handle NULL frame */
    if (frame->type != PJMEDIA_FRAME_TYPE_AUDIO) {
	TRACE_UP_((THIS_FILE, "Upstream write %d null samples at buf pos %d",
		   this_port->info.samples_per_frame, rport->up_write_pos));
	pjmedia_zero_samples(rport->upstream_buf[rport->up_write_pos],
			     this_port->info.samples_per_frame);
	rport->up_write_pos = (rport->up_write_pos+1) % rport->buf_cnt;
	return PJ_SUCCESS;
    }

    /* Not sure how to handle partial frame, so better reject for now */
    PJ_ASSERT_RETURN(frame->size == this_port->info.bytes_per_frame,
		     PJ_EINVAL);

    /* Copy normal frame to curcular buffer */
    count = frame->size * 8 / this_port->info.bits_per_sample;

    TRACE_UP_((THIS_FILE, "Upstream write %d samples at buf pos %d",
	       count, rport->up_write_pos));


    pjmedia_copy_samples(rport->upstream_buf[rport->up_write_pos],
			 frame->buf, count);
    rport->up_write_pos = (rport->up_write_pos+1) % rport->buf_cnt;

    return PJ_SUCCESS;
}


/*
 * Get a mono frame from a reversed phase channel.
 */
static pj_status_t rport_get_frame(pjmedia_port *this_port, 
				   pjmedia_frame *frame)
{
    struct reverse_port *rport = (struct reverse_port*) this_port;
    unsigned count;

    count = rport->base.info.samples_per_frame;

    frame->size = this_port->info.bytes_per_frame;
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;

    /* Check for underflows */
    if (rport->dn_read_pos == rport->dn_write_pos) {

	/* Only report underflow if the buffer is constantly filled
	 * up at the other side.
	 * It is possible that nobody writes the buffer, so causing
	 * underflow to happen rapidly, and writing log message this
	 * way does not seem to be wise.
	 */
	if (rport->dn_write_pos != rport->dn_underflow_pos) {
	    rport->dn_underflow_pos = rport->dn_write_pos;
	    LOG_DN_((THIS_FILE, "Underflow in downstream direction"));
	}

	/* Adjust read position */
	rport->dn_read_pos = 
	    (rport->dn_write_pos - rport->buf_cnt/2) % rport->buf_cnt;
	
    }

    /* Get the samples from the circular buffer */
    pjmedia_copy_samples(frame->buf, 
			 rport->dnstream_buf[rport->dn_read_pos],
			 count);
    rport->dn_read_pos = (rport->dn_read_pos+1) % rport->buf_cnt;

    return PJ_SUCCESS;
}


static pj_status_t rport_on_destroy(pjmedia_port *this_port)
{
    /* Nothing to do */
    PJ_UNUSED_ARG(this_port);

    return PJ_SUCCESS;
}

