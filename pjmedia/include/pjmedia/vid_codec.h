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
#ifndef __PJMEDIA_VID_CODEC_H__
#define __PJMEDIA_VID_CODEC_H__


/**
 * @file vid_codec.h
 * @brief Video codec framework.
 */

#include <pjmedia/codec.h>
#include <pjmedia/event.h>
#include <pjmedia/format.h>
#include <pjmedia/types.h>
#include <pj/list.h>
#include <pj/pool.h>

PJ_BEGIN_DECL

#define PJMEDIA_VID_CODEC_MAX_DEC_FMT_CNT    8
#define PJMEDIA_VID_CODEC_MAX_FPS_CNT        16

/** 
 * Identification used to search for codec factory that supports specific 
 * codec specification. 
 */
typedef struct pjmedia_vid_codec_info
{
    pjmedia_format_id   fmt_id;         /**< Encoded format ID              */
    unsigned            pt;             /**< Payload type		    */
    pj_str_t	        encoding_name;  /**< Encoding name                  */
    pj_str_t	        encoding_desc;	/**< Encoding desc		    */
    unsigned            clock_rate;     /**< Clock rate			    */
    pjmedia_dir         dir;            /**< Direction                      */
    unsigned            dec_fmt_id_cnt; /**< # of supported encoding source 
                                             format IDs                     */
    pjmedia_format_id   dec_fmt_id[PJMEDIA_VID_CODEC_MAX_DEC_FMT_CNT];
                                        /**< Supported encoding source 
                                             format IDs                     */
    unsigned            fps_cnt;        /**< # of supported frame-rates, can be
					     zero (support any frame-rate)  */
    pjmedia_ratio       fps[PJMEDIA_VID_CODEC_MAX_FPS_CNT];
                                        /**< Supported frame-rates	    */
    pj_bool_t		has_rtp_pack;	/**< Support RTP packetization      */
} pjmedia_vid_codec_info;


/** 
 * Detailed codec attributes used in configuring a codec and in querying
 * the capability of codec factories. Default attributes of any codecs could
 * be queried using #pjmedia_vid_codec_mgr_get_default_param() and modified
 * using #pjmedia_vid_codec_mgr_set_default_param().
 *
 * Please note that codec parameter also contains SDP specific setting, 
 * #dec_fmtp and #enc_fmtp, which may need to be set appropriately based on
 * the effective setting. See each codec documentation for more detail.
 */
typedef struct pjmedia_vid_codec_param
{
    pjmedia_dir         dir;            /**< Direction                      */
    pjmedia_format      enc_fmt;        /**< Encoded format	            */
    pjmedia_format      dec_fmt;        /**< Decoded format	            */

    pjmedia_codec_fmtp  enc_fmtp;       /**< Encoder fmtp params	    */
    pjmedia_codec_fmtp  dec_fmtp;       /**< Decoder fmtp params	    */

    unsigned            enc_mtu;        /**< MTU or max payload size setting*/
} pjmedia_vid_codec_param;


/**
 * Duplicate video codec parameter.
 *
 * @param pool	    The pool.
 * @param src	    The video codec parameter to be duplicated.
 *
 * @return	    Duplicated codec parameter.
 */
PJ_DECL(pjmedia_vid_codec_param*) pjmedia_vid_codec_param_clone(
					pj_pool_t *pool, 
					const pjmedia_vid_codec_param *src);

/**
 * Forward declaration for video codec.
 */
typedef struct pjmedia_vid_codec pjmedia_vid_codec;


/**
 * This structure describes codec operations. Each codec MUST implement
 * all of these functions.
 */
