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
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_console_app.h>


#define THIS_FILE	"main.c"

/*****************************************************************************
 * main():
 */
int main(int argc, char *argv[])
{
    pjsua_config cfg;
    pj_str_t uri_to_call = { NULL, 0 };

    /* Init default settings. */
    pjsua_default_config(&cfg);


    /* Create PJLIB and memory pool */
    pjsua_create();


    /* Parse command line arguments: */
    if (pjsua_parse_args(argc, argv, &cfg, &uri_to_call) != PJ_SUCCESS)
	return 1;


    /* Init pjsua */
    if (pjsua_init(&cfg, &console_callback) != PJ_SUCCESS)
	return 1;


    /* Start pjsua! */
    if (pjsua_start() != PJ_SUCCESS) {
	pjsua_destroy();
	return 1;
    }


    /* Sleep for a while, let any messages get printed to console: */
    pj_thread_sleep(500);


    /* Start UI console main loop: */
    pjsua_console_app_main(&uri_to_call);


    /* Destroy pjsua: */
    pjsua_destroy();

    /* This is for internal testing, to make sure that pjsua_destroy()
     * can be called multiple times. 
     */
    pjsua_destroy();


    /* Exit... */

    return 0;
}

