/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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

#include "pjsua_common.h"
#include <pjlib-util/cli.h>
#include <pjlib-util/cli_imp.h>
#include <pjlib-util/cli_console.h>
#include <pjlib-util/cli_telnet.h>
#include <pjlib-util/scanner.h>

#define THIS_FILE	"pjsua_cli.c"

pj_cli_telnet_on_started   cli_telnet_on_started_cb = NULL;
pj_cli_on_quit	   	   cli_on_quit_cb = NULL;
pj_cli_on_destroy	   cli_on_destroy_cb = NULL;
pj_cli_on_restart_pjsua	   cli_on_restart_pjsua_cb = NULL;

pj_bool_t		   pjsua_restarted = PJ_TRUE;
static pj_bool_t	   pj_inited = PJ_FALSE;
static pj_caching_pool	   cli_cp;
static pj_cli_t		   *cli = NULL;
static pj_cli_sess	   *cli_cons_sess = NULL;

/** Forward declaration **/
pj_status_t setup_command(pj_cli_t *cli);

static void log_writer(int level, const char *buffer, int len)
{
    if (cli)
	pj_cli_write_log(cli, level, buffer, len);

    if (app_config.disable_cli_console)
	pj_log_write(level, buffer, len);
}

void destroy_cli(pj_bool_t app_restart)
{    
    pj_log_set_log_func(&pj_log_write);
    
    if (cli) {		
	pj_cli_destroy(cli);
	cli = NULL;
    }

    if (cli_cp.factory.create_pool) {
	pj_caching_pool_destroy(&cli_cp);
	pj_bzero(&cli_cp, sizeof(cli_cp));
    }

    if (pj_inited) {
	pj_shutdown();
	pj_inited = PJ_FALSE;
    }
    if (!app_restart) {
	if (cli_on_destroy_cb)
	    (*cli_on_destroy_cb)();
    }
}

