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
#include <pjnath.h>
#include <pjlib-util.h>
#include <pjlib.h>


#define THIS_FILE	"client_main.c"
#define LOCAL_PORT	1998
#define BANDWIDTH	64		    /* -1 to disable */
#define LIFETIME	600		    /* -1 to disable */
#define REQ_TRANSPORT	-1		    /* 0: udp, 1: tcp, -1: disable */
#define REQ_PORT_PROPS	-1		    /* -1 to disable */
#define REQ_IP		0		    /* IP address string */

//#define OPTIONS		PJ_STUN_NO_AUTHENTICATE
#define OPTIONS		0


static struct global
{
    pj_stun_config	 stun_config;
    pj_pool_t		*pool;
    pj_caching_pool	 cp;
    pj_timer_heap_t	*th;
    pj_stun_session	*sess;
    pj_sock_t		 sock;
    pj_thread_t		*thread;
    pj_bool_t		 quit;
    pj_sockaddr_in	 peer_addr;
    pj_sockaddr_in	 srv_addr;
    pj_sockaddr_in	 relay_addr;
    char		 data_buf[256];
    char		*data;
} g;

static struct options
{
    char    *srv_addr;
    char    *srv_port;
    char    *realm;
    char    *user_name;
    char    *password;
    char    *nonce;
    char    *peer_addr;
    pj_bool_t use_fingerprint;
} o;


static pj_status_t parse_addr(const char *input, pj_sockaddr_in *addr);


static void my_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3,(THIS_FILE, "%s: %s", title, errmsg));
}

static pj_status_t on_send_msg(pj_stun_session *sess,
			       const void *pkt,
			       pj_size_t pkt_size,
			       const pj_sockaddr_t *srv_addr,
			       unsigned addr_len)
{
    pj_ssize_t len;
    pj_status_t status;

    len = pkt_size;
    status = pj_sock_sendto(g.sock, pkt, &len, 0, srv_addr, addr_len);

    if (status != PJ_SUCCESS)
	my_perror("Error sending packet", status);

    return status;
}

