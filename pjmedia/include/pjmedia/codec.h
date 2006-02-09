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

#include <pjmedia/types.h>
#include <pj/list.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJMED_CODEC Codec framework.
 * @ingroup PJMEDIA
 * @{
 */


/** 
 * Standard RTP static payload types, as defined by RFC 3551. 
 */
enum pjmedia_rtp_pt
{
    PJMEDIA_RTP_PT_PCMU = 0,	    /* audio PCMU			    */
    PJMEDIA_RTP_PT_GSM  = 3,	    /* audio GSM			    */
    PJMEDIA_RTP_PT_G723 = 4,	    /* audio G723			    */
    PJMEDIA_RTP_PT_DVI4_8K = 5,	    /* audio DVI4 8KHz			    */
    PJMEDIA_RTP_PT_DVI4_16K = 6,    /* audio DVI4 16Khz			    */
    PJMEDIA_RTP_PT_LPC = 7,	    /* audio LPC			    */
    PJMEDIA_RTP_PT_PCMA = 8,	    /* audio PCMA			    */
    PJMEDIA_RTP_PT_G722 = 9,	    /* audio G722			    */
    PJMEDIA_RTP_PT_L16_2 = 10,	    /* audio 16bit linear 44.1KHz stereo    */
    PJMEDIA_RTP_PT_L16_1 = 11,	    /* audio 16bit linear 44.1KHz mono	    */
    PJMEDIA_RTP_PT_QCELP = 12,	    /* audio QCELP			    */
    PJMEDIA_RTP_PT_CN = 13,	    /* audio Comfort Noise		    */
    PJMEDIA_RTP_PT_MPA = 14,	    /* audio MPEG1/MPEG2 elementary streams */
    PJMEDIA_RTP_PT_G728 = 15,	    /* audio G728			    */
    PJMEDIA_RTP_PT_DVI4_11K = 16,   /* audio DVI4 11.025KHz mono	    */
    PJMEDIA_RTP_PT_DVI4_22K = 17,   /* audio DVI4 22.050KHz mono	    */
    PJMEDIA_RTP_PT_G729 = 18,	    /* audio G729			    */

    PJMEDIA_RTP_PT_CELB = 25,	    /* video/comb Cell-B by Sun (RFC 2029)  */
    PJMEDIA_RTP_PT_JPEG = 26,	    /* video JPEG			    */
    PJMEDIA_RTP_PT_NV = 28,	    /* video NV  by nv program by Xerox	    */
    PJMEDIA_RTP_PT_H261 = 31,	    /* video H261			    */
    PJMEDIA_RTP_PT_MPV = 32,	    /* video MPEG1 or MPEG2 elementary	    */
    PJMEDIA_RTP_PT_MP2T = 33,	    /* video MPEG2 transport		    */
    PJMEDIA_RTP_PT_H263 = 34,	    /* video H263			    */

    PJMEDIA_RTP_PT_DYNAMIC = 96,    /* start of dynamic RTP payload	    */

};


/** 
 * Identification used to search for codec factory that supports specific 
 * codec specification. 
 */
struct pjmedia_codec_info
{
    pjmedia_type    type;	    /**< Media type.			*/
    unsigned	    pt;		    /**< Payload type (can be dynamic). */
    pj_str_t	    encoding_name;  /**< Encoding name.			*/
    unsigned	    sample_rate;    /**< Sampling rate.			*/
};


/** 
 * Detailed codec attributes used both to configure a codec and to query
 * the capability of codec factories.
 */
struct pjmedia_codec_param
{
    pj_uint32_t	sample_rate;	    /**< Sampling rate in Hz		*/
    pj_uint32_t	avg_bps;	    /**< Average bandwidth in bits/sec	*/

    pj_uint8_t	pcm_bits_per_sample;/**< Bits/sample in the PCM side	*/
    pj_uint16_t	ptime;		    /**< Packet time in miliseconds	*/

    unsigned	pt:8;		    /**< Payload type.			*/
    unsigned    vad_enabled:1;	    /**< Voice Activity Detector.	*/
    unsigned    cng_enabled:1;	    /**< Comfort Noise Generator.	*/
    unsigned    lpf_enabled:1;	    /**< Low pass filter		*/
    unsigned    hpf_enabled:1;	    /**< High pass filter		*/
    unsigned    penh_enabled:1;	    /**< Perceptual Enhancement		*/
    unsigned    concl_enabled:1;    /**< Packet loss concealment	*/
    unsigned    reserved_bit:1;	    /**< Reserved, must be NULL.	*/

};


/** 
 * Types of media frame. 
 */
enum pjmedia_frame_type
{
    PJMEDIA_FRAME_TYPE_SILENCE_AUDIO,	/**< Silence audio frame.	*/
    PJMEDIA_FRAME_TYPE_AUDIO,		/**< Normal audio frame.	*/

};

/** 
 * This structure describes a media frame. 
 */
struct pjmedia_frame
{
    pjmedia_frame_type	 type;	/**< Frame type.		    */
    void		*buf;	/**< Pointer to buffer.		    */
    pj_size_t		 size;	/**< Frame size in bytes.	    */
};

/**
 * This structure describes codec operations. Each codec MUST implement
 * all of these functions.
 */
struct pjmedia_codec_op
{
    /** 
     * Get default attributes for this codec. 
     *
     * @param codec	The codec instance.
     * @param attr	Pointer to receive default codec attributes.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t (*default_attr)(pjmedia_codec *codec, 
				pjmedia_codec_param *attr);

    /** 
     * Initialize codec using the specified attribute.
     *
     * @param codec	The codec instance.
     * @param pool	Pool to use when the codec needs to allocate
     *			some memory.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t	(*init)(pjmedia_codec *codec, 
			pj_pool_t *pool );

    /** 
     * Open the codec and initialize with the specified parameter..
     *
     * @param codec	The codec instance.
     * @param param	Codec initialization parameter.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t	(*open)(pjmedia_codec *codec, 
			pjmedia_codec_param *param );

    /** 
     * Close and shutdown codec, releasing all resources allocated by
     * this codec, if any.
     *
     * @param codec	The codec instance.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t (*close)(pjmedia_codec *codec);


    /** 
     * Instruct the codec to encode the specified input frame.
     *
     * @param codec	The codec instance.
     * @param input	The input frame.
     * @param out_size	The length of buffer in the output frame.
     * @param output	The output frame.
     *
     * @return		PJ_SUCCESS on success;
     */
    pj_status_t (*encode)(pjmedia_codec *codec, 
			  const struct pjmedia_frame *input,
			  unsigned out_size, 
			  struct pjmedia_frame *output);

    /** 
     * Instruct the codec to decode the specified input frame.
     *
     * @param codec	The codec instance.
     * @param input	The input frame.
     * @param out_size	The length of buffer in the output frame.
     * @param output	The output frame.
     *
     * @return		PJ_SUCCESS on success;
     */
    pj_status_t (*decode)(pjmedia_codec *codec, 
			  const struct pjmedia_frame *input,
			  unsigned out_size, 
			  struct pjmedia_frame *output);

};


/**
 * This structure describes a codec instance. 
 */
struct pjmedia_codec
{
    /** Entries to put this codec instance in codec factory's list. */
    PJ_DECL_LIST_MEMBER(struct pjmedia_codec);

    /** Codec's private data. */
    void	*codec_data;

    /** Codec factory where this codec was allocated. */
    pjmedia_codec_factory *factory;

    /** Operations to codec. */
    pjmedia_codec_op	*op;
};


/**
 * This structure describes operations that must be supported by codec 
 * factories.
 */
struct pjmedia_codec_factory_op
{
    /** 
     * Check whether the factory can create codec with the specified 
     * codec info.
     *
     * @param factory	The codec factory.
     * @param info	The codec info.
     *
     * @return		PJ_SUCCESS if this factory is able to create an
     *			instance of codec with the specified info.
     */
    pj_status_t	(*test_alloc)(pjmedia_codec_factory *factory, 
			      const pjmedia_codec_info *info );

    /** 
     * Create default attributes for the specified codec ID. This function
     * can be called by application to get the capability of the codec.
     *
     * @param factory	The codec factory.
     * @param info	The codec info.
     * @param attr	The attribute to be initialized.
     *
     * @return		PJ_SUCCESS if success.
     */
    pj_status_t (*default_attr)(pjmedia_codec_factory *factory, 
				const pjmedia_codec_info *info,
				pjmedia_codec_param *attr );

    /** 
     * Enumerate supported codecs that can be created using this factory.
     * 
     *  @param factory	The codec factory.
     *  @param count	On input, specifies the number of elements in
     *			the array. On output, the value will be set to
     *			the number of elements that have been initialized
     *			by this function.
     *  @param info	The codec info array, which contents will be 
     *			initialized upon return.
     *
     *  @return		PJ_SUCCESS on success.
     */
    pj_status_t (*enum_info)(pjmedia_codec_factory *factory, 
			     unsigned *count, 
			     pjmedia_codec_info codecs[]);

    /** 
     * Create one instance of the codec with the specified codec info.
     *
     * @param factory	The codec factory.
     * @param info	The codec info.
     * @param p_codec	Pointer to receive the codec instance.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t (*alloc_codec)(pjmedia_codec_factory *factory, 
			       const pjmedia_codec_info *info,
			       pjmedia_codec **p_codec);

    /** 
     * This function is called by codec manager to return a particular 
     * instance of codec back to the codec factory.
     *
     * @param factory	The codec factory.
     * @param codec	The codec instance to be returned.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t (*dealloc_codec)(pjmedia_codec_factory *factory, 
				 pjmedia_codec *codec );

};


/**
 * Codec factory describes a module that is able to create codec with specific
 * capabilities. These capabilities can be queried by codec manager to create
 * instances of codec.
 */
struct pjmedia_codec_factory
{
    /** Entries to put this structure in the codec manager list. */
    PJ_DECL_LIST_MEMBER(struct pjmedia_codec_factory);

    /** The factory's private data. */
    void		     *factory_data;

    /** Operations to the factory. */
    pjmedia_codec_factory_op *op;

};

/**
 * Declare maximum codecs
 */
#define PJMEDIA_CODEC_MGR_MAX_CODECS	    32

/**
 * Codec manager maintains codec factory etc.
 */
struct pjmedia_codec_mgr
{
    pjmedia_codec_factory   factory_list;
    unsigned		    codec_cnt;
    pjmedia_codec_info	    codecs[PJMEDIA_CODEC_MGR_MAX_CODECS];
};



/**
 * Initialize codec manager.
 *
 * @param mgr	    Codec manager instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_mgr_init(pjmedia_codec_mgr *mgr);


/** 
 * Register codec factory to codec manager. 
 *
 * @param mgr	    The codec manager.
 * @param factory   The codec factory to be registered.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_codec_mgr_register_factory( pjmedia_codec_mgr *mgr,
				    pjmedia_codec_factory *factory);

/**
 * Unregister codec factory from the codec manager.
 *
 * @param mgr	    The codec manager.
 * @param factory   The codec factory to be unregistered.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_codec_mgr_unregister_factory( pjmedia_codec_mgr *mgr, 
				      pjmedia_codec_factory *factory);

/**
 * Enumerate all supported codec.
 *
 * @param mgr	    The codec manager.
 * @param count	    On input, specifies the number of elements in
 *		    the array. On output, the value will be set to
 *		    the number of elements that have been initialized
 *		    by this function.
 * @param info	    The codec info array, which contents will be 
 *		    initialized upon return.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_mgr_enum_codecs( pjmedia_codec_mgr *mgr, 
						    unsigned *count, 
						    pjmedia_codec_info info[]);

/**
 * Request the codec manager to allocate one instance of codec with the
 * specified codec info. The codec will enumerate all codec factories
 * until it finds factory that is able to create the specified codec.
 *
 * @param mgr	    The codec manager.
 * @param info	    The information about the codec to be created.
 * @param p_codec   Pointer to receive the codec instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_mgr_alloc_codec(pjmedia_codec_mgr *mgr, 
						   const pjmedia_codec_info *info,
						   pjmedia_codec **p_codec);

/**
 * Deallocate the specified codec instance. The codec manager will return
 * the instance of the codec back to its factory.
 *
 * @param mgr	    The codec manager.
 * @param codec	    The codec instance.
 *
 * @return	    PJ_SUCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_mgr_dealloc_codec(pjmedia_codec_mgr *mgr, 
						     pjmedia_codec *codec);

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_CODEC_H__ */
