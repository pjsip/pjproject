/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#include <pjlib-util.h>
#include <pjlib.h>


#define THIS_FILE	"client_main.c"

static struct global
{
    pj_stun_endpoint	*endpt;
    pj_pool_t		*pool;
    pj_caching_pool	 cp;
    pj_timer_heap_t	*th;
    pj_stun_session	*sess;
    unsigned		 sess_options;
    pj_sock_t		 sock;
    pj_thread_t		*thread;
    pj_bool_t		 quit;

    pj_sockaddr_in	 dst_addr;  /**< destination addr */

} g;

static struct options
{
    char    *dst_addr;
    char    *dst_port;
    char    *realm;
    char    *user_name;
    char    *password;
    pj_bool_t use_fingerprint;
} o;


static my_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3,(THIS_FILE, "%s: %s", title, errmsg));
}

static pj_status_t on_send_msg(pj_stun_session *sess,
			       const void *pkt,
			       pj_size_t pkt_size,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len)
{
    pj_ssize_t len;
    pj_status_t status;

    len = pkt_size;
    status = pj_sock_sendto(g.sock, pkt, &len, 0, dst_addr, addr_len);

    if (status != PJ_SUCCESS)
	my_perror("Error sending packet", status);

    return status;
}

static void on_request_complete(pj_stun_session *sess,
			        pj_status_t status,
			        pj_stun_tx_data *tdata,
			        const pj_stun_msg *response)
{
    if (status == PJ_SUCCESS) {
	puts("Client transaction completes");
    } else {
	my_perror("Client transaction error", status);
    }
}

static int worker_thread(void *unused)
{
    PJ_UNUSED_ARG(unused);

    while (!g.quit) {
	pj_time_val timeout =  {0, 50};
	pj_fd_set_t readset;
	int n;

	pj_timer_heap_poll(g.th, NULL);

	PJ_FD_ZERO(&readset);
	PJ_FD_SET(g.sock, &readset);

	n = pj_sock_select(g.sock+1, &readset, NULL, NULL, &timeout);
	if (n > 0) {
	    if (PJ_FD_ISSET(g.sock, &readset)) {
		char buffer[512];
		pj_ssize_t len;
		pj_sockaddr_in addr;
		int addrlen;
		pj_status_t rc;

		len = sizeof(buffer);
		addrlen = sizeof(addr);
		rc = pj_sock_recvfrom(g.sock, buffer, &len, 0, &addr, &addrlen);
		if (rc == PJ_SUCCESS && len > 0) {
		    rc = pj_stun_session_on_rx_pkt(g.sess, buffer, len, 
						   PJ_STUN_IS_DATAGRAM|PJ_STUN_CHECK_PACKET, 
						   NULL, &addr, addrlen);
		    if (rc != PJ_SUCCESS)
			my_perror("Error processing packet", rc);
		}
	    }
	} else if (n < 0)
	    pj_thread_sleep(50);
    }

    return 0;
}

static int init()
{
    pj_sockaddr_in addr;
    pj_stun_session_cb stun_cb;
    pj_status_t status;

    g.sock = PJ_INVALID_SOCKET;

    status = pj_init();
    status = pjlib_util_init();

    pj_caching_pool_init(&g.cp, &pj_pool_factory_default_policy, 0);

    if (o.dst_addr) {
	pj_str_t s;
	pj_uint16_t port;

	if (o.dst_port)
	    port = (pj_uint16_t) atoi(o.dst_port);
	else
	    port = PJ_STUN_PORT;

	status = pj_sockaddr_in_init(&g.dst_addr, pj_cstr(&s, o.dst_addr), port);
	if (status != PJ_SUCCESS) {
	    my_perror("Invalid address", status);
	    return status;
	}

	printf("Destination address set to %s:%d\n", o.dst_addr, (int)port);
    } else {
	printf("Error: address must be specified\n");
	return PJ_EINVAL;
    }

    g.pool = pj_pool_create(&g.cp.factory, NULL, 1000, 1000, NULL);

    status = pj_timer_heap_create(g.pool, 1000, &g.th);
    pj_assert(status == PJ_SUCCESS);

    status = pj_stun_endpoint_create(&g.cp.factory, 0, NULL, g.th, &g.endpt);
    pj_assert(status == PJ_SUCCESS);

    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &g.sock);
    pj_assert(status == PJ_SUCCESS);

    status = pj_sockaddr_in_init(&addr, NULL, 0);
    pj_assert(status == PJ_SUCCESS);

    pj_memset(&stun_cb, 0, sizeof(stun_cb));
    stun_cb.on_send_msg = &on_send_msg;
    stun_cb.on_request_complete = &on_request_complete;

    status = pj_stun_session_create(g.endpt, NULL, &stun_cb, &g.sess);
    pj_assert(status == PJ_SUCCESS);

    if (o.realm) {
	pj_str_t r, u, p;

	if (o.user_name == NULL) {
	    printf("error: username must be specified\n");
	    return PJ_EINVAL;
	}
	if (o.password == NULL)
	    o.password = "";
	g.sess_options = PJ_STUN_USE_LONG_TERM_CRED;
	pj_stun_session_set_long_term_credential(g.sess, pj_cstr(&r, o.realm),
						 pj_cstr(&u, o.user_name),
						 pj_cstr(&p, o.password));
	puts("Using long term credential");
    } else if (o.user_name) {
	pj_str_t u, p;

	if (o.password == NULL)
	    o.password = "";
	g.sess_options = PJ_STUN_USE_SHORT_TERM_CRED;
	pj_stun_session_set_short_term_credential(g.sess, 
						  pj_cstr(&u, o.user_name),
						  pj_cstr(&p, o.password));
	puts("Using short term credential");
    } else {
	puts("Credential not set");
    }

    if (o.use_fingerprint)
	g.sess_options |= PJ_STUN_USE_FINGERPRINT;

    status = pj_thread_create(g.pool, "stun", &worker_thread, NULL, 
			      0, 0, &g.thread);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}