typedef struct pjmedia_vid_codec_op
{
    /** 
     * Initialize codec using the specified attribute.
     *
     * Application should call #pjmedia_vid_codec_init() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance.
     * @param pool	Pool to use when the codec needs to allocate
     *			some memory.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t	(*init)(pjmedia_vid_codec *codec, 
			pj_pool_t *pool );

    /** 
     * Open the codec and initialize with the specified parameter.
     * Upon successful initialization, the codec may modify the parameter
     * and fills in the unspecified values (such as size or frame rate of
     * the encoder format, as it may need to be negotiated with remote
     * preferences via SDP fmtp).
     *
     * Application should call #pjmedia_vid_codec_open() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance.
     * @param param	Codec initialization parameter.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t	(*open)(pjmedia_vid_codec *codec, 
			pjmedia_vid_codec_param *param );

    /** 
     * Close and shutdown codec, releasing all resources allocated by
     * this codec, if any.
     *
     * Application should call #pjmedia_vid_codec_close() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t (*close)(pjmedia_vid_codec *codec);

    /** 
     * Modify the codec parameter after the codec is open. 
     * Note that not all codec parameters can be modified during run-time. 
     * When the parameter cannot be changed, this function will return 
     * non-PJ_SUCCESS, and the original parameters will not be changed.
     *
     * Application should call #pjmedia_vid_codec_modify() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance.
     * @param param	The new codec parameter.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t	(*modify)(pjmedia_vid_codec *codec, 
			  const pjmedia_vid_codec_param *param);

    /** 
     * Get the codec parameter after the codec is opened. 
     *
     * Application should call #pjmedia_vid_codec_get_param() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance.
     * @param param	The codec parameter.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t	(*get_param)(pjmedia_vid_codec *codec,
			     pjmedia_vid_codec_param *param);

    /**
     * Instruct the codec to generate a payload/packet from a picture
     * bitstream to be sent (via network). The maximum payload size or
     * MTU is configurable via enc_mtu field of #pjmedia_vid_codec_param.
     * For a long bitstream, application usually need to call this function
     * multiple times until the whole bitstream is sent. Note that, for
     * performance reason, the packetization will be done in-place, so the
     * original bitstream may be modified by this function.
     *
     * Application should call #pjmedia_vid_codec_packetize() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance
     * @param bits	The picture bitstream.
     * @param bits_len	The length of the bitstream.
     * @param bits_pos	On input, the start position of the bitstream
     *			to be packetized. On output, the next position for
     *			next packet.
     * @param pkt	The pointer of the generated payload.
     * @param pkt_len	The payload length.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t (*packetize) (pjmedia_vid_codec *codec,
                              pj_uint8_t *bits,
                              pj_size_t bits_len,
                              unsigned *bits_pos,
                              const pj_uint8_t **pkt,
                              pj_size_t *pkt_len);

    /**
     * Instruct the codec to parse a payload and append it into a picture
     * bitstream. A picture bitstreams may need to be reconstructed from
     * one or more payloads. Note that this function will not provide the
     * detection of picture boundary, so application should manage the
     * picture boundary detection by itself, e.g: for RTP delivery, payloads
     * belong to the same picture will share the same RTP timestamp and also
     * there is marker bit in the RTP header that is usually reserved for
     * end-of-picture flag. Also note that in case of noticing packet lost,
     * application should keep calling this function with payload pointer
     * set to NULL, as the packetizer need to update its internal state.
     *
     * Application should call #pjmedia_vid_codec_unpacketize() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance
     * @param pkt	The input packet.
     * @param pkt_size	Size of the packet.
     * @param timestamp	The timestamp of the first sample in the packet.
     * @param frame_cnt	On input, specifies the maximum number of frames
     *			in the array. On output, the codec must fill
     *			with number of frames detected in the packet.
     * @param frames	On output, specifies the frames that have been
     *			detected in the packet.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t (*unpacketize)(pjmedia_vid_codec *codec,
                               const pj_uint8_t *payload,
                               pj_size_t   payload_len,
                               pj_uint8_t *bits,
                               pj_size_t   bits_len,
			       unsigned	  *bits_pos);

    /** 
     * Instruct the codec to encode the specified input frame. The input
     * MUST contain only one picture with appropriate format as specified
     * in opening the codec.
     *
     * Application should call #pjmedia_vid_codec_encode() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance.
     * @param input	The input frame.
     * @param out_size	The length of buffer in the output frame.
     * @param output	The output frame.
     *
     * @return		PJ_SUCCESS on success;
     */
    pj_status_t (*encode)(pjmedia_vid_codec *codec, 
			  const pjmedia_frame *input,
			  unsigned out_size, 
			  pjmedia_frame *output);

    /** 
     * Instruct the codec to decode the specified input frame. The input
     * frame MUST contain exactly one picture. Note that the decoded picture
     * format may different to the current setting, e.g: the format specified
     * in the #pjmedia_vid_codec_param when opening the codec, in this case the
     * PJMEDIA_EVENT_FMT_CHANGED event will be emitted by the codec. The codec
     * parameter will also be updated, and application can query the format by
     * using #get_param().
     *
     * Application should call #pjmedia_vid_codec_decode() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance.
     * @param input	The input frame.
     * @param out_size	The length of buffer in the output frame.
     * @param output	The output frame.
     *
     * @return		PJ_SUCCESS on success;
     */
    pj_status_t (*decode)(pjmedia_vid_codec *codec, 
			  const pjmedia_frame *input,
			  unsigned out_size, 
			  pjmedia_frame *output);

    /**
     * Instruct the codec to recover a missing frame.
     *
     * Application should call #pjmedia_vid_codec_recover() instead of 
     * calling this function directly.
     *
     * @param codec	The codec instance.
     * @param out_size	The length of buffer in the output frame.
     * @param output	The output frame where generated signal
     *			will be placed.
     *
     * @return		PJ_SUCCESS on success;
     */
    pj_status_t (*recover)(pjmedia_vid_codec *codec,
			   unsigned out_size,
			   pjmedia_frame *output);

} pjmedia_vid_codec_op;



