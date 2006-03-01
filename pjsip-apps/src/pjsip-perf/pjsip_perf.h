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
#ifndef __PJSIP_PERF_H__
#define __PJSIP_PERF_H__

#include <pjsua-lib/pjsua.h>


PJ_BEGIN_DECL


typedef struct batch batch;
typedef struct session session;

/**
 * A test batch.
 */
struct batch
{
    PJ_DECL_LIST_MEMBER(struct batch);

    unsigned	 rate;		/**< How many tasks to perform		    */

    unsigned	 started;	/**< # of tasks started.		    */
    unsigned	 success;	/**< # of tasks completed successfully.	    */
    unsigned	 failed;	/**< # of failed tasks.			    */

    pj_time_val	 start_time;	/**< Start time of the tests.		    */
    pj_time_val	 spawned_time;	/**< Time when all tasks has been started.  */
    pj_time_val	 end_time;	/**< Time when all tasks has completed.	    */
};

/**
 * Test session.
 */
struct session
{
    pj_pool_t	    *pool;
    pj_time_val	     start_time;
    pj_bool_t	     stopping;
    pjsip_method     method;
    struct batch     active_list;
    struct batch     free_list;

    unsigned	     outstanding;
    unsigned	     total_created;
};


/**
 * Request parameter.
 */
struct request_param
{
    pj_str_t	    dst;
    pj_str_t	    src;
    pjsip_cred_info cred;
};


typedef struct request_param request_param;


void app_perror(const char *sender, const char *title, pj_status_t status);

/* OPTIONS test */
pj_status_t options_handler_init(void);
pj_status_t options_spawn_test(const pj_str_t *target,
			       const pj_str_t *from,
			       const pj_str_t *to,
			       unsigned cred_cnt,
			       const pjsip_cred_info cred[],
			       const pjsip_route_hdr *route_set,
			       void *test_data,
			       void (*completion_cb)(void*,pj_bool_t));

/* CALL test */
pj_status_t call_handler_init(void);
pj_status_t call_spawn_test( const pj_str_t *target,
			     const pj_str_t *from,
			     const pj_str_t *to,
			     unsigned cred_cnt,
			     const pjsip_cred_info cred[],
			     const pjsip_route_hdr *route_set,
			     void *test_data,
			     void (*completion_cb)(void*,pj_bool_t));



/**
 * Global settings
 */
struct pjsip_perf_settings
{
    /* Global */
    pj_caching_pool  cp;
    pj_pool_t	    *pool;
    pjsip_endpoint  *endpt;
    pj_mutex_t	    *mutex;

    /* Misc: */
    int		     log_level;
    int		     app_log_level;
    char	    *log_file;

    /* Network: */
    int		     local_port;

    /* Threads. */
    pj_bool_t	     quit_flag;
    int		     thread_cnt;
    pj_thread_t	    *thread[16];

    /* Outgoing request method: */
    pjsip_method     method;

    /* Default target: */
    pj_str_t	     target;

    /* Media: */
    pjmedia_endpt   *med_endpt;
    pjmedia_conf    *mconf;

    /* Handling incoming requests: */
    pj_bool_t	     stateless;

    /* Rate control.	*/
    pj_uint32_t	     start_rate;
    pj_uint32_t	     cur_rate;
    
    /* Capacity control. */
    pj_uint32_t	     max_capacity;

    /* Duration control: */
    pj_uint32_t	     duration;

    /* Test control: */
    session	    *session;
    pj_timer_entry   timer;

    /* Counters: */
    pj_uint32_t	     tx_req;
    pj_uint32_t	     tx_res;
    pj_uint32_t	     rx_req;
    pj_uint32_t	     rx_res;
};


typedef struct pjsip_perf_settings pjsip_perf_settings;

extern pjsip_perf_settings settings;



PJ_END_DECL


#endif	/* __PJSIP_PERF_H__ */