static void on_request_complete(pj_stun_session *sess,
			        pj_status_t status,
			        pj_stun_tx_data *tdata,
			        const pj_stun_msg *response,
				const pj_sockaddr_t *src_addr,
				unsigned src_addr_len)
{
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    if (status == PJ_SUCCESS) {
	switch (response->hdr.type) {
	case PJ_STUN_ALLOCATE_RESPONSE:
	    {
		pj_stun_relay_addr_attr *ar;

		ar = (pj_stun_relay_addr_attr*)
		     pj_stun_msg_find_attr(response, 
					   PJ_STUN_ATTR_RELAY_ADDR, 0);
		if (ar) {
		    pj_memcpy(&g.relay_addr, &ar->sockaddr.ipv4,
			      sizeof(pj_sockaddr_in));
		    PJ_LOG(3,(THIS_FILE, "Relay address is %s:%d",
			      pj_inet_ntoa(g.relay_addr.sin_addr),
			      (int)pj_ntohs(g.relay_addr.sin_port)));
		} else {
		    pj_memset(&g.relay_addr, 0, sizeof(g.relay_addr));
		}
	    }
	    break;
	}
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
		pj_uint8_t buffer[512];
		pj_ssize_t len;
		pj_sockaddr_in addr;
		int addrlen;
		pj_status_t rc;

		len = sizeof(buffer);
		addrlen = sizeof(addr);
		rc = pj_sock_recvfrom(g.sock, buffer, &len, 0, &addr, &addrlen);
		if (rc != PJ_SUCCESS || len <= 0)
		    continue;

		if (pj_stun_msg_check(buffer, len, PJ_STUN_IS_DATAGRAM)==PJ_SUCCESS) {
		    rc = pj_stun_session_on_rx_pkt(g.sess, buffer, len, 
						   OPTIONS, 
						   NULL, &addr, addrlen);
		    if (rc != PJ_SUCCESS)
			my_perror("Error processing packet", rc);

		} else {
		    buffer[len] = '\0';
		    PJ_LOG(3,(THIS_FILE, "Received data: %s", (char*)buffer));
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
    int len;
    pj_status_t status;

    g.sock = PJ_INVALID_SOCKET;

    status = pj_init();
    status = pjlib_util_init();
    status = pjnath_init();

    pj_caching_pool_init(&g.cp, &pj_pool_factory_default_policy, 0);

    if (o.srv_addr) {
	pj_str_t s;
	pj_uint16_t port;

	if (o.srv_port)
	    port = (pj_uint16_t) atoi(o.srv_port);
	else
	    port = PJ_STUN_PORT;

	status = pj_sockaddr_in_init(&g.srv_addr, pj_cstr(&s, o.srv_addr), port);
	if (status != PJ_SUCCESS) {
	    my_perror("Invalid address", status);
	    return status;
	}

	printf("Destination address set to %s:%d\n", o.srv_addr, (int)port);
    } else {
	printf("Error: address must be specified\n");
	return PJ_EINVAL;
    }

    g.pool = pj_pool_create(&g.cp.factory, NULL, 1000, 1000, NULL);

    status = pj_timer_heap_create(g.pool, 1000, &g.th);
    pj_assert(status == PJ_SUCCESS);

    pj_stun_config_init(&g.stun_config, &g.cp.factory, 0, NULL, g.th);
    pj_assert(status == PJ_SUCCESS);

    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &g.sock);
    pj_assert(status == PJ_SUCCESS);

    status = pj_sockaddr_in_init(&addr, NULL, 0);
    pj_assert(status == PJ_SUCCESS);

    addr.sin_port = pj_htons((pj_uint16_t)LOCAL_PORT);
    status = pj_sock_bind(g.sock, &addr, sizeof(addr));
    pj_assert(status == PJ_SUCCESS);

    len = sizeof(addr);
    status = pj_sock_getsockname(g.sock, &addr, &len);
    pj_assert(status == PJ_SUCCESS);

    PJ_LOG(3,(THIS_FILE, "Listening on port %d", (int)pj_ntohs(addr.sin_port)));

    pj_memcpy(&g.peer_addr, &addr, sizeof(pj_sockaddr_in));
    if (g.peer_addr.sin_addr.s_addr == 0)
	pj_gethostip(&g.peer_addr.sin_addr);

    pj_memset(&stun_cb, 0, sizeof(stun_cb));
    stun_cb.on_send_msg = &on_send_msg;
    stun_cb.on_request_complete = &on_request_complete;

    status = pj_stun_session_create(&g.stun_config, NULL, &stun_cb, 
				    o.use_fingerprint!=0, &g.sess);
    pj_assert(status == PJ_SUCCESS);

    if (o.user_name) {
	pj_stun_auth_cred cred;

	pj_bzero(&cred, sizeof(cred));

	cred.type = PJ_STUN_AUTH_CRED_STATIC;
	cred.data.static_cred.realm = pj_str(o.realm);
	cred.data.static_cred.username = pj_str(o.user_name);
	cred.data.static_cred.data_type = 0;
	cred.data.static_cred.data = pj_str(o.password);
	cred.data.static_cred.nonce = pj_str(o.nonce);

	pj_stun_session_set_credential(g.sess, &cred);
	puts("Session credential set");
    } else {
	puts("Credential not set");
    }

    if (o.peer_addr) {
	if (parse_addr(o.peer_addr, &g.peer_addr)!=PJ_SUCCESS)
	    return -1;
    }

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

static void send_bind_request(void)
{
    pj_stun_tx_data *tdata;
    pj_status_t rc;

    rc = pj_stun_session_create_req(g.sess, PJ_STUN_BINDING_REQUEST, 
				    NULL, &tdata);
    pj_assert(rc == PJ_SUCCESS);

    rc = pj_stun_session_send_msg(g.sess, PJ_FALSE, 
				  &g.srv_addr, sizeof(g.srv_addr),
				  tdata);
    if (rc != PJ_SUCCESS)
	my_perror("Error sending STUN request", rc);
}

static void send_allocate_request(pj_bool_t allocate)
{
    pj_stun_tx_data *tdata;
    pj_status_t rc;

    rc = pj_stun_session_create_req(g.sess, PJ_STUN_ALLOCATE_REQUEST, 
				    NULL, &tdata);
    pj_assert(rc == PJ_SUCCESS);


    if (BANDWIDTH != -1) {
	pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg, 
				  PJ_STUN_ATTR_BANDWIDTH, BANDWIDTH);
    }

    if (!allocate) {
	pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg, 
				  PJ_STUN_ATTR_LIFETIME, 0);

    } else {
	if (LIFETIME != -1) {
	    pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg, 
				      PJ_STUN_ATTR_LIFETIME, LIFETIME);
	}

	if (REQ_TRANSPORT != -1) {
	    pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg, 
				      PJ_STUN_ATTR_REQ_TRANSPORT, REQ_TRANSPORT);
	}

	if (REQ_PORT_PROPS != -1) {
	    pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg, 
				      PJ_STUN_ATTR_REQ_PORT_PROPS, REQ_PORT_PROPS);
	}

	if (REQ_IP) {
	    pj_sockaddr_in addr;
	    pj_str_t tmp;

	    pj_sockaddr_in_init(&addr, pj_cstr(&tmp, REQ_IP), 0);
	    pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg,
					 PJ_STUN_ATTR_REQ_IP, PJ_FALSE,
					 &addr, sizeof(addr));
	}
    }

    rc = pj_stun_session_send_msg(g.sess, PJ_FALSE, 
				  &g.srv_addr, sizeof(g.srv_addr),
				  tdata);
    pj_assert(rc == PJ_SUCCESS);
}

