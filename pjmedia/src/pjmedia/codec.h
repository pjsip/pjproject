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
#ifndef __PJMEDIA_CODEC_H__
#define __PJMEDIA_CODEC_H__


/**
 * @file codec.h
 * @brief Codec framework.
 */

#include <pj/list.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJMED_CODEC Codec framework.
 * @ingroup PJMEDIA
 * @{
 */

/** Top most media type. */
typedef enum pj_media_type
{
    /** No type. */
    PJ_MEDIA_TYPE_NONE = 0,

    /** The media is audio */
    PJ_MEDIA_TYPE_AUDIO = 1,

    /** The media is video. */
    PJ_MEDIA_TYPE_VIDEO = 2,

    /** Unknown media type, in this case the name will be specified in 
     *  encoding_name.
     */
    PJ_MEDIA_TYPE_UNKNOWN = 3,

} pj_media_type;


/** Media direction. */
typedef enum pj_media_dir_t
{
    /** None */
    PJ_MEDIA_DIR_NONE = 0,

    /** Encoding (outgoing to network) stream */
    PJ_MEDIA_DIR_ENCODING = 1,

    /** Decoding (incoming from network) stream. */
    PJ_MEDIA_DIR_DECODING = 2,

    /** Incoming and outgoing stream. */
    PJ_MEDIA_DIR_ENCODING_DECODING = 3,

} pj_media_dir_t;


/** Standard RTP paylist types. */
typedef enum pj_rtp_pt
{
    PJ_RTP_PT_PCMU = 0,		/* audio PCMU */
    PJ_RTP_PT_GSM  = 3,		/* audio GSM */
    PJ_RTP_PT_G723 = 4,		/* audio G723 */
    PJ_RTP_PT_DVI4_8K = 5,	/* audio DVI4 8KHz */
    PJ_RTP_PT_DVI4_16K = 6,	/* audio DVI4 16Khz */
    PJ_RTP_PT_LPC = 7,		/* audio LPC */
    PJ_RTP_PT_PCMA = 8,		/* audio PCMA */
    PJ_RTP_PT_G722 = 9,		/* audio G722 */
    PJ_RTP_PT_L16_2 = 10,	/* audio 16bit linear 44.1KHz stereo */
    PJ_RTP_PT_L16_1 = 11,	/* audio 16bit linear 44.1KHz mono */
    PJ_RTP_PT_QCELP = 12,	/* audio QCELP */
    PJ_RTP_PT_CN = 13,		/* audio Comfort Noise */
    PJ_RTP_PT_MPA = 14,		/* audio MPEG1 or MPEG2 as elementary streams */
    PJ_RTP_PT_G728 = 15,	/* audio G728 */
    PJ_RTP_PT_DVI4_11K = 16,	/* audio DVI4 11.025KHz mono */
    PJ_RTP_PT_DVI4_22K = 17,	/* audio DVI4 22.050KHz mono */
    PJ_RTP_PT_G729 = 18,	/* audio G729 */
    PJ_RTP_PT_CELB = 25,	/* video/comb Cell-B by Sun Microsystems (RFC 2029) */
    PJ_RTP_PT_JPEG = 26,	/* video JPEG */
    PJ_RTP_PT_NV = 28,		/* video NV implemented by nv program by Xerox */
    PJ_RTP_PT_H261 = 31,	/* video H261 */
    PJ_RTP_PT_MPV = 32,		/* video MPEG1 or MPEG2 elementary streams */
    PJ_RTP_PT_MP2T = 33,	/* video MPEG2 transport */
    PJ_RTP_PT_H263 = 34,	/* video H263 */

    PJ_RTP_PT_DYNAMIC = 96,	/* start of dynamic RTP payload */
} pj_rtp_pt;


/** Identification used to search for codec factory that supports specific 
 *  codec specification. 
 */
typedef struct pj_codec_id
{
    /** Media type. */
    pj_media_type   type;

    /** Payload type (can be dynamic). */
    unsigned	    pt;

    /** Encoding name, must be present if the payload type is dynamic. */
    pj_str_t	    encoding_name;

    /** Sampling rate. */
    unsigned	    sample_rate;
} pj_codec_id;


/** Detailed codec attributes used both to configure a codec and to query
 *  the capability of codec factories.
 */
typedef struct pj_codec_attr
{
    pj_uint32_t	sample_rate;	    /* Sampling rate in Hz */
    pj_uint32_t	avg_bps;	    /* Average bandwidth in bits per second */

    pj_uint8_t	pcm_bits_per_sample;/* Bits per sample in the PCM side */
    pj_uint16_t	ptime;		    /* Packet time in miliseconds */

    unsigned	pt:8;		    /* Payload type. */
    unsigned    vad_enabled:1;	    /* Voice Activity Detector. */
    unsigned    cng_enabled:1;	    /* Comfort Noise Generator. */
    unsigned    lpf_enabled:1;	    /* Low pass filter */
    unsigned    hpf_enabled:1;	    /* High pass filter */
    unsigned    penh_enabled:1;	    /* Perceptual Enhancement */
    unsigned    concl_enabled:1;    /* Packet loss concealment */
    unsigned    reserved_bit:1;

} pj_codec_attr;

/** Types of audio frame. */
typedef enum pj_audio_frame_type
{
    /** The frame is a silence audio frame. */
    PJ_AUDIO_FRAME_SILENCE,

    /** The frame is a non-silence audio frame. */
    PJ_AUDIO_FRAME_AUDIO,

} pj_audio_frame_type;

typedef struct pj_codec pj_codec;
typedef struct pj_codec_factory pj_codec_factory;


/** This structure describes an audio frame. */
struct pj_audio_frame
{
    /** Type: silence or non-silence. */
    pj_audio_frame_type type;

    /** Pointer to buffer. */
    void	*buf;

    /** Frame size in bytes. */
    unsigned	 size;
};

/**
 * Operations that must be supported by the codec.
 */
typedef struct pj_codec_op
{
    /** Get default attributes. */
    pj_status_t (*default_attr) (pj_codec *codec, pj_codec_attr *attr);

    /** Open and initialize codec using the specified attribute.
     *  @return zero on success.
     */
    pj_status_t	(*init)( pj_codec *codec, pj_pool_t *pool );

    /** Close and shutdown codec.
     */
    pj_status_t	(*open)( pj_codec *codec, pj_codec_attr *attr );

    /** Close and shutdown codec.
     */
    pj_status_t (*close)( pj_codec *codec );

    /** Encode frame.
     */
    pj_status_t (*encode)( pj_codec *codec, const struct pj_audio_frame *input,
			   unsigned output_buf_len, struct pj_audio_frame *output);

    /** Decode frame.
     */
    pj_status_t (*decode)( pj_codec *codec, const struct pj_audio_frame *input,
			   unsigned output_buf_len, struct pj_audio_frame *output);

} pj_codec_op;

/**
 * A codec describes an instance to encode or decode media frames. 
 */
struct pj_codec
{
    /** Entries to put this codec instance in codec factory's list. */
    PJ_DECL_LIST_MEMBER(struct pj_codec)

    /** Codec's private data. */
    void	*codec_data;

    /** Codec factory where this codec was allocated. */
    pj_codec_factory *factory;

    /** Operations to codec. */
    pj_codec_op	*op;
};

/**
 * This structure describes operations that must be supported by codec factories.
 */
typedef struct pj_codec_factory_op
{
    /** Check whether the factory can create codec with the specified ID.
     *  @param factory The codec factory.
     *  @param id  The codec ID.
     *  @return zero it matches.
     */
    pj_status_t	(*match_id)( pj_codec_factory *factory, const pj_codec_id *id );

    /** Create default attributes for the specified codec ID. This function can
     *  be called by application to get the capability of the codec.
     *  @param factory The codec factory.
     *  @param id  The codec ID.
     *  @param attr The attribute to be initialized.
     *  @return zero if success.
     */
    pj_status_t (*default_attr)( pj_codec_factory *factory, const pj_codec_id *id,
				 pj_codec_attr *attr );

    /** Enumerate supported codecs.
     *  @param factory The codec factory.
     *  @param count Number of entries in the array.
     *  @param codecs The codec array.
     *  @return the total number of supported codecs, which can be less or 
     *          greater than requested.
     */
    unsigned (*enum_codecs) (pj_codec_factory *factory, unsigned count, pj_codec_id codecs[]);

    /** This function is called by codec manager to instantiate one codec
     *  instance.
     *  @param factory The codec factory.
     *  @param id  The codec ID.
     *  @return the instance of the codec, or NULL if codec can not be created.
     */
    pj_codec* (*alloc_codec)( pj_codec_factory *factory, const pj_codec_id *id);

    /** This function is called by codec manager to return a particular instance
     *  of codec back to the codec factory.
     *  @param factory The codec factory.
     *  @param codec The codec instance to be returned.
     */
    void (*dealloc_codec)( pj_codec_factory *factory, pj_codec *codec );

} pj_codec_factory_op;

/**
 * Codec factory describes a module that is able to create codec with specific
 * capabilities. These capabilities can be queried by codec manager to create
 * instances of codec.
 */
struct pj_codec_factory
{
    /** Entries to put this structure in the codec manager list. */
    PJ_DECL_LIST_MEMBER(struct pj_codec_factory)

    /** The factory's private data. */
    void		*factory_data;

    /** Operations to the factory. */
    pj_codec_factory_op *op;

};

/**
 * Declare maximum codecs
 */
#define PJ_CODEC_MGR_MAX_CODECS	    32

/**
 * Codec manager maintains codec factory etc.
 */
typedef struct pj_codec_mgr
{
    pj_codec_factory factory_list;
    unsigned	     codec_cnt;
    pj_codec_id	     codecs[PJ_CODEC_MGR_MAX_CODECS];
} pj_codec_mgr;

/**
 * Init codec manager.
 */
PJ_DECL(pj_status_t) 
pj_codec_mgr_init (pj_codec_mgr *mgr);

/** 
 * Register codec to codec manager. 
 */
PJ_DECL(pj_status_t) 
pj_codec_mgr_register_factory (pj_codec_mgr *mgr, pj_codec_factory *factory);

/**
 * Unregister codec.
 */
PJ_DECL(void) 
pj_codec_mgr_unregister_factory (pj_codec_mgr *mgr, pj_codec_factory *factory);

/**
 * Enumerate codecs.
 */
PJ_DECL(unsigned)
pj_codec_mgr_enum_codecs (pj_codec_mgr *mgr, unsigned count, const pj_codec_id *codecs[]);

/**
 * Open codec.
 */
PJ_DECL(pj_codec*) 
pj_codec_mgr_alloc_codec (pj_codec_mgr *mgr, const struct pj_codec_id *id);

/**
 * Close codec.
 */
PJ_DECL(void) 
pj_codec_mgr_dealloc_codec (pj_codec_mgr *mgr, pj_codec *codec);

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_CODEC_H__ */
