/* $Id$ */
/* 
 * Copyright (C) 2019 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_VID_CONF_H__
#define __PJMEDIA_VID_CONF_H__

/**
 * @file vid_conf.h
 * @brief Video conference bridge.
 */
#include <pjmedia/port.h>

/**
 * @addtogroup PJMEDIA_VID_CONF Video conference bridge
 * @ingroup PJMEDIA_PORT
 * @brief Video conference bridge implementation
 *  destination
 * @{
 *
 * This describes the video conference bridge implementation in PJMEDIA. The
 * conference bridge provides powerful and efficient mechanism to route the
 * video flow and combine multiple video data from multiple video sources.
 */

PJ_BEGIN_DECL


/**
 * Opaque type for video conference bridge.
 */
typedef struct pjmedia_vid_conf pjmedia_vid_conf;


/**
 * Enumeration of video conference layout mode.
 */
typedef enum pjmedia_vid_conf_layout
{
    /**
     * In mixing video from multiple sources, each source will occupy about
     * the same size in the mixing result frame at all time.
     */
    PJMEDIA_VID_CONF_LAYOUT_DEFAULT,

    /**
     * Warning: this is not implemented yet.
     *
     * In mixing video from multiple sources, one specified participant
     * (or source port) will be the focus (i.e: occupy bigger portion
     * than the others).
     */
    PJMEDIA_VID_CONF_LAYOUT_SELECTIVE_FOCUS,

    /**
     * Warning: this is not implemented yet.
     *
     * In mixing video from multiple sources, one participant will be the
     * focus at a time (i.e: occupy bigger portion than the others), and
     * after some interval the focus will be shifted to another participant,
     * so each participant will have the same focus duration.
     */
    PJMEDIA_VID_CONF_LAYOUT_INTERVAL_FOCUS,

    /**
     * Warning: this is not implemented yet.
     *
     * In mixing video from multiple sources, each participant (or source
     * port) will have specific layout configuration.
     */
    PJMEDIA_VID_CONF_LAYOUT_CUSTOM,

} pjmedia_vid_conf_layout;


/**
 * Video conference bridge settings.
 */
typedef struct pjmedia_vid_conf_setting
{
    /**
     * Maximum number of slots or media ports can be registered to the bridge.
     *
     * Default: 32
     */
    unsigned		 max_slot_cnt;

    /**
     * Frame rate the bridge will operate at. For video playback smoothness,
     * ideally the bridge frame rate should be the common multiple of the
     * frame rates of the ports. Otherwise, ports whose unaligned frame rates
     * may experience jitter. For example, if the application will work with
     * frame rates of 10, 15, and 30 fps, setting this to 30 should be okay.
     * But if it also needs to handle 20 fps, better setting this to 60.
     *
     * Default: 60 (frames per second)
     */
    unsigned		 frame_rate;

    /**
     * Layout setting, see pjmedia_vid_conf_layout.
     *
     * Default: PJMEDIA_VID_CONF_LAYOUT_DEFAULT
     */
    unsigned		 layout;

} pjmedia_vid_conf_setting;


/**
 * Video conference bridge port info.
 */
typedef struct pjmedia_vid_conf_port_info
{
    unsigned		 slot;		    /**< Slot index.		    */
    pj_str_t		 name;		    /**< Port name.		    */
    pjmedia_format	 format;	    /**< Format.		    */
    unsigned		 listener_cnt;	    /**< Number of listeners.	    */
    unsigned		*listener_slots;    /**< Array of listeners.	    */
    unsigned		 transmitter_cnt;   /**< Number of transmitter.	    */
    unsigned		*transmitter_slots; /**< Array of transmitter.	    */
} pjmedia_vid_conf_port_info;


/**
 * Initialize video conference settings with default values.
 *
 * @param opt		The settings to be initialized.
 */
PJ_DECL(void) pjmedia_vid_conf_setting_default(pjmedia_vid_conf_setting *opt);


/**
 * Create a video conference bridge.
 *
 * @param pool		The memory pool.
 * @param opt		The video conference settings.
 * @param p_vid_conf	Pointer to receive the video conference bridge.
 *
 * @return		PJ_SUCCESS on success, or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_vid_conf_create(
					pj_pool_t *pool,
					const pjmedia_vid_conf_setting *opt,
					pjmedia_vid_conf **p_vid_conf);


/**
 * Destroy video conference bridge.
 *
 * @param vid_conf	The video conference bridge.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_conf_destroy(pjmedia_vid_conf *vid_conf);


/**
 * Add a media port to the video conference bridge.
 *
 * @param vid_conf	The video conference bridge.
 * @param pool		The memory pool, the brige will create new pool
 *			based on this pool factory for this media port.
 * @param port		The media port to be added.
 * @param name		Name to be assigned to the slot. If not set, it will
 *			be set to the media port name.
 * @param opt		The option, for future use, currently this must
 *			be NULL.
 * @param p_slot	Pointer to receive the slot index of the port in
 *			the conference bridge.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error
 *			code.
 */
PJ_DECL(pj_status_t) pjmedia_vid_conf_add_port(pjmedia_vid_conf *vid_conf,
					       pj_pool_t *pool,
					       pjmedia_port *port,
					       const pj_str_t *name,
					       void *opt,
					       unsigned *p_slot);


/**
 * Remove a media port from the video conference bridge.
 *
 * @param vid_conf	The video conference bridge.
 * @param slot		The media port's slot index to be removed.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error
 *			code.
 */
PJ_DECL(pj_status_t) pjmedia_vid_conf_remove_port(pjmedia_vid_conf *vid_conf,
						  unsigned slot);


/**
 * Get number of ports currently registered in the video conference bridge.
 *
 * @param vid_conf	The video conference bridge.
 *
 * @return		Number of ports currently registered to the video
 *			conference bridge.
 */
PJ_DECL(unsigned) pjmedia_vid_conf_get_port_count(pjmedia_vid_conf *vid_conf);


/**
 * Enumerate occupied slots in the video conference bridge.
 *
 * @param conf		The video conference bridge.
 * @param slots		Array of slot to be filled in.
 * @param count		On input, specifies the maximum number of slot
 *			in the array. On return, it will be filled with
 *			the actual number of slot.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_conf_enum_ports(pjmedia_vid_conf *vid_conf,
						 unsigned slots[],
						 unsigned *count);


/**
 * Get port info.
 *
 * @param vid_conf	The video conference bridge.
 * @param slot		Slot index.
 * @param info		Pointer to receive the info.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_conf_get_port_info(
					    pjmedia_vid_conf *vid_conf,
					    unsigned slot,
					    pjmedia_vid_conf_port_info *info);


/**
 * Enable unidirectional video flow from the specified source slot to
 * the specified sink slot.
 *
 * @param conf		The video conference bridge.
 * @param src_slot	Source slot.
 * @param sink_slot	Sink slot.
 * @param opt		The option, for future use, currently this must
 *			be NULL.
 *
 * @return		PJ_SUCCES on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_conf_connect_port(
					    pjmedia_vid_conf *vid_conf,
					    unsigned src_slot,
					    unsigned sink_slot,
					    void *opt);


/**
 * Disconnect unidirectional video flow from the specified source to
 * the specified sink slot.
 *
 * @param conf		The video conference bridge.
 * @param src_slot	Source slot.
 * @param sink_slot	Sink slot.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_conf_disconnect_port(
					    pjmedia_vid_conf *vid_conf,
					    unsigned src_slot,
					    unsigned sink_slot);


PJ_END_DECL

/**
 * @}
 */

#endif /* __PJMEDIA_VID_CONF_H__ */