static void send_sad_request(pj_bool_t set)
{
    pj_stun_tx_data *tdata;
    pj_status_t rc;

    if (g.peer_addr.sin_addr.s_addr == 0 ||
	g.peer_addr.sin_port == 0)
    {
	puts("Error: peer address is not set");
	return;
    }

    rc = pj_stun_session_create_req(g.sess, 
				    PJ_STUN_SET_ACTIVE_DESTINATION_REQUEST, 
				    NULL, &tdata);
    pj_assert(rc == PJ_SUCCESS);

    if (set) {
	pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg,
				     PJ_STUN_ATTR_REMOTE_ADDR, PJ_FALSE,
				     &g.peer_addr, sizeof(g.peer_addr));
    }

    rc = pj_stun_session_send_msg(g.sess, PJ_FALSE, 
				  &g.srv_addr, sizeof(g.srv_addr),
				  tdata);
    pj_assert(rc == PJ_SUCCESS);
}

static void send_send_ind(void)
{
    pj_stun_tx_data *tdata;
    int len;
    pj_status_t rc;

    if (g.peer_addr.sin_addr.s_addr == 0 ||
	g.peer_addr.sin_port == 0)
    {
	puts("Error: peer address is not set");
	return;
    }

    len = strlen(g.data);
    if (len==0) {
	puts("Error: data is not set");
	return;
    }

    rc = pj_stun_session_create_ind(g.sess, PJ_STUN_SEND_INDICATION, &tdata);
    pj_assert(rc == PJ_SUCCESS);

    pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg,
				  PJ_STUN_ATTR_REMOTE_ADDR, PJ_FALSE,
				  &g.peer_addr, sizeof(g.peer_addr));
    pj_stun_msg_add_binary_attr(tdata->pool, tdata->msg,
				PJ_STUN_ATTR_DATA, (pj_uint8_t*)g.data, len);

    rc = pj_stun_session_send_msg(g.sess, PJ_FALSE, 
				  &g.srv_addr, sizeof(g.srv_addr),
				  tdata);
    pj_assert(rc == PJ_SUCCESS);

}

static void send_raw_data_to_srv(void)
{
    pj_ssize_t len;

    if (g.srv_addr.sin_addr.s_addr == 0 ||
	g.srv_addr.sin_port == 0)
    {
	puts("Error: server address is not set");
	return;
    }

    len = strlen(g.data);
    if (len==0) {
	puts("Error: data is not set");
	return;
    }

    len = strlen(g.data);
    pj_sock_sendto(g.sock, g.data, &len, 0, &g.srv_addr, sizeof(g.srv_addr));
}

static void send_raw_data_to_relay(void)
{
    pj_ssize_t len;

    if (g.relay_addr.sin_addr.s_addr == 0 ||
	g.relay_addr.sin_port == 0)
    {
	puts("Error: relay address is not set");
	return;
    }

    len = strlen(g.data);
    if (len==0) {
	puts("Error: data is not set");
	return;
    }

    len = strlen(g.data);
    pj_sock_sendto(g.sock, g.data, &len, 0, &g.relay_addr, sizeof(g.relay_addr));
}

static pj_status_t parse_addr(const char *input,
			      pj_sockaddr_in *addr)
{
    const char *pos;
    pj_str_t ip;
    pj_uint16_t port;
    pj_sockaddr_in tmp_addr;

    pos = pj_ansi_strchr(input, ':');
    if (pos==NULL) {
	puts("Invalid format");
	return -1;
    }

    ip.ptr = (char*)input;
    ip.slen = pos - input;
    port = (pj_uint16_t)atoi(pos+1);

    if (port==0) {
	puts("Invalid port");
	return -1;
    }

    if (pj_sockaddr_in_init(&tmp_addr, &ip, port)!=PJ_SUCCESS) {
	puts("Invalid address");
	return -1;
    }

    pj_memcpy(addr, &tmp_addr, sizeof(tmp_addr));

    return PJ_SUCCESS;
}