/*
 * Forward declaration for pjmedia_vid_codec_factory.
 */
typedef struct pjmedia_vid_codec_factory pjmedia_vid_codec_factory;


/**
 * This structure describes a video codec instance. Codec implementers
 * should use #pjmedia_vid_codec_init() to initialize this structure with
 * default values.
 */
struct pjmedia_vid_codec
{
    /** Entries to put this codec instance in codec factory's list. */
    PJ_DECL_LIST_MEMBER(struct pjmedia_vid_codec);

    /** Codec's private data. */
    void			*codec_data;

    /** Codec factory where this codec was allocated. */
    pjmedia_vid_codec_factory   *factory;

    /** Operations to codec. */
    pjmedia_vid_codec_op	*op;

    /** Event publisher object */
    pjmedia_event_publisher	 epub;
};



/**
 * This structure describes operations that must be supported by codec 
 * factories.
 */
typedef struct pjmedia_vid_codec_factory_op
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
    pj_status_t	(*test_alloc)(pjmedia_vid_codec_factory *factory, 
			      const pjmedia_vid_codec_info *info );

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
    pj_status_t (*default_attr)(pjmedia_vid_codec_factory *factory, 
    				const pjmedia_vid_codec_info *info,
    				pjmedia_vid_codec_param *attr );

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
    pj_status_t (*enum_info)(pjmedia_vid_codec_factory *factory, 
			     unsigned *count, 
			     pjmedia_vid_codec_info codecs[]);

    /** 
     * Create one instance of the codec with the specified codec info.
     *
     * @param factory	The codec factory.
     * @param info	The codec info.
     * @param p_codec	Pointer to receive the codec instance.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t (*alloc_codec)(pjmedia_vid_codec_factory *factory, 
			       const pjmedia_vid_codec_info *info,
			       pjmedia_vid_codec **p_codec);

    /** 
     * This function is called by codec manager to return a particular 
     * instance of codec back to the codec factory.
     *
     * @param factory	The codec factory.
     * @param codec	The codec instance to be returned.
     *
     * @return		PJ_SUCCESS on success.
     */
    pj_status_t (*dealloc_codec)(pjmedia_vid_codec_factory *factory, 
				 pjmedia_vid_codec *codec );

} pjmedia_vid_codec_factory_op;