pj_status_t setup_cli(pj_bool_t with_console, pj_bool_t with_telnet,
		      pj_uint16_t telnet_port, 
		      pj_cli_telnet_on_started on_started_cb,
		      pj_cli_on_quit on_quit_cb,
		      pj_cli_on_destroy on_destroy_cb,
		      pj_cli_on_restart_pjsua on_restart_pjsua_cb)
{
    pj_cli_cfg cli_cfg;
    pj_status_t status;

    /* Destroy CLI if initialized */
    destroy_cli(PJ_TRUE);

    /* Init PJLIB */
    status = pj_init();
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_inited = PJ_TRUE;

    /* Init PJLIB-UTIL */
    status = pjlib_util_init();
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Init CLI */
    pj_caching_pool_init(&cli_cp, NULL, 0);
    pj_cli_cfg_default(&cli_cfg);
    cli_cfg.pf = &cli_cp.factory;
    cli_cfg.name = pj_str("pjsua_cli");
    cli_cfg.title = pj_str("Pjsua CLI Application");
    status = pj_cli_create(&cli_cfg, &cli);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = setup_command(cli);
    if (status != PJ_SUCCESS)
	goto on_error;

    if (on_destroy_cb)
	cli_on_destroy_cb = on_destroy_cb;

    if (on_restart_pjsua_cb)
	cli_on_restart_pjsua_cb = on_restart_pjsua_cb;

    if (on_quit_cb)
	cli_on_quit_cb = on_quit_cb;

    /* Init telnet frontend */
    if (with_telnet) {
	pj_cli_telnet_cfg telnet_cfg;
	pj_pool_t *pool;

	pool = pj_pool_create(&cli_cp.factory, "cli_cp", 128, 128, NULL);
	pj_assert(pool);

	pj_cli_telnet_cfg_default(&telnet_cfg);
	telnet_cfg.log_level = 5;
	telnet_cfg.port = telnet_port;
	if (on_started_cb)
	    cli_telnet_on_started_cb = on_started_cb;

	telnet_cfg.on_started = cli_telnet_on_started_cb;

	status = pj_cli_telnet_create(cli, &telnet_cfg, NULL);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Init console frontend */
    if (with_console) {
	pj_cli_console_cfg console_cfg;
	
	pj_cli_console_cfg_default(&console_cfg);
	console_cfg.quit_command = pj_str("shutdown");
	status = pj_cli_console_create(cli, &console_cfg,
				       &cli_cons_sess, NULL);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    return PJ_SUCCESS;

on_error:
    destroy_cli(PJ_FALSE);
    return status;
}

PJ_DEF(pj_bool_t) is_cli_inited()
{
    return (cli != NULL);
}

static pj_status_t setup_timer(pj_timer_heap_t **timer, 
			       pj_ioqueue_t **ioqueue)
{
    pj_status_t status = pj_timer_heap_create(app_config.pool, 16, timer);

    if (status != PJ_SUCCESS)
	return status;

    status = pj_ioqueue_create(app_config.pool, 16, ioqueue);    

    return status;
}

static pj_status_t stop_timer(pj_timer_heap_t *timer, 
			      pj_ioqueue_t *ioqueue)
{
    if ((!timer) || (!ioqueue))
	return PJ_SUCCESS;

    pj_timer_heap_destroy(timer);

    return pj_ioqueue_destroy(ioqueue);
}

pj_status_t cli_pjsua_start(pj_str_t *uri_to_call, 
			    pj_timer_heap_t **main_timer_heap, 
			    pj_ioqueue_t **main_ioqueue)
{
    pj_status_t status = PJ_SUCCESS;

    pjsua_restarted = PJ_FALSE;

    if (app_config.disable_cli_console) {
	status = setup_timer(main_timer_heap, main_ioqueue);
	if (status != PJ_SUCCESS)
	    return status;
    }

    status = pjsua_start();
    if (status != PJ_SUCCESS)
	return status;

    pj_log_set_log_func(&log_writer);

    setup_signal_handler();

    /* If user specifies URI to call, then call the URI */
    if (uri_to_call->slen) {
	pjsua_call_make_call(current_acc, uri_to_call, &call_opt, NULL, 
			     NULL, NULL);
    }

    if (!app_config.disable_cli_console)
	PJ_LOG(3,(THIS_FILE, "CLI console is ready, press '?' for help"));

    return status;
}

void start_cli_main(pj_str_t *uri_to_call, pj_bool_t *app_restart)
{    
    pj_status_t status;
    char cmdline[PJ_CLI_MAX_CMDBUF];
    pj_timer_heap_t *main_timer_heap = NULL;
    pj_ioqueue_t *main_ioqueue = NULL;    

    *app_restart = PJ_FALSE;
    pjsua_restarted = PJ_TRUE;

    do {
	if (pjsua_restarted) {
	    status = cli_pjsua_start(uri_to_call, &main_timer_heap, 
				     &main_ioqueue);

	    if (status != PJ_SUCCESS)
		return;
	}

	if (app_config.disable_cli_console) {
	    pj_time_val delay = {0, 10};
	    pj_ioqueue_poll(main_ioqueue, &delay);
	    if (pj_cli_is_quitting(cli))
		continue;
	    pj_timer_heap_poll(main_timer_heap, NULL);     
	} else {
	    pj_cli_console_process(cli_cons_sess, &cmdline[0], sizeof(cmdline));
	}
	if (pjsua_restarted) {
	    status = stop_timer(main_timer_heap, main_ioqueue);
	    if (status != PJ_SUCCESS)
		return;
	    
	    status = app_init(NULL, NULL, NULL, NULL);
	    if (status != PJ_SUCCESS)
		return;
	}

    } while (!pj_cli_is_quitting(cli));
    stop_timer(main_timer_heap, main_ioqueue);
    *app_restart = pj_cli_is_restarting(cli);
}
