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
#include "turn.h"
#include "auth.h"

#define REALM	"pjsip.org"

int err(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    printf("%s: %s\n", title, errmsg);
    return 1;
}

int main()
{
    pj_caching_pool cp;
    pj_turn_srv *srv;
    pj_turn_listener *listener;
    pj_status_t status;

    status = pj_init();
    if (status != PJ_SUCCESS)
	return err("pj_init() error", status);

    pjlib_util_init();
    pjnath_init();

    pj_caching_pool_init(&cp, NULL, 0);

    pj_turn_auth_init(REALM);

    status = pj_turn_srv_create(&cp.factory, &srv);
    if (status != PJ_SUCCESS)
	return err("Error creating server", status);

    status = pj_turn_listener_create_udp(srv, pj_AF_INET(), NULL, 
					 PJ_STUN_PORT, 1, 0, &listener);
    if (status != PJ_SUCCESS)
	return err("Error creating listener", status);

    status = pj_turn_srv_add_listener(srv, listener);
    if (status != PJ_SUCCESS)
	return err("Error adding listener", status);

    puts("Server is running");
    puts("Press <ENTER> to quit");

    {
	char line[10];
	fgets(line, sizeof(line), stdin);
    }

    pj_turn_srv_destroy(srv);
    pj_caching_pool_destroy(&cp);
    pj_shutdown();

    return 0;
}