/**
 * Codec factory describes a module that is able to create codec with specific
 * capabilities. These capabilities can be queried by codec manager to create
 * instances of codec.
 */
struct pjmedia_vid_codec_factory
{
    /** Entries to put this structure in the codec manager list. */
    PJ_DECL_LIST_MEMBER(struct pjmedia_vid_codec_factory);

    /** The factory's private data. */
    void		     *factory_data;

    /** Operations to the factory. */
    pjmedia_vid_codec_factory_op *op;

};


/**
 * Opaque declaration for codec manager.
 */
typedef struct pjmedia_vid_codec_mgr pjmedia_vid_codec_mgr;

/**
 * Declare maximum codecs
 */
#define PJMEDIA_VID_CODEC_MGR_MAX_CODECS	    32


/**
 * Initialize pjmedia_vid_codec structure with default values.
 *
 * @param codec	    The codec to be initialized.
 * @param sig	    Codec's object signature (see signatures.h)
 */
PJ_DECL(void) pjmedia_vid_codec_reset(pjmedia_vid_codec *codec,
                                      pjmedia_obj_sig sig);

/**
 * Initialize codec manager. If there is no the default video codec manager,
 * this function will automatically set the default video codec manager to
 * the new codec manager instance. Normally this function is called by pjmedia
 * endpoint's initialization code.
 *
 * @param pool	    The pool instance.
 * @param mgr	    The pointer to the new codec manager instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_codec_mgr_create(pj_pool_t *pool,
                                                  pjmedia_vid_codec_mgr **mgr);


/**
 * Destroy codec manager. Normally this function is called by pjmedia
 * endpoint's deinitialization code.
 *
 * @param mgr	    Codec manager instance.  If NULL, it is the default codec
 *		    manager instance will be destroyed.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_codec_mgr_destroy(pjmedia_vid_codec_mgr *mgr);


/**
 * Get the default codec manager instance.
 *
 * @return	    The default codec manager instance or NULL if none.
 */
PJ_DECL(pjmedia_vid_codec_mgr*) pjmedia_vid_codec_mgr_instance(void);


/**
 * Set the default codec manager instance.
 *
 * @param mgr	    The codec manager instance.
 */
PJ_DECL(void) pjmedia_vid_codec_mgr_set_instance(pjmedia_vid_codec_mgr* mgr);


/** 
 * Register codec factory to codec manager. This will also register
 * all supported codecs in the factory to the codec manager.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param factory   The codec factory to be registered.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_vid_codec_mgr_register_factory( pjmedia_vid_codec_mgr *mgr,
				        pjmedia_vid_codec_factory *factory);

/**
 * Unregister codec factory from the codec manager. This will also
 * remove all the codecs registered by the codec factory from the
 * codec manager's list of supported codecs.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param factory   The codec factory to be unregistered.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_vid_codec_mgr_unregister_factory( pjmedia_vid_codec_mgr *mgr, 
				          pjmedia_vid_codec_factory *factory);

/**
 * Enumerate all supported codecs that have been registered to the
 * codec manager by codec factories.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param count	    On input, specifies the number of elements in
 *		    the array. On output, the value will be set to
 *		    the number of elements that have been initialized
 *		    by this function.
 * @param info	    The codec info array, which contents will be 
 *		    initialized upon return.
 * @param prio	    Optional pointer to receive array of codec priorities.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_codec_mgr_enum_codecs( 
                                            pjmedia_vid_codec_mgr *mgr, 
					    unsigned *count, 
					    pjmedia_vid_codec_info info[],
					    unsigned *prio);


/**
 * Get codec info for the specified payload type. The payload type must be
 * static or locally defined in #pjmedia_video_pt.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param pt	    The payload type/number.
 * @param info	    Pointer to receive codec info.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_vid_codec_mgr_get_codec_info( pjmedia_vid_codec_mgr *mgr,
				      unsigned pt,
				      const pjmedia_vid_codec_info **info);


/**
 * Get codec info for the specified format ID.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param fmt_id    Format ID. See #pjmedia_format_id
 * @param info	    Pointer to receive codec info.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_vid_codec_mgr_get_codec_info2(pjmedia_vid_codec_mgr *mgr,
				      pjmedia_format_id fmt_id,
				      const pjmedia_vid_codec_info **info);


/**
 * Convert codec info struct into a unique codec identifier.
 * A codec identifier looks something like "H263/90000".
 *
 * @param info	    The codec info
 * @param id	    Buffer to put the codec info string.
 * @param max_len   The length of the buffer.
 *
 * @return	    The null terminated codec info string, or NULL if
 *		    the buffer is not long enough.
 */
