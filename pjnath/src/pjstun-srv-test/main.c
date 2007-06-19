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
#include "server.h"

#define THIS_FILE	"main.c"

struct options
{
    char	*realm;
    char	*user_name;
    char	*password;
    char	*nonce;
    pj_bool_t	 use_fingerprint;
} o;

static void usage(void)
{
    puts("Usage: pjstun_srv_test [OPTIONS]");
    puts("");
    puts("where OPTIONS:");
    puts(" --realm, -r       Set realm of the credential");
    puts(" --username, -u    Set username of the credential");
    puts(" --password, -p    Set password of the credential");
    puts(" --nonce, -N       Set NONCE");      
    puts(" --fingerprint, -F Use fingerprint for outgoing requests");
    puts(" --help, -h");
}


static void server_main(pj_stun_server *srv)
{
    int quit = 0;

    while (!quit) {
	char line[10];

	printf("Menu:\n"
	       "  d     Dump status\n"
	       "  q     Quit\n"
	       "Choice:");

	fgets(line, sizeof(line), stdin);
	if (line[0] == 'q') {
	    quit = 1;
	} else if (line[0] == 'd') {
	    pj_stun_server_info *si = pj_stun_server_get_info(srv);
	    pj_pool_factory_dump(si->pf, PJ_TRUE);
	}
    }
}

int main(int argc, char *argv[])
{
    struct pj_getopt_option long_options[] = {
	{ "realm",	1, 0, 'r'},
	{ "username",	1, 0, 'u'},
	{ "password",	1, 0, 'p'},
	{ "nonce",	1, 0, 'N'},
	{ "fingerprint",0, 0, 'F'},
	{ "help",	0, 0, 'h'}
    };
    int c, opt_id;
    pj_caching_pool cp;
    pj_stun_server *srv;
    pj_stun_usage *turn;
    pj_status_t status;

    while((c=pj_getopt_long(argc,argv, "r:u:p:N:hF", long_options, &opt_id))!=-1) {
	switch (c) {
	case 'r':
	    o.realm = pj_optarg;
	    break;
	case 'u':
	    o.user_name = pj_optarg;
	    break;
	case 'p':
	    o.password = pj_optarg;
	    break;
	case 'N':
	    o.nonce = pj_optarg;
	    break;
	case 'h':
	    usage();
	    return 0;
	case 'F':
	    o.use_fingerprint = PJ_TRUE;
	    break;
	default:
	    printf("Argument \"%s\" is not valid. Use -h to see help",
		   argv[pj_optind]);
	    return 1;
	}
    }

    if (pj_optind != argc) {
	puts("Error: invalid arguments");
	return 1;
    }

    pj_init();
    pjlib_util_init();
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    status = pj_stun_server_create(&cp.factory, 1, &srv);
    if (status != PJ_SUCCESS) {
	pj_stun_perror(THIS_FILE, "Unable to create server", status);
	return 1;
    }

    /*
    status = pj_stun_bind_usage_create(srv, NULL, 3478, NULL);
    if (status != PJ_SUCCESS) {
	pj_stun_perror(THIS_FILE, "Unable to create bind usage", status);
	return 1;
    }
    */

    status = pj_stun_turn_usage_create(srv, PJ_SOCK_DGRAM, NULL,
				       3478, o.use_fingerprint, &turn);
    if (status != PJ_SUCCESS) {
	pj_stun_perror(THIS_FILE, "Unable to create bind usage", status);
	return 1;
    }

    if (o.user_name && o.password) {
	pj_stun_auth_cred cred;
	pj_bzero(&cred, sizeof(cred));
	cred.type = PJ_STUN_AUTH_CRED_STATIC;
	cred.data.static_cred.realm = pj_str(o.realm);
	cred.data.static_cred.username = pj_str(o.user_name);
	cred.data.static_cred.data_type = 0;
	cred.data.static_cred.data = pj_str(o.password);
	cred.data.static_cred.nonce = pj_str(o.nonce);
	pj_stun_turn_usage_set_credential(turn, &cred);
    }

    server_main(srv);

    pj_stun_server_destroy(srv);
    pj_pool_factory_dump(&cp.factory, PJ_TRUE);
    pj_shutdown();
    return 0;
}
