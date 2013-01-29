/* $Id$ */
/*
 * Copyright (C) 2011-2013 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2011 Dan Arrhenius <dan@keystream.se>
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
#ifndef __PJMEDIA_CODEC_OPENCORE_AMR_H__
#define __PJMEDIA_CODEC_OPENCORE_AMR_H__

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_OC_AMR OpenCORE AMR Codec
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief AMRCodec wrapper for OpenCORE AMR codec
 * @{
 */

PJ_BEGIN_DECL

/**
 * Bitmask options to be passed during AMR codec factory initialization.
 */
enum pjmedia_amr_options
{
    PJMEDIA_AMR_NO_NB	    = 1,    /**< Disable narrowband mode.	*/
    PJMEDIA_AMR_NO_WB	    = 2,    /**< Disable wideband mode.		*/
};

/**
 * Settings. Use #pjmedia_codec_opencore_amrnb/wb_set_config() to
 * activate.
 */
typedef struct pjmedia_codec_amr_config
{
    /**
     * Control whether to use octent align.
     */
    pj_bool_t octet_align;

    /**
     * Set the bitrate.
     */
    unsigned bitrate;

} pjmedia_codec_amr_config;

typedef pjmedia_codec_amr_config pjmedia_codec_amrnb_config;
typedef pjmedia_codec_amr_config pjmedia_codec_amrwb_config;

/**
 * Initialize and register AMR codec factory to pjmedia endpoint.
 *
 * @param endpt     The pjmedia endpoint.
 * @param options   Bitmask of pjmedia_amr_options (default=0).
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opencore_amr_init(pjmedia_endpt* endpt,
                                                     unsigned options);

/**
 * Initialize and register AMR codec factory using default settings to
 * pjmedia endpoint.
 *
 * @param endpt The pjmedia endpoint.
 *
 * @return	PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_codec_opencore_amr_init_default(pjmedia_endpt* endpt);

/**
 * Unregister AMR codec factory from pjmedia endpoint and deinitialize
 * the OpenCORE codec library.
 *
 * @return	PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opencore_amr_deinit(void);

/**
 * Initialize and register AMR-NB codec factory to pjmedia endpoint. Calling
 * this function will automatically initialize AMR codec factory without
 * the wideband mode (i.e. it is equivalent to calling
 * #pjmedia_codec_opencore_amr_init() with PJMEDIA_AMR_NO_WB). Application
 * should call #pjmedia_codec_opencore_amr_init() instead if wishing to use
 * both modes.
 *
 * @param endpt	The pjmedia endpoint.
 *
 * @return	PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opencore_amrnb_init(pjmedia_endpt* endpt);

/**
 * Unregister AMR-NB codec factory from pjmedia endpoint and deinitialize
 * the OpenCORE codec library.
 *
 * @return	PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opencore_amrnb_deinit(void);


/**
 * Set AMR-NB parameters.
 *
 * @param cfg	The settings;
 *
 * @return	PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opencore_amrnb_set_config(
				const pjmedia_codec_amrnb_config* cfg);


/**
 * Set AMR-WB parameters.
 *
 * @param cfg	The settings;
 *
 * @return	PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opencore_amrwb_set_config(
                                        const pjmedia_codec_amrwb_config* cfg);

PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODEC_OPENCORE_AMRNB_H__ */