PJ_DECL(char*) pjmedia_vid_codec_info_to_id(
                                        const pjmedia_vid_codec_info *info,
				        char *id, unsigned max_len );


/**
 * Find codecs by the unique codec identifier. This function will find
 * all codecs that match the codec identifier prefix. For example, if
 * "H26" is specified, then it will find "H263/90000", "H264/90000",
 * and so on, up to the maximum count specified in the argument.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param codec_id  The full codec ID or codec ID prefix. If an empty
 *		    string is given, it will match all codecs.
 * @param count	    Maximum number of codecs to find. On return, it
 *		    contains the actual number of codecs found.
 * @param p_info    Array of pointer to codec info to be filled. This
 *		    argument may be NULL, which in this case, only
 *		    codec count will be returned.
 * @param prio	    Optional array of codec priorities.
 *
 * @return	    PJ_SUCCESS if at least one codec info is found.
 */
PJ_DECL(pj_status_t) 
pjmedia_vid_codec_mgr_find_codecs_by_id(pjmedia_vid_codec_mgr *mgr,
					const pj_str_t *codec_id,
					unsigned *count,
					const pjmedia_vid_codec_info *p_info[],
					unsigned prio[]);


/**
 * Set codec priority. The codec priority determines the order of
 * the codec in the SDP created by the endpoint. If more than one codecs
 * are found with the same codec_id prefix, then the function sets the
 * priorities of all those codecs.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param codec_id  The full codec ID or codec ID prefix. If an empty
 *		    string is given, it will match all codecs.
 * @param prio	    Priority to be set. The priority can have any value
 *		    between 1 to 255. When the priority is set to zero,
 *		    the codec will be disabled.
 *
 * @return	    PJ_SUCCESS if at least one codec info is found.
 */
PJ_DECL(pj_status_t)
pjmedia_vid_codec_mgr_set_codec_priority(pjmedia_vid_codec_mgr *mgr, 
					 const pj_str_t *codec_id,
					 pj_uint8_t prio);


