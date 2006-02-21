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
#ifndef __PJMEDIA_PORT_H__
#define __PJMEDIA_PORT_H__

/**
 * @file port.h
 * @brief Port interface declaration
 */
#include <pjmedia/types.h>
#include <pj/os.h>


PJ_BEGIN_DECL


/**
 * Port operation setting.
 */
enum pjmedia_port_op
{
    /** 
     * No change to the port TX or RX settings.
     */
    PJMEDIA_PORT_NO_CHANGE,

    /**
     * TX or RX is disabled from the port. It means get_frame() or
     * put_frame() WILL NOT be called for this port.
     */
    PJMEDIA_PORT_DISABLE,

    /**
     * TX or RX is muted, which means that get_frame() or put_frame()
     * will still be called, but the audio frame is discarded.
     */
    PJMEDIA_PORT_MUTE,

    /**
     * Enable TX and RX to/from this port.
     */
    PJMEDIA_PORT_ENABLE,
};


/**
 * @see pjmedia_port_op
 */
typedef enum pjmedia_port_op pjmedia_port_op;


/**
 * Port info.
 */
struct pjmedia_port_info
{
    pj_str_t	    name;		/**< Port name.			    */
    pj_uint32_t	    signature;		/**< Port signature.		    */
    pjmedia_type    type;		/**< Media type.		    */
    pj_bool_t	    has_info;		/**< Has info?			    */
    pj_bool_t	    need_info;		/**< Need info on connect?	    */
    unsigned	    pt;			/**< Payload type (can be dynamic). */
    pj_str_t	    encoding_name;	/**< Encoding name.		    */
    unsigned	    sample_rate;	/**< Sampling rate.		    */
    unsigned	    bits_per_sample;	/**< Bits/sample		    */
    unsigned	    samples_per_frame;	/**< No of samples per frame.	    */
    unsigned	    bytes_per_frame;	/**< No of samples per frame.	    */
};

/**
 * @see pjmedia_port_info
 */
typedef struct pjmedia_port_info pjmedia_port_info;


/** 
 * Types of media frame. 
 */
enum pjmedia_frame_type
{
    PJMEDIA_FRAME_TYPE_NONE,	    /**< No frame.		*/
    PJMEDIA_FRAME_TYPE_CNG,	    /**< Silence audio frame.	*/
    PJMEDIA_FRAME_TYPE_AUDIO,	    /**< Normal audio frame.	*/

};

/** 
 * This structure describes a media frame. 
 */
struct pjmedia_frame
{
    pjmedia_frame_type	 type;	    /**< Frame type.		    */
    void		*buf;	    /**< Pointer to buffer.	    */
    pj_size_t		 size;	    /**< Frame size in bytes.	    */
    pj_timestamp	 timestamp; /**< Frame timestamp.	    */
};

/**
 * For future graph.
 */
typedef struct pjmedia_graph pjmedia_graph;


/**
 * @see pjmedia_port
 */
typedef struct pjmedia_port pjmedia_port;

/**
 * Port interface.
 */
struct pjmedia_port
{
    pjmedia_port_info	 info;
    pjmedia_graph	*graph;
    pjmedia_port	*upstream_port;
    pjmedia_port	*downstream_port;
    void		*user_data;

    /**
     * Called when this port is connected to an upstream port.
     */
    pj_status_t (*on_upstream_connect)(pj_pool_t *pool,
				       pjmedia_port *this_port,
				       pjmedia_port *upstream);

    /**
     * Called when this port is connected to a downstream port.
     */
    pj_status_t (*on_downstream_connect)(pj_pool_t *pool,
					 pjmedia_port *this_port,
				         pjmedia_port *upstream);

    /**
     * Sink interface. 
     * This should only be called by #pjmedia_port_put_frame().
     */
    pj_status_t (*put_frame)(pjmedia_port *this_port, 
			     const pjmedia_frame *frame);

    /**
     * Source interface. 
     * This should only be called by #pjmedia_port_get_frame().
     */
    pj_status_t (*get_frame)(pjmedia_port *this_port, 
			     pjmedia_frame *frame);

    /**
     * Called to destroy this port.
     */
    pj_status_t (*on_destroy)(pjmedia_port *this_port);
};



/**
 * Connect two ports.
 */
PJ_DECL(pj_status_t) pjmedia_port_connect( pj_pool_t *pool,
					   pjmedia_port *upstream_port,
					   pjmedia_port *downstream_port);

/**
 * Disconnect ports.
 */
PJ_DECL(pj_status_t) pjmedia_port_disconnect( pjmedia_port *upstream_port,
					      pjmedia_port *downstream_port);


/**
 * Get a frame from the port (and subsequent downstream ports).
 */
PJ_DECL(pj_status_t) pjmedia_port_get_frame( pjmedia_port *port,
					     pjmedia_frame *frame );

/**
 * Put a frame to the port (and subsequent downstream ports).
 */
PJ_DECL(pj_status_t) pjmedia_port_put_frame( pjmedia_port *port,
					     const pjmedia_frame *frame );


/**
 * Destroy port (and subsequent downstream ports)
 */
PJ_DECL(pj_status_t) pjmedia_port_destroy( pjmedia_port *port );



PJ_END_DECL


#endif	/* __PJMEDIA_PORT_H__ */

