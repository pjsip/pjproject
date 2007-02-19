/* $Id$ */
/* 
 * Copyright (C)2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_CODEC_TYPES_H__
#define __PJMEDIA_CODEC_TYPES_H__

#include <pjmedia-codec/config.h>


/**
 * These are the dynamic payload types that are used by codecs in
 * this library. Also see the header file <pjmedia/codec.h> for list
 * of static payload types.
 */
enum
{
    /* PJMEDIA_RTP_PT_TELEPHONE_EVENTS is declared in
     * <pjmedia/config.h>
     */
    PJMEDIA_RTP_PT_START = PJMEDIA_RTP_PT_TELEPHONE_EVENTS,

    PJMEDIA_RTP_PT_SPEEX_NB,			/**< Speex narrowband/8KHz  */
    PJMEDIA_RTP_PT_SPEEX_WB,			/**< Speex wideband/16KHz   */
    PJMEDIA_RTP_PT_SPEEX_UWB,			/**< Speex 32KHz	    */
    PJMEDIA_RTP_PT_L16_8KHZ_MONO,		/**< L16 @ 8KHz, mono	    */
    PJMEDIA_RTP_PT_L16_8KHZ_STEREO,		/**< L16 @ 8KHz, stereo     */
    PJMEDIA_RTP_PT_L16_11KHZ_MONO,		/**< L16 @ 11KHz, mono	    */
    PJMEDIA_RTP_PT_L16_11KHZ_STEREO,		/**< L16 @ 11KHz, stereo    */
    PJMEDIA_RTP_PT_L16_16KHZ_MONO,		/**< L16 @ 16KHz, mono	    */
    PJMEDIA_RTP_PT_L16_16KHZ_STEREO,		/**< L16 @ 16KHz, stereo    */
    PJMEDIA_RTP_PT_L16_22KHZ_MONO,		/**< L16 @ 22KHz, mono	    */
    PJMEDIA_RTP_PT_L16_22KHZ_STEREO,		/**< L16 @ 22KHz, stereo    */
    PJMEDIA_RTP_PT_L16_32KHZ_MONO,		/**< L16 @ 32KHz, mono	    */
    PJMEDIA_RTP_PT_L16_32KHZ_STEREO,		/**< L16 @ 32KHz, stereo    */
    PJMEDIA_RTP_PT_L16_48KHZ_MONO,		/**< L16 @ 48KHz, mono	    */
    PJMEDIA_RTP_PT_L16_48KHZ_STEREO,		/**< L16 @ 48KHz, stereo    */
    PJMEDIA_RTP_PT_ILBC				/**< iLBC (13.3/15.2Kbps)   */
};



#endif	/* __PJMEDIA_CODEC_TYPES_H__ */