/**
 * Get default codec param for the specified codec info.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param info	    The codec info, which default parameter's is being
 *		    queried.
 * @param param	    On return, will be filled with the default codec
 *		    parameter.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_vid_codec_mgr_get_default_param(pjmedia_vid_codec_mgr *mgr,
					const pjmedia_vid_codec_info *info,
					pjmedia_vid_codec_param *param);


/**
 * Set default codec param for the specified codec info.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param pool	    The pool instance.
 * @param info	    The codec info, which default parameter's is being
 *		    updated.
 * @param param	    The new default codec parameter. Set to NULL to reset
 *		    codec parameter to library default settings.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_vid_codec_mgr_set_default_param(pjmedia_vid_codec_mgr *mgr,
                                        pj_pool_t *pool,
				        const pjmedia_vid_codec_info *info,
				        const pjmedia_vid_codec_param *param);


/**
 * Request the codec manager to allocate one instance of codec with the
 * specified codec info. The codec will enumerate all codec factories
 * until it finds factory that is able to create the specified codec.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param info	    The information about the codec to be created.
 * @param p_codec   Pointer to receive the codec instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_vid_codec_mgr_alloc_codec( pjmedia_vid_codec_mgr *mgr, 
			           const pjmedia_vid_codec_info *info,
			           pjmedia_vid_codec **p_codec);

/**
 * Deallocate the specified codec instance. The codec manager will return
 * the instance of the codec back to its factory.
 *
 * @param mgr	    The codec manager instance. If NULL, the default codec
 *		    manager instance will be used.
 * @param codec	    The codec instance.
 *
 * @return	    PJ_SUCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_codec_mgr_dealloc_codec(
                                                pjmedia_vid_codec_mgr *mgr, 
						pjmedia_vid_codec *codec);



/** 
 * Initialize codec using the specified attribute.
 *
 * @param codec	    The codec instance.
 * @param pool	    Pool to use when the codec needs to allocate
 *		    some memory.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_init( pjmedia_vid_codec *codec, 
					       pj_pool_t *pool )
{
    return (*codec->op->init)(codec, pool);
}


/** 
 * Open the codec and initialize with the specified parameter.
 * Upon successful initialization, the codec may modify the parameter
 * and fills in the unspecified values (such as size or frame rate of
 * the encoder format, as it may need to be negotiated with remote
 * preferences via SDP fmtp).
 *
 * @param codec	    The codec instance.
 * @param param	    Codec initialization parameter.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_open(
					    pjmedia_vid_codec *codec, 
					    pjmedia_vid_codec_param *param )
{
    return (*codec->op->open)(codec, param);
}


/** 
 * Close and shutdown codec, releasing all resources allocated by
 * this codec, if any.
 *
 * @param codec	    The codec instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_close( pjmedia_vid_codec *codec )
{
    return (*codec->op->close)(codec);
}


/** 
 * Modify the codec parameter after the codec is open. 
 * Note that not all codec parameters can be modified during run-time. 
 * When the parameter cannot be changed, this function will return 
 * non-PJ_SUCCESS, and the original parameters will not be changed.
 *
 * @param codec	The codec instance.
 * @param param	The new codec parameter.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_modify(
				    pjmedia_vid_codec *codec, 
				    const pjmedia_vid_codec_param *param)
{
    return (*codec->op->modify)(codec, param);
}


/** 
 * Get the codec parameter after the codec is opened. 
 *
 * @param codec	The codec instance.
 * @param param	The codec parameter.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_get_param(
				    pjmedia_vid_codec *codec, 
				    pjmedia_vid_codec_param *param)
{
    return (*codec->op->get_param)(codec, param);
}


/**
 * Instruct the codec to generate a payload/packet from a picture
 * bitstream to be sent (via network). The maximum payload size or
 * MTU is configurable via enc_mtu field of #pjmedia_vid_codec_param.
 * For a long bitstream, application usually need to call this function
 * multiple times until the whole bitstream is sent. Note that, for
 * performance reason, the packetization will be done in-place, so the
 * original bitstream may be modified by this function.
 *
 * @param codec	The codec instance
 * @param bits	The picture bitstream.
 * @param bits_len	The length of the bitstream.
 * @param bits_pos	On input, the start position of the bitstream
 *			to be packetized. On output, the next position for
 *			next packet.
 * @param pkt	The pointer of the generated payload.
 * @param pkt_len	The payload length.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_packetize(
					    pjmedia_vid_codec *codec,
					    pj_uint8_t *bits,
					    pj_size_t bits_len,
					    unsigned *bits_pos,
					    const pj_uint8_t **pkt,
					    pj_size_t *pkt_len )
{
    return (*codec->op->packetize)(codec, bits, bits_len, bits_pos,
				   pkt, pkt_len);
}


/**
 * Instruct the codec to parse a payload and append it into a picture
 * bitstream. A picture bitstreams may need to be reconstructed from
 * one or more payloads. Note that this function will not provide the
 * detection of picture boundary, so application should manage the
 * picture boundary detection by itself, e.g: for RTP delivery, payloads
 * belong to the same picture will share the same RTP timestamp and also
 * there is marker bit in the RTP header that is usually reserved for
 * end-of-picture flag. Also note that in case of noticing packet lost,
 * application should keep calling this function with payload pointer
 * set to NULL, as the packetizer need to update its internal state.
 *
 * @param codec	The codec instance
 * @param pkt	The input packet.
 * @param pkt_size	Size of the packet.
 * @param timestamp	The timestamp of the first sample in the packet.
 * @param frame_cnt	On input, specifies the maximum number of frames
 *			in the array. On output, the codec must fill
 *			with number of frames detected in the packet.
 * @param frames	On output, specifies the frames that have been
 *			detected in the packet.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_unpacketize(
						pjmedia_vid_codec *codec,
						const pj_uint8_t *payload,
						pj_size_t payload_len,
						pj_uint8_t *bits,
						pj_size_t bits_len,
						unsigned *bits_pos )
{
    return (*codec->op->unpacketize)(codec, payload, payload_len, bits,
				     bits_len, bits_pos);
}


/** 
 * Instruct the codec to encode the specified input frame. The input
 * MUST contain only one picture with appropriate format as specified
 * in opening the codec.
 *
 * @param codec	The codec instance.
 * @param input	The input frame.
 * @param out_size	The length of buffer in the output frame.
 * @param output	The output frame.
 *
 * @return		PJ_SUCCESS on success;
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_encode(
					    pjmedia_vid_codec *codec, 
					    const pjmedia_frame *input,
					    unsigned out_size, 
					    pjmedia_frame *output)
{
    return (*codec->op->encode)(codec, input, out_size, output);
}


/** 
 * Instruct the codec to decode the specified input frame. The input
 * frame MUST contain exactly one picture. Note that the decoded picture
 * format may different to the current setting, e.g: the format specified
 * in the #pjmedia_vid_codec_param when opening the codec, in this case the
 * PJMEDIA_EVENT_FMT_CHANGED event will be emitted by the codec. The codec
 * parameter will also be updated, and application can query the format by
 * using #get_param().
 *
 * @param codec	The codec instance.
 * @param input	The input frame.
 * @param out_size	The length of buffer in the output frame.
 * @param output	The output frame.
 *
 * @return		PJ_SUCCESS on success;
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_decode(
					    pjmedia_vid_codec *codec, 
					    const pjmedia_frame *input,
					    unsigned out_size, 
					    pjmedia_frame *output)
{
    return (*codec->op->decode)(codec, input, out_size, output);
}


/**
 * Instruct the codec to recover a missing frame.
 *
 * @param codec	The codec instance.
 * @param out_size	The length of buffer in the output frame.
 * @param output	The output frame where generated signal
 *			will be placed.
 *
 * @return		PJ_SUCCESS on success;
 */
PJ_INLINE(pj_status_t) pjmedia_vid_codec_recover(
					    pjmedia_vid_codec *codec, 
					    unsigned out_size, 
					    pjmedia_frame *output)
{
    if (codec->op && codec->op->recover)
	return (*codec->op->recover)(codec, out_size, output);
    else
	return PJ_ENOTSUP;
}


/**
 * @}
 */

/**
 * @defgroup PJMEDIA_CODEC_VID_CODECS Supported video codecs
 * @ingroup PJMEDIA_CODEC
 * @brief Documentation about individual video codec supported by PJMEDIA
 * @{
 * Please see the APIs provided by the individual codecs below.
 */
/**
 * @}
 */




PJ_END_DECL


#endif	/* __PJMEDIA_VID_CODEC_H__ */
