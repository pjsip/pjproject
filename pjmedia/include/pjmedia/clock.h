/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_CLOCK_H__
#define __PJMEDIA_CLOCK_H__

/**
 * @file clock.h
 * @brief Media clock.
 */
#include <pjmedia/types.h>


/**
 * @addtogroup PJMEDIA_CLOCK Clock Generator
 * @ingroup PJMEDIA_PORT_CLOCK
 * @brief Interface for generating clock.
 * @{
 * 
 * The clock generator provides the application with media timing,
 * and it is used by the @ref PJMEDIA_MASTER_PORT for its sound clock.
 *
 * The clock generator may be configured to run <b>asynchronously</b> 
 * (the default behavior) or <b>synchronously</b>. When it is run 
 * asynchronously, it will call the application's callback every time
 * the clock <b>tick</b> expires. When it is run synchronously, 
 * application must continuously polls the clock generator to synchronize
 * the timing.
 */

PJ_BEGIN_DECL


/**
 * Opaque declaration for media clock.
 */
typedef struct pjmedia_clock pjmedia_clock;


/**
 * Options when creating the clock.
 */
enum pjmedia_clock_options
{
    /**
     * Prevents the clock from running asynchronously. In this case,
     * application must poll the clock continuously by calling
     * #pjmedia_clock_wait() in order to synchronize timing.
     */
    PJMEDIA_CLOCK_NO_ASYNC  = 1,

    /**
     * Prevent the clock from setting it's thread to highest priority.
     */
    PJMEDIA_CLOCK_NO_HIGHEST_PRIO = 2
};


/**
 * Type of media clock callback.
 *
 * @param ts		    Current timestamp, in samples.
 * @param user_data	    Application data that is passed when
 *			    the clock was created.
 */
typedef void pjmedia_clock_callback(const pj_timestamp *ts,
				    void *user_data);



/**
 * Create media clock.
 *
 * @param pool		    Pool to allocate memory.
 * @param clock_rate	    Number of samples per second.
 * @param samples_per_frame Number of samples per frame. This argument
 *			    along with clock_rate, specifies the interval
 *			    of each clock run (or clock ticks).
 * @param options	    Bitmask of pjmedia_clock_options.
 * @param cb		    Callback to be called for each clock tick.
 * @param user_data	    User data, which will be passed to the callback.
 * @param p_clock	    Pointer to receive the clock instance.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) pjmedia_clock_create( pj_pool_t *pool,
					   unsigned clock_rate,
					   unsigned samples_per_frame,
					   unsigned options,
					   pjmedia_clock_callback *cb,
					   void *user_data,
					   pjmedia_clock **p_clock);

/**
 * Start the clock. For clock created with asynchronous flag set to TRUE,
 * this may start a worker thread for the clock (depending on the 
 * backend clock implementation being used).
 *
 * @param clock		    The media clock.
 *
 * @return		    PJ_SUCCES on success.
 */
PJ_DECL(pj_status_t) pjmedia_clock_start(pjmedia_clock *clock);


/**
 * Stop the clock.
 *
 * @param clock		    The media clock.
 *
 * @return		    PJ_SUCCES on success.
 */
PJ_DECL(pj_status_t) pjmedia_clock_stop(pjmedia_clock *clock);



/**
 * Poll the media clock, and execute the callback when the clock tick has
 * elapsed. This operation is only valid if the clock is created with async
 * flag set to FALSE.
 *
 * @param clock		    The media clock.
 * @param wait		    If non-zero, then the function will block until
 *			    a clock tick elapsed and callback has been called.
 * @param ts		    Optional argument to receive the current 
 *			    timestamp.
 *
 * @return		    Non-zero if clock tick has elapsed, or FALSE if
 *			    the function returns before a clock tick has
 *			    elapsed.
 */
PJ_DECL(pj_bool_t) pjmedia_clock_wait(pjmedia_clock *clock,
				      pj_bool_t wait,
				      pj_timestamp *ts);


/**
 * Destroy the clock.
 *
 * @param clock		    The media clock.
 *
 * @return		    PJ_SUCCES on success.
 */
PJ_DECL(pj_status_t) pjmedia_clock_destroy(pjmedia_clock *clock);



PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_CLOCK_H__ */