static void set_peer_addr(void)
{
    char addr[64];

    printf("Current peer address: %s:%d\n", 
	   pj_inet_ntoa(g.peer_addr.sin_addr), 
	   pj_ntohs(g.peer_addr.sin_port));

    printf("Input peer address in IP:PORT format: ");
    fflush(stdout);
    fgets(addr, sizeof(addr), stdin);

    if (parse_addr(addr, &g.peer_addr) != PJ_SUCCESS) {
	return;
    }

}

static void menu(void)
{
    puts("Menu:");
    printf("  pr      Set peer address (currently %s:%d)\n",
	   pj_inet_ntoa(g.peer_addr.sin_addr), pj_ntohs(g.peer_addr.sin_port));
    printf("  dt      Set data (currently \"%s\")\n", g.data);
    puts("  br      Send Bind request");
    puts("  ar      Send Allocate request");
    puts("  dr      Send de-Allocate request");
    puts("  sr      Send Set Active Destination request");
    puts("  cr      Send clear Active Destination request");
    puts("  si      Send data with Send Indication");
    puts("  rw      Send raw data to TURN server");
    puts("  rW      Send raw data to relay address");
    puts("  q       Quit");
    puts("");
    printf("Choice: ");
}


static void console_main(void)
{
    while (!g.quit) {
	char input[10];

	menu();

	fgets(input, sizeof(input), stdin);
	
	if (0) {

	} else if (input[0]=='d' && input[1]=='t') {
	    printf("Input data: ");
	    fgets(g.data, sizeof(g.data_buf), stdin);
	    
	} else if (input[0]=='p' && input[1]=='r') {
	    set_peer_addr();
	    
	} else if (input[0]=='b' && input[1]=='r') {
	    send_bind_request();
	    
	} else if (input[0]=='a' && input[1]=='r') {
	    send_allocate_request(PJ_TRUE);
	    
	} else if (input[0]=='d' && input[1]=='r') {
	    send_allocate_request(PJ_FALSE);
	    
	} else if (input[0]=='s' && input[1]=='r') {
	    send_sad_request(PJ_TRUE);
	    
	} else if (input[0]=='c' && input[1]=='r') {
	    send_sad_request(PJ_FALSE);
	    
	} else if (input[0]=='s' && input[1]=='i') {
	    send_send_ind();
	    
	} else if (input[0]=='r' && input[1]=='w') {
	    send_raw_data_to_srv();
	    
	} else if (input[0]=='r' && input[1]=='W') {
	    send_raw_data_to_relay();
	    
	} else if (input[0]=='q') {
	    g.quit = 1;
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
    puts(" --nonce, -N       Set NONCE");   
    puts(" --fingerprint, -F Use fingerprint for outgoing requests");
    puts(" --peer, -P        Set peer address (address is in HOST:PORT format)");
    puts(" --data, -D        Set data");
    puts(" --help, -h");
}

int main(int argc, char *argv[])
{
    struct pj_getopt_option long_options[] = {
	{ "realm",	1, 0, 'r'},
	{ "username",	1, 0, 'u'},
	{ "password",	1, 0, 'p'},
	{ "nonce",	1, 0, 'N'},
	{ "fingerprint",0, 0, 'F'},
	{ "peer",	1, 0, 'P'},
	{ "data",	1, 0, 'D'},
	{ "help",	0, 0, 'h'}
    };
    int c, opt_id;
    char *pos;
    pj_status_t status;

    g.data = g.data_buf;

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
	case 'N':
	    o.nonce = pj_optarg;
	    break;
	case 'h':
	    usage();
	    return 0;
	case 'F':
	    o.use_fingerprint = PJ_TRUE;
	    break;
	case 'P':
	    o.peer_addr = pj_optarg;
	    break;
	case 'D':
	    g.data = pj_optarg;
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
	o.srv_addr = argv[pj_optind];
	*pos = '\0';
	o.srv_port = pos+1;
    } else {
	o.srv_addr = argv[pj_optind];
    }

    status = init();
    if (status != PJ_SUCCESS)
	goto on_return;
    
    console_main();

on_return:
    shutdown();
    return status ? 1 : 0;
}