static int shutdown()
{
    if (g.thread) {
	g.quit = 1;
	pj_thread_join(g.thread);
	pj_thread_destroy(g.thread);
	g.thread = NULL;
    }
    if (g.sess)
	pj_stun_session_destroy(g.sess);
    if (g.endpt)
	pj_stun_endpoint_destroy(g.endpt);
    if (g.sock != PJ_INVALID_SOCKET)
	pj_sock_close(g.sock);
    if (g.th)
	pj_timer_heap_destroy(g.th);
    if (g.pool)
	pj_pool_release(g.pool);

    pj_pool_factory_dump(&g.cp.factory, PJ_TRUE);
    pj_caching_pool_destroy(&g.cp);

    return PJ_SUCCESS;
}

static void menu(void)
{
    puts("Menu:");
    puts("  b      Send Bind request");
    puts("  q      Quit");
    puts("");
    printf("Choice: ");
}

static void console_main(void)
{
    while (!g.quit) {
	char input[10];

	menu();

	fgets(input, sizeof(input), stdin);
	
	switch (input[0]) {
	case 'b':
	    {
		pj_stun_tx_data *tdata;
		pj_status_t rc;

		rc = pj_stun_session_create_bind_req(g.sess, &tdata);
		pj_assert(rc == PJ_SUCCESS);

		rc = pj_stun_session_send_msg(g.sess, g.sess_options, 
					      &g.dst_addr, sizeof(g.dst_addr),
					      tdata);
		if (rc != PJ_SUCCESS)
		    my_perror("Error sending STUN request", rc);
	    }
	    break;
	case 'q':
	    g.quit = 1;
	    break;
	default:
	    break;
	}
    }
}


static void usage(void)
{
    puts("Usage: pjstun_client TARGET [OPTIONS]");
    puts("");
    puts("where TARGET is \"host[:port]\"");
    puts("");
    puts("and OPTIONS:");
    puts(" --realm, -r       Set realm of the credential");
    puts(" --username, -u    Set username of the credential");
    puts(" --password, -p    Set password of the credential");
    puts(" --fingerprint, -F Use fingerprint for outgoing requests");
    puts(" --help, -h");
}

int main(int argc, char *argv[])
{
    struct pj_getopt_option long_options[] = {
	{ "realm",	1, 0, 'r'},
	{ "username",	1, 0, 'u'},
	{ "password",	1, 0, 'p'},
	{ "fingerprint",0, 0, 'F'},
	{ "help",	0, 0, 'h'}
    };
    int c, opt_id;
    char *pos;
    pj_status_t status;

    while((c=pj_getopt_long(argc,argv, "r:u:p:hF", long_options, &opt_id))!=-1) {
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

    if (pj_optind == argc) {
	puts("Error: TARGET is needed");
	return 1;
    }

    if ((pos=pj_ansi_strchr(argv[pj_optind], ':')) != NULL) {
	o.dst_addr = argv[pj_optind];
	*pos = '\0';
	o.dst_port = pos+1;
    } else {
	o.dst_addr = argv[pj_optind];
    }

    status = init();
    if (status != PJ_SUCCESS)
	goto on_return;
    
    console_main();

on_return:
    shutdown();
    return status ? 1 : 0;
}

