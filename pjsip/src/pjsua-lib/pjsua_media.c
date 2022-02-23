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
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>


#define THIS_FILE		"pjsua_media.c"

#ifndef PJSUA_REQUIRE_CONSECUTIVE_RTCP_PORT
#   define PJSUA_REQUIRE_CONSECUTIVE_RTCP_PORT	0
#endif

#ifndef PJSUA_RESET_SRTP_ROC_ON_REM_ADDRESS_CHANGE
#   define PJSUA_RESET_SRTP_ROC_ON_REM_ADDRESS_CHANGE	0
#endif

static void stop_media_stream(pjsua_call *call, unsigned med_idx);

static void pjsua_media_config_dup(pj_pool_t *pool,
				   pjsua_media_config *dst,
				   const pjsua_media_config *src)
{
    pj_memcpy(dst, src, sizeof(*src));
    pj_strdup(pool, &dst->turn_server, &src->turn_server);
    pj_stun_auth_cred_dup(pool, &dst->turn_auth_cred, &src->turn_auth_cred);
#if PJ_HAS_SSL_SOCK
    pj_turn_sock_tls_cfg_dup(pool, &dst->turn_tls_setting,
			     &src->turn_tls_setting);
#endif
}


/**
 * Init media subsystems.
 */
pj_status_t pjsua_media_subsys_init(const pjsua_media_config *cfg)
{
    pj_status_t status;

    pj_log_push_indent();

    /* Specify which audio device settings are save-able */
    pjsua_var.aud_svmask = 0xFFFFFFFF;
    /* These are not-settable */
    pjsua_var.aud_svmask &= ~(PJMEDIA_AUD_DEV_CAP_EXT_FORMAT |
			      PJMEDIA_AUD_DEV_CAP_INPUT_SIGNAL_METER |
			      PJMEDIA_AUD_DEV_CAP_OUTPUT_SIGNAL_METER);
    /* EC settings use different API */
    pjsua_var.aud_svmask &= ~(PJMEDIA_AUD_DEV_CAP_EC |
			      PJMEDIA_AUD_DEV_CAP_EC_TAIL);

    /* Copy configuration */
    pjsua_media_config_dup(pjsua_var.pool, &pjsua_var.media_cfg, cfg);

    /* Normalize configuration */
    if (pjsua_var.media_cfg.snd_clock_rate == 0) {
	pjsua_var.media_cfg.snd_clock_rate = pjsua_var.media_cfg.clock_rate;
    }

    if (pjsua_var.media_cfg.has_ioqueue &&
	pjsua_var.media_cfg.thread_cnt == 0)
    {
	pjsua_var.media_cfg.thread_cnt = 1;
    }

    if (pjsua_var.media_cfg.max_media_ports < pjsua_var.ua_cfg.max_calls) {
	pjsua_var.media_cfg.max_media_ports = pjsua_var.ua_cfg.max_calls + 2;
    }

    /* Create media endpoint. */
    status = pjmedia_endpt_create(&pjsua_var.cp.factory, 
				  pjsua_var.media_cfg.has_ioqueue? NULL :
				     pjsip_endpt_get_ioqueue(pjsua_var.endpt),
				  pjsua_var.media_cfg.thread_cnt,
				  &pjsua_var.med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Media stack initialization has returned error", 
		     status);
	goto on_error;
    }

    status = pjsua_aud_subsys_init();
    if (status != PJ_SUCCESS)
	goto on_error;

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* Initialize SRTP library (ticket #788). */
    status = pjmedia_srtp_init_lib(pjsua_var.med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing SRTP library", 
		     status);
	goto on_error;
    }
#endif

    /* Video */
#if PJMEDIA_HAS_VIDEO
    status = pjsua_vid_subsys_init();
    if (status != PJ_SUCCESS)
	goto on_error;
#endif

    /* Create event manager */
    if (!pjmedia_event_mgr_instance()) {
	status = pjmedia_event_mgr_create(pjsua_var.pool, 0, NULL);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating PJMEDIA event manager",
			 status);
	    goto on_error;
	}
    }

    pj_log_pop_indent();
    return PJ_SUCCESS;

on_error:
    pj_log_pop_indent();
    return status;
}

/*
 * Start pjsua media subsystem.
 */
pj_status_t pjsua_media_subsys_start(void)
{
    pj_status_t status;

    pj_log_push_indent();

#if DISABLED_FOR_TICKET_1185
    /* Create media for calls, if none is specified */
    if (pjsua_var.calls[0].media[0].tp == NULL) {
	pjsua_transport_config transport_cfg;

	/* Create default transport config */
	pjsua_transport_config_default(&transport_cfg);
	transport_cfg.port = DEFAULT_RTP_PORT;

	status = pjsua_media_transports_create(&transport_cfg);
	if (status != PJ_SUCCESS) {
	    pj_log_pop_indent();
	    return status;
	}
    }
#endif

    /* Audio */
    status = pjsua_aud_subsys_start();
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	return status;
    }

    /* Video */
#if PJMEDIA_HAS_VIDEO
    status = pjsua_vid_subsys_start();
    if (status != PJ_SUCCESS) {
	pjsua_aud_subsys_destroy();
	pj_log_pop_indent();
	return status;
    }
#endif

    /* Perform NAT detection */
    // Performed only when STUN server resolution by pjsua_init() completed
    // successfully (see ticket #1865).
    //if (pjsua_var.ua_cfg.stun_srv_cnt) {
	//status = pjsua_detect_nat_type();
	//if (status != PJ_SUCCESS) {
	//    PJ_PERROR(1,(THIS_FILE, status, "NAT type detection failed"));
	//}
    //}

    pj_log_pop_indent();
    return PJ_SUCCESS;
}


/*
 * Destroy pjsua media subsystem.
 */
pj_status_t pjsua_media_subsys_destroy(unsigned flags)
{
    PJ_UNUSED_ARG(flags);

    PJ_LOG(4,(THIS_FILE, "Shutting down media.."));
    pj_log_push_indent();

    if (pjsua_var.med_endpt) {
        /* Wait for media endpoint's worker threads to quit. */
        pjmedia_endpt_stop_threads(pjsua_var.med_endpt);

	pjsua_aud_subsys_destroy();
    }

#if 0
    // This part has been moved out to pjsua_destroy() (see also #1717).
    /* Close media transports */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
        /* TODO: check if we're not allowed to send to network in the
         *       "flags", and if so do not do TURN allocation...
         */
	PJ_UNUSED_ARG(flags);
	pjsua_media_channel_deinit(i);
    }
#endif

    /* Destroy media endpoint. */
    if (pjsua_var.med_endpt) {

#	if PJMEDIA_HAS_VIDEO
	    pjsua_vid_subsys_destroy();
#	endif

	pjmedia_endpt_destroy(pjsua_var.med_endpt);
	pjsua_var.med_endpt = NULL;

	/* Deinitialize sound subsystem */
	// Not necessary, as pjmedia_snd_deinit() should have been called
	// in pjmedia_endpt_destroy().
	//pjmedia_snd_deinit();
    }

    if (pjmedia_event_mgr_instance())
	pjmedia_event_mgr_destroy(NULL);

    pj_log_pop_indent();

    return PJ_SUCCESS;
}

/*
 * Create RTP and RTCP socket pair, and possibly resolve their public
 * address via STUN.
 */
static pj_status_t create_rtp_rtcp_sock(pjsua_call_media *call_med,
					const pjsua_transport_config *cfg,
					pjmedia_sock_info *skinfo)
{
    enum {
	RTP_RETRY = 100
    };
    int i;
    pj_bool_t use_ipv6, use_nat64;
    int af;
    pj_sockaddr bound_addr;
    pj_sockaddr mapped_addr[2];
    pj_status_t status = PJ_SUCCESS;
    char addr_buf[PJ_INET6_ADDRSTRLEN+10];
    pjsua_acc *acc = &pjsua_var.acc[call_med->call->acc_id];
    pj_sock_t sock[2];

    use_ipv6 = (acc->cfg.ipv6_media_use != PJSUA_IPV6_DISABLED);
    use_nat64 = (acc->cfg.nat64_opt != PJSUA_NAT64_DISABLED);
    af = (use_ipv6 || use_nat64) ? pj_AF_INET6() : pj_AF_INET();

    /* Make sure STUN server resolution has completed */
    if ((!use_ipv6 || use_nat64) &&
        pjsua_media_acc_is_using_stun(call_med->call->acc_id))
    {
	pj_bool_t retry_stun = (acc->cfg.media_stun_use &
				PJSUA_STUN_RETRY_ON_FAILURE) ==
				PJSUA_STUN_RETRY_ON_FAILURE;
	status = resolve_stun_server(PJ_TRUE, retry_stun,
				     (unsigned)acc->cfg.nat64_opt);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	    return status;
	}
    }

    if (acc->next_rtp_port == 0 || cfg->port == 0)
	acc->next_rtp_port = (pj_uint16_t)cfg->port;

    for (i=0; i<2; ++i)
	sock[i] = PJ_INVALID_SOCKET;

    pj_sockaddr_init(af, &bound_addr, NULL, 0);
    if (cfg->bound_addr.slen) {
	status = pj_sockaddr_set_str_addr(af, &bound_addr, &cfg->bound_addr);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to resolve transport bind address",
			 status);
	    return status;
	}
    }

    /* Loop retry to bind RTP and RTCP sockets. */
    for (i=0; i<RTP_RETRY; ++i, acc->next_rtp_port += 2) {

        if (cfg->port > 0 && cfg->port_range > 0 &&
            (acc->next_rtp_port > cfg->port + cfg->port_range ||
             acc->next_rtp_port < cfg->port))
        {
            acc->next_rtp_port = (pj_uint16_t)cfg->port;
        }

	/* Create RTP socket. */
	status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &sock[0]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    return status;
	}

	/* Apply QoS to RTP socket, if specified */
	status = pj_sock_apply_qos2(sock[0], cfg->qos_type,
				    &cfg->qos_params,
				    2, THIS_FILE, "RTP socket");

	/* Apply sockopt, if specified */
	if (cfg->sockopt_params.cnt)
	    status = pj_sock_setsockopt_params(sock[0], &cfg->sockopt_params);

	/* Bind RTP socket */
	pj_sockaddr_set_port(&bound_addr, acc->next_rtp_port);
	status=pj_sock_bind(sock[0], &bound_addr,
	                    pj_sockaddr_get_len(&bound_addr));
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock[0]);
	    sock[0] = PJ_INVALID_SOCKET;
	    continue;
	}
	
	/* If bound to random port, find out the port number. */
	if (acc->next_rtp_port == 0) {
	    pj_sockaddr sock_addr;
	    int addr_len = sizeof(pj_sockaddr);

            status = pj_sock_getsockname(sock[0], &sock_addr, &addr_len);
	    if (status != PJ_SUCCESS) {
	    	pjsua_perror(THIS_FILE, "getsockname() error", status);
	    	pj_sock_close(sock[0]);
	    	return status;
	    }
	    acc->next_rtp_port = pj_sockaddr_get_port(&sock_addr);
	}

	/* Create RTCP socket. */
	status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &sock[1]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    pj_sock_close(sock[0]);
	    return status;
	}

	/* Apply QoS to RTCP socket, if specified */
	status = pj_sock_apply_qos2(sock[1], cfg->qos_type,
				    &cfg->qos_params,
				    2, THIS_FILE, "RTCP socket");

	/* Apply sockopt, if specified */
	if (cfg->sockopt_params.cnt)
	    status = pj_sock_setsockopt_params(sock[1], &cfg->sockopt_params);

	/* Bind RTCP socket */
	pj_sockaddr_set_port(&bound_addr, (pj_uint16_t)(acc->next_rtp_port+1));
	status=pj_sock_bind(sock[1], &bound_addr,
	                    pj_sockaddr_get_len(&bound_addr));
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock[0]);
	    sock[0] = PJ_INVALID_SOCKET;

	    pj_sock_close(sock[1]);
	    sock[1] = PJ_INVALID_SOCKET;
	    continue;
	}

	/*
	 * If we're configured to use STUN, then find out the mapped address,
	 * and make sure that the mapped RTCP port is adjacent with the RTP.
	 */
	if ((!use_ipv6 || use_nat64) &&
	    pjsua_media_acc_is_using_stun(call_med->call->acc_id) &&
	    pjsua_var.stun_srv.addr.sa_family != 0)
	{
	    char ip_addr[PJ_INET6_ADDRSTRLEN];
	    pj_str_t stun_srv;
	    pj_sockaddr_in resolved_addr[2];
	    pjstun_setting stun_opt;

	    pj_sockaddr_print(&pjsua_var.stun_srv, ip_addr,sizeof(ip_addr),0);
	    stun_srv = pj_str(ip_addr);

	    pj_bzero(&stun_opt, sizeof(stun_opt));
	    stun_opt.use_stun2 = pjsua_var.ua_cfg.stun_map_use_stun2;
	    stun_opt.af = pjsua_var.stun_srv.addr.sa_family;
	    stun_opt.srv1  = stun_opt.srv2  = stun_srv;
	    stun_opt.port1 = stun_opt.port2 = 
			     pj_sockaddr_get_port(&pjsua_var.stun_srv);
	    status=pjstun_get_mapped_addr2(&pjsua_var.cp.factory, &stun_opt,
					   2, sock, resolved_addr);
#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
	    /* Handle EPIPE (Broken Pipe) error, which happens on UDP socket
	     * after app wakes up from suspended state. In this case, simply
	     * just retry.
	     * P.S.: The magic status is PJ_STATUS_FROM_OS(EPIPE)
	     */
	    if (status == 120032) {
		PJ_LOG(4,(THIS_FILE, "Got EPIPE error, retrying.."));
		pj_sock_close(sock[0]);
		sock[0] = PJ_INVALID_SOCKET;

		pj_sock_close(sock[1]);
		sock[1] = PJ_INVALID_SOCKET;

		continue;
	    }
	    else
#endif

	    if (status != PJ_SUCCESS && pjsua_var.ua_cfg.stun_srv_cnt > 1 &&
	        ((acc->cfg.media_stun_use & PJSUA_STUN_RETRY_ON_FAILURE)==
		  PJSUA_STUN_RETRY_ON_FAILURE))
	    {
		pj_str_t srv = 
			     pjsua_var.ua_cfg.stun_srv[pjsua_var.stun_srv_idx];

		PJ_LOG(4,(THIS_FILE, "Failed to get STUN mapped address, "
		       "retrying other STUN servers"));

		if (pjsua_var.stun_srv_idx < pjsua_var.ua_cfg.stun_srv_cnt-1) {
		    PJSUA_LOCK();
		    /* Move the unusable STUN server to the last position
		     * as the least prioritize.
		     */
		    pj_array_erase(pjsua_var.ua_cfg.stun_srv, 
				   sizeof(pj_str_t),
				   pjsua_var.ua_cfg.stun_srv_cnt,
				   pjsua_var.stun_srv_idx);

		    pj_array_insert(pjsua_var.ua_cfg.stun_srv, 
				    sizeof(pj_str_t),
				    pjsua_var.ua_cfg.stun_srv_cnt-1,
				    pjsua_var.ua_cfg.stun_srv_cnt-1,
				    &srv);

		    PJSUA_UNLOCK();
		}
	    	status=pjsua_update_stun_servers(pjsua_var.ua_cfg.stun_srv_cnt,
	    			   	    	 pjsua_var.ua_cfg.stun_srv,
	    			   	    	 PJ_TRUE);
	    	if (status == PJ_SUCCESS) {
		    if (pjsua_var.stun_srv.addr.sa_family != 0) {
    			pj_sockaddr_print(&pjsua_var.stun_srv,
    		     		     	  ip_addr, sizeof(ip_addr), 0);
			stun_srv = pj_str(ip_addr);
	    	    } else {
		    	stun_srv.slen = 0;
    	            }
    	    
    	            stun_opt.af = pjsua_var.stun_srv.addr.sa_family;
    	            stun_opt.srv1  = stun_opt.srv2  = stun_srv;
	            stun_opt.port1 = stun_opt.port2 = 
			        pj_sockaddr_get_port(&pjsua_var.stun_srv);
	    	    status = pjstun_get_mapped_addr2(&pjsua_var.cp.factory,
	    				  	     &stun_opt, 2, sock,
	    				    	     resolved_addr);
	        }
	    }

	    if (status != PJ_SUCCESS) {
		if (!pjsua_var.ua_cfg.stun_ignore_failure) {
		    pjsua_perror(THIS_FILE, "STUN resolve error", status);
		    goto on_error;
		}

		PJ_LOG(4,(THIS_FILE, "Ignoring STUN resolve error %d", 
		          status));

		if (!pj_sockaddr_has_addr(&bound_addr)) {
		    pj_sockaddr addr;

		    /* Get local IP address. */
		    status = pj_gethostip(af, &addr);
		    if (status != PJ_SUCCESS)
			goto on_error;

		    pj_sockaddr_copy_addr(&bound_addr, &addr);
		}

		for (i=0; i<2; ++i) {
		    pj_sockaddr_init(af, &mapped_addr[i], NULL, 0);
		    pj_sockaddr_copy_addr(&mapped_addr[i], &bound_addr);
		    pj_sockaddr_set_port(&mapped_addr[i],
					 (pj_uint16_t)(acc->next_rtp_port+i));
		}
		break;
	    }

	    pj_sockaddr_cp(&mapped_addr[0], &resolved_addr[0]);
	    pj_sockaddr_cp(&mapped_addr[1], &resolved_addr[1]);

#if PJSUA_REQUIRE_CONSECUTIVE_RTCP_PORT
	    if (pj_sockaddr_get_port(&mapped_addr[1]) ==
		pj_sockaddr_get_port(&mapped_addr[0])+1)
	    {
		/* Success! */
		break;
	    }

	    pj_sock_close(sock[0]);
	    sock[0] = PJ_INVALID_SOCKET;

	    pj_sock_close(sock[1]);
	    sock[1] = PJ_INVALID_SOCKET;
#else
	    if (pj_sockaddr_get_port(&mapped_addr[1]) !=
		pj_sockaddr_get_port(&mapped_addr[0])+1)
	    {
		PJ_LOG(4,(THIS_FILE,
			  "Note: STUN mapped RTCP port %d is not adjacent"
			  " to RTP port %d",
			  pj_sockaddr_get_port(&mapped_addr[1]),
			  pj_sockaddr_get_port(&mapped_addr[0])));
	    }
	    /* Success! */
	    break;
#endif

	} else if (cfg->public_addr.slen) {

	    status = pj_sockaddr_init(af, &mapped_addr[0], &cfg->public_addr,
				      (pj_uint16_t)acc->next_rtp_port);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    status = pj_sockaddr_init(af, &mapped_addr[1], &cfg->public_addr,
				      (pj_uint16_t)(acc->next_rtp_port+1));
	    if (status != PJ_SUCCESS)
		goto on_error;

	    break;

	} else {
	    if (acc->cfg.allow_sdp_nat_rewrite && acc->reg_mapped_addr.slen) {
		pj_status_t status2;

		/* Take the address from mapped addr as seen by registrar */
		status2 = pj_sockaddr_set_str_addr(af, &bound_addr,
		                                   &acc->reg_mapped_addr);
		if (status2 != PJ_SUCCESS) {
		    /* just leave bound_addr with whatever it was
		    pj_bzero(pj_sockaddr_get_addr(&bound_addr),
		             pj_sockaddr_get_addr_len(&bound_addr));
		     */
		}
	    }

	    if (!pj_sockaddr_has_addr(&bound_addr)) {
		pj_sockaddr addr;

		/* Get local IP address. */
		status = pj_gethostip(af, &addr);
		if (status != PJ_SUCCESS)
		    goto on_error;

		pj_sockaddr_copy_addr(&bound_addr, &addr);
	    }

	    for (i=0; i<2; ++i) {
		pj_sockaddr_init(af, &mapped_addr[i], NULL, 0);
		pj_sockaddr_copy_addr(&mapped_addr[i], &bound_addr);
		pj_sockaddr_set_port(&mapped_addr[i],
		                     (pj_uint16_t)(acc->next_rtp_port+i));
	    }

	    break;
	}
    }

    if (sock[0] == PJ_INVALID_SOCKET) {
	PJ_LOG(1,(THIS_FILE,
		  "Unable to find appropriate RTP/RTCP ports combination"));
	goto on_error;
    }


    skinfo->rtp_sock = sock[0];
    pj_sockaddr_cp(&skinfo->rtp_addr_name, &mapped_addr[0]);

    skinfo->rtcp_sock = sock[1];
    pj_sockaddr_cp(&skinfo->rtcp_addr_name, &mapped_addr[1]);

    PJ_LOG(4,(THIS_FILE, "RTP%s socket reachable at %s",
	      (call_med->enable_rtcp_mux? " & RTCP": ""),
	      pj_sockaddr_print(&skinfo->rtp_addr_name, addr_buf,
				sizeof(addr_buf), 3)));
    PJ_LOG(4,(THIS_FILE, "RTCP socket reachable at %s",
	      pj_sockaddr_print(&skinfo->rtcp_addr_name, addr_buf,
				sizeof(addr_buf), 3)));

    acc->next_rtp_port += 2;
    return PJ_SUCCESS;

on_error:
    for (i=0; i<2; ++i) {
	if (sock[i] != PJ_INVALID_SOCKET)
	    pj_sock_close(sock[i]);
    }
    return status;
}

/* Create normal UDP media transports */
static pj_status_t create_udp_media_transport(const pjsua_transport_config *cfg,
					      pjsua_call_media *call_med)
{
    pjmedia_sock_info skinfo;
    pj_status_t status;

    status = create_rtp_rtcp_sock(call_med, cfg, &skinfo);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create RTP/RTCP socket",
		     status);
	goto on_error;
    }

    status = pjmedia_transport_udp_attach(pjsua_var.med_endpt, NULL,
					  &skinfo, 0, &call_med->tp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create media transport",
		     status);
	goto on_error;
    }

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_ENCODING,
				    pjsua_var.media_cfg.tx_drop_pct);

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_DECODING,
				    pjsua_var.media_cfg.rx_drop_pct);

    call_med->tp_ready = PJ_SUCCESS;

    return PJ_SUCCESS;

on_error:
    if (call_med->tp)
	pjmedia_transport_close(call_med->tp);

    return status;
}

/* Create loop media transport */
static pj_status_t create_loop_media_transport(
		       const pjsua_transport_config *cfg,
		       pjsua_call_media *call_med)
{
    pj_status_t status;
    pjmedia_loop_tp_setting opt;
    pj_bool_t use_ipv6, use_nat64;
    int af;
    pjsua_acc *acc = &pjsua_var.acc[call_med->call->acc_id];

    use_ipv6 = (acc->cfg.ipv6_media_use != PJSUA_IPV6_DISABLED);
    use_nat64 = (acc->cfg.nat64_opt != PJSUA_NAT64_DISABLED);
    af = (use_ipv6 || use_nat64) ? pj_AF_INET6() : pj_AF_INET();

    pjmedia_loop_tp_setting_default(&opt);
    opt.af = af;
    if (cfg->bound_addr.slen)
        opt.addr = cfg->bound_addr;

    if (acc->next_rtp_port == 0 || cfg->port == 0)
	acc->next_rtp_port = (pj_uint16_t)cfg->port;

    if (cfg->port > 0 && cfg->port_range > 0 &&
        (acc->next_rtp_port > cfg->port + cfg->port_range ||
         acc->next_rtp_port < cfg->port))
    {
        acc->next_rtp_port = (pj_uint16_t)cfg->port;
    }
    opt.port = acc->next_rtp_port;
    acc->next_rtp_port += 2;

    opt.disable_rx=!pjsua_var.acc[call_med->call->acc_id].cfg.enable_loopback;
    status = pjmedia_transport_loop_create2(pjsua_var.med_endpt, &opt,
    					    &call_med->tp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create loop media transport",
		     status);
	goto on_error;
    }

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_ENCODING,
				    pjsua_var.media_cfg.tx_drop_pct);

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_DECODING,
				    pjsua_var.media_cfg.rx_drop_pct);

    call_med->tp_ready = PJ_SUCCESS;

    return PJ_SUCCESS;

on_error:
    if (call_med->tp)
	pjmedia_transport_close(call_med->tp);

    return status;
}

#if DISABLED_FOR_TICKET_1185
/* Create normal UDP media transports */
static pj_status_t create_udp_media_transports(pjsua_transport_config *cfg)
{
    unsigned i;
    pj_status_t status;

    for (i=0; i < pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    status = create_udp_media_transport(cfg, &call_med->tp);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
    }

    return PJ_SUCCESS;

on_error:
    for (i=0; i < pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    if (call_med->tp) {
		pjmedia_transport_close(call_med->tp);
		call_med->tp = NULL;
	    }
	}
    }
    return status;
}
#endif

/* Deferred callback to notify ICE init complete */
static void ice_init_complete_cb(void *user_data)
{
    pjsua_call_media *call_med = (pjsua_call_media*)user_data;

    if (call_med->call == NULL || call_med->tp_ready == PJ_SUCCESS)
	return;

    /* No need to acquire_call() if we only change the tp_ready flag
     * (i.e. transport is being created synchronously). Otherwise
     * calling acquire_call() here may cause deadlock. See
     * https://trac.pjsip.org/repos/ticket/1578
     */
    call_med->tp_ready = call_med->tp_result;

    if (call_med->med_create_cb) {
	pjsua_call *call = NULL;
	pjsip_dialog *dlg = NULL;
	pj_status_t status;

	status = acquire_call("ice_init_complete_cb", call_med->call->index,
			      &call, &dlg);
	if (status != PJ_SUCCESS) {
	    if (status != PJSIP_ESESSIONTERMINATED) {
		/* Retry, if call is still active */
		pjsua_schedule_timer2(&ice_init_complete_cb, call_med, 10);
	    }
	    return;
	}

        (*call_med->med_create_cb)(call_med, call_med->tp_ready,
                                   call_med->call->secure_level, NULL);

        if (dlg)
            pjsip_dlg_dec_lock(dlg);
    }
}

/* Deferred callback to notify ICE negotiation failure */
static void ice_failed_nego_cb(void *user_data)
{
    int call_id = (int)(pj_ssize_t)user_data;
    pjsua_call *call = NULL;
    pjsip_dialog *dlg = NULL;
    pj_status_t status;

    status = acquire_call("ice_failed_nego_cb", call_id, &call, &dlg);
    if (status != PJ_SUCCESS) {
	if (status != PJSIP_ESESSIONTERMINATED) {
	    /* Retry, if call is still active */
	    pjsua_schedule_timer2(&ice_failed_nego_cb,
				  (void*)(pj_ssize_t)call_id, 10);
	}
	return;
    }

    if (!call->hanging_up)
    	pjsua_var.ua_cfg.cb.on_call_media_state(call_id);

    if (dlg)
        pjsip_dlg_dec_lock(dlg);

}

/* This callback is called when ICE negotiation completes */
static void on_ice_complete(pjmedia_transport *tp, 
			    pj_ice_strans_op op,
			    pj_status_t result)
{
    pjsua_call_media *call_med = (pjsua_call_media*)tp->user_data;
    pjsua_call *call;

    if (!call_med)
	return;

    call = call_med->call;
    
    switch (op) {
    case PJ_ICE_STRANS_OP_INIT:
        call_med->tp_result = result;
        pjsua_schedule_timer2(&ice_init_complete_cb, call_med, 1);
	break;
    case PJ_ICE_STRANS_OP_NEGOTIATION:
	if (result == PJ_SUCCESS) {
            /* Update RTP address */
            pjmedia_transport_info tpinfo;
            pjmedia_transport_info_init(&tpinfo);
            pjmedia_transport_get_info(call_med->tp, &tpinfo);
            pj_sockaddr_cp(&call_med->rtp_addr, &tpinfo.sock_info.rtp_addr_name);
        } else {
	    call_med->state = PJSUA_CALL_MEDIA_ERROR;
	    call_med->dir = PJMEDIA_DIR_NONE;
	    if (call && !call->hanging_up &&
	        pjsua_var.ua_cfg.cb.on_call_media_state)
	    {
		/* Defer the callback to a timer */
		pjsua_schedule_timer2(&ice_failed_nego_cb,
				      (void*)(pj_ssize_t)call->index, 1);
	    }
        }

	/* Stop trickling */
	if (call->trickle_ice.trickling < PJSUA_OP_STATE_DONE) {
	    call->trickle_ice.trickling = PJSUA_OP_STATE_DONE;
	    pjsua_cancel_timer(&call->trickle_ice.timer);
	    PJ_LOG(4,(THIS_FILE, "Call %d: ICE trickle stopped trickling as "
		      "ICE nego completed",
		      call->index));
	}

	/* Check if default ICE transport address is changed */
        call->reinv_ice_sent = PJ_FALSE;
	pjsua_call_schedule_reinvite_check(call, 0);
	break;
    case PJ_ICE_STRANS_OP_KEEP_ALIVE:
    case PJ_ICE_STRANS_OP_ADDR_CHANGE:
	if (result != PJ_SUCCESS) {
	    PJ_PERROR(4,(THIS_FILE, result,
		         "ICE keep alive failure for transport %d:%d",
		         call->index, call_med->idx));
	}
        if (!call->hanging_up &&
            pjsua_var.ua_cfg.cb.on_call_media_transport_state)
        {
            pjsua_med_tp_state_info info;

            pj_bzero(&info, sizeof(info));
            info.med_idx = call_med->idx;
            info.state = call_med->tp_st;
            info.status = result;
            info.ext_info = &op;
	    (*pjsua_var.ua_cfg.cb.on_call_media_transport_state)(
                call->index, &info);
        }
	if (pjsua_var.ua_cfg.cb.on_ice_transport_error &&
	    op == PJ_ICE_STRANS_OP_KEEP_ALIVE)
	{
	    pjsua_call_id id = call->index;
	    (*pjsua_var.ua_cfg.cb.on_ice_transport_error)(id, op, result,
							  NULL);
	}
	break;
    }
}


/* Parse "HOST:PORT" format */
static pj_status_t parse_host_port(const pj_str_t *host_port,
				   pj_str_t *host, pj_uint16_t *port)
{
    pj_str_t str_port;

    str_port.ptr = pj_strchr(host_port, ':');
    if (str_port.ptr != NULL) {
	int iport;

	host->ptr = host_port->ptr;
	host->slen = (str_port.ptr - host->ptr);
	str_port.ptr++;
	str_port.slen = host_port->slen - host->slen - 1;
	iport = (int)pj_strtoul(&str_port);
	if (iport < 1 || iport > 65535)
	    return PJ_EINVAL;
	*port = (pj_uint16_t)iport;
    } else {
	*host = *host_port;
	*port = 0;
    }

    return PJ_SUCCESS;
}

/* Create ICE media transports (when ice is enabled) */
static pj_status_t create_ice_media_transport(
				const pjsua_transport_config *cfg,
				pjsua_call_media *call_med,
                                pj_bool_t async)
{
    char stunip[PJ_INET6_ADDRSTRLEN];
    pjsua_acc_config *acc_cfg;
    pj_ice_strans_cfg ice_cfg;
    pjmedia_ice_cb ice_cb;
    char name[32];
    unsigned comp_cnt;
    pj_status_t status;
    pj_bool_t use_ipv6, use_nat64;
    pj_bool_t trickle = PJ_FALSE;
    pjmedia_sdp_session *rem_sdp;

    acc_cfg = &pjsua_var.acc[call_med->call->acc_id].cfg;
    use_ipv6 = (acc_cfg->ipv6_media_use != PJSUA_IPV6_DISABLED);
    use_nat64 = (acc_cfg->nat64_opt != PJSUA_NAT64_DISABLED);

    /* Make sure STUN server resolution has completed */
    if (pjsua_media_acc_is_using_stun(call_med->call->acc_id)) {
	pj_bool_t retry_stun = (acc_cfg->media_stun_use &
				PJSUA_STUN_RETRY_ON_FAILURE) ==
				PJSUA_STUN_RETRY_ON_FAILURE;
	status = resolve_stun_server(PJ_TRUE, retry_stun,
				     (unsigned)acc_cfg->nat64_opt);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	    return status;
	}
    }

    /* Create ICE stream transport configuration */
    pj_ice_strans_cfg_default(&ice_cfg);
    pj_bzero(&ice_cfg.stun, sizeof(ice_cfg.stun));
    pj_bzero(&ice_cfg.turn, sizeof(ice_cfg.turn));
    pj_stun_config_init(&ice_cfg.stun_cfg, &pjsua_var.cp.factory, 0,
		        pjsip_endpt_get_ioqueue(pjsua_var.endpt),
			pjsip_endpt_get_timer_heap(pjsua_var.endpt));
    
    ice_cfg.resolver = pjsua_var.resolver;
    
    ice_cfg.opt = acc_cfg->ice_cfg.ice_opt;
    rem_sdp = call_med->call->async_call.rem_sdp;

    if (rem_sdp) {
    	/* Match the default address family according to the offer */
        const pj_str_t ID_IP6 = { "IP6", 3};
    	const pjmedia_sdp_media *m;
	const pjmedia_sdp_conn *c;

	m = rem_sdp->media[call_med->idx];
	c = m->conn? m->conn : rem_sdp->conn;

	if (pj_stricmp(&c->addr_type, &ID_IP6) == 0)
	    ice_cfg.af = pj_AF_INET6();
    } else if (use_ipv6 || use_nat64) {
    	ice_cfg.af = pj_AF_INET6();
    }

    /* Should not wait for ICE STUN/TURN ready when trickle ICE is enabled */
    if (ice_cfg.opt.trickle != PJ_ICE_SESS_TRICKLE_DISABLED &&
	(call_med->call->inv == NULL || 
	 call_med->call->inv->state < PJSIP_INV_STATE_CONFIRMED))
    {
	if (rem_sdp) {
	    /* As answerer: and when remote signals trickle ICE in SDP */
	    trickle = pjmedia_ice_sdp_has_trickle(rem_sdp, call_med->idx);
	    if (trickle) {
		call_med->call->trickle_ice.remote_sup = PJ_TRUE;
		call_med->call->trickle_ice.enabled = PJ_TRUE;
	    }
	} else {
	    /* As offerer: and when trickle ICE mode is full */
	    trickle = (ice_cfg.opt.trickle==PJ_ICE_SESS_TRICKLE_FULL);
	    call_med->call->trickle_ice.enabled = PJ_TRUE;
	}

	/* Check if trickle ICE can start trickling/sending SIP INFO */
	pjsua_ice_check_start_trickling(call_med->call, PJ_FALSE, NULL);
    } else {
	/* For non-initial INVITE, always use regular ICE */
	ice_cfg.opt.trickle = PJ_ICE_SESS_TRICKLE_DISABLED;
    }

    /* If STUN transport is configured, initialize STUN transport settings */
    if ((pj_sockaddr_has_addr(&pjsua_var.stun_srv) &&
	 pjsua_media_acc_is_using_stun(call_med->call->acc_id)) ||
	acc_cfg->ice_cfg.ice_max_host_cands != 0)
    {
	ice_cfg.stun_tp_cnt = 1;
	pj_ice_strans_stun_cfg_default(&ice_cfg.stun_tp[0]);
	if (use_nat64) {
	    ice_cfg.stun_tp[0].af = pj_AF_INET6();
	} else if (use_ipv6 && PJ_ICE_MAX_STUN >= 2) {
	    ice_cfg.stun_tp_cnt = 2;
	    pj_ice_strans_stun_cfg_default(&ice_cfg.stun_tp[1]);
	    ice_cfg.stun_tp[1].af = pj_AF_INET6();
	}
    }

    /* Configure STUN transport settings */
    if (ice_cfg.stun_tp_cnt) {
	unsigned i;

	if (pj_sockaddr_has_addr(&pjsua_var.stun_srv)) {
	    pj_sockaddr_print(&pjsua_var.stun_srv, stunip,
			      sizeof(stunip), 0);
	}

	for (i = 0; i < ice_cfg.stun_tp_cnt; ++i) {
	    pj_str_t IN6_ADDR_ANY = {"0", 1};

	    /* Configure STUN server */
	    if (pjsua_media_acc_is_using_stun(call_med->call->acc_id) &&
		pj_sockaddr_has_addr(&pjsua_var.stun_srv) &&
		pjsua_var.stun_srv.addr.sa_family == ice_cfg.stun_tp[i].af)
	    {
	    	ice_cfg.stun_tp[i].server = pj_str(stunip);
	    	ice_cfg.stun_tp[i].port = pj_sockaddr_get_port(
	    				      &pjsua_var.stun_srv);
	    }

	    /* Configure max host candidates */
	    if (acc_cfg->ice_cfg.ice_max_host_cands >= 0) {
		ice_cfg.stun_tp[i].max_host_cands =
				acc_cfg->ice_cfg.ice_max_host_cands;
	    }

	    /* Configure binding address */
	    pj_sockaddr_init(ice_cfg.stun_tp[i].af,
			     &ice_cfg.stun_tp[i].cfg.bound_addr,
			     (ice_cfg.stun_tp[i].af == pj_AF_INET()?
			     &cfg->bound_addr: &IN6_ADDR_ANY),
			     (pj_uint16_t)cfg->port);
	    ice_cfg.stun_tp[i].cfg.port_range = (pj_uint16_t)cfg->port_range;
	    if (cfg->port != 0 && ice_cfg.stun_tp[i].cfg.port_range == 0) {
	    	ice_cfg.stun_tp[i].cfg.port_range = 
			    (pj_uint16_t)(pjsua_var.ua_cfg.max_calls * 10);
	    }

	    /* Configure QoS setting */
	    ice_cfg.stun_tp[i].cfg.qos_type = cfg->qos_type;
	    pj_memcpy(&ice_cfg.stun_tp[i].cfg.qos_params, &cfg->qos_params,
		      sizeof(cfg->qos_params));

	    /* Configure max packet size */
	    ice_cfg.stun_tp[i].cfg.max_pkt_size = PJMEDIA_MAX_MRU;
	}
    }

    /* Configure TURN settings */
    if (acc_cfg->turn_cfg.enable_turn) {
        unsigned i, idx = 0;
        
        if (use_ipv6 && !use_nat64 && PJ_ICE_MAX_TURN >= 3) {
            ice_cfg.turn_tp_cnt = 3;
            idx = 1;
        } else {
	    ice_cfg.turn_tp_cnt = 1;
	}
	
	for (i = 0; i < ice_cfg.turn_tp_cnt; i++)
	    pj_ice_strans_turn_cfg_default(&ice_cfg.turn_tp[i]);

	if (use_ipv6 || use_nat64) {
	    if (!use_nat64)
	        ice_cfg.turn_tp[idx++].af = pj_AF_INET6();

	    /* Additional candidate: IPv4 relay via IPv6 TURN server */
	    ice_cfg.turn_tp[idx].af = pj_AF_INET6();
	    ice_cfg.turn_tp[idx].alloc_param.af = pj_AF_INET();
	}

	/* Configure TURN server */
	status = parse_host_port(&acc_cfg->turn_cfg.turn_server,
				 &ice_cfg.turn_tp[0].server,
				 &ice_cfg.turn_tp[0].port);
	if (status != PJ_SUCCESS || ice_cfg.turn_tp[0].server.slen == 0) {
	    PJ_LOG(1,(THIS_FILE, "Invalid TURN server setting"));
	    return PJ_EINVAL;
	}

	if (ice_cfg.turn_tp[0].port == 0)
	    ice_cfg.turn_tp[0].port = 3479;

	for (i = 0; i < ice_cfg.turn_tp_cnt; i++) {
	    pj_str_t IN6_ADDR_ANY = {"0", 1};

	    /* Configure TURN connection settings and credential */
	    ice_cfg.turn_tp[i].server    = ice_cfg.turn_tp[0].server;
	    ice_cfg.turn_tp[i].port      = ice_cfg.turn_tp[0].port;
	    ice_cfg.turn_tp[i].conn_type = acc_cfg->turn_cfg.turn_conn_type;
	    pj_memcpy(&ice_cfg.turn_tp[i].auth_cred, 
		      &acc_cfg->turn_cfg.turn_auth_cred,
		      sizeof(ice_cfg.turn_tp[i].auth_cred));

	    /* Configure QoS setting */
	    ice_cfg.turn_tp[i].cfg.qos_type = cfg->qos_type;
	    pj_memcpy(&ice_cfg.turn_tp[i].cfg.qos_params, &cfg->qos_params,
		      sizeof(cfg->qos_params));

	    /* Configure binding address */
	    pj_sockaddr_init(ice_cfg.turn_tp[i].af,
	    		     &ice_cfg.turn_tp[i].cfg.bound_addr,
			     (ice_cfg.turn_tp[i].af == pj_AF_INET()?
			     &cfg->bound_addr: &IN6_ADDR_ANY),
			     (pj_uint16_t)cfg->port);
	    ice_cfg.turn_tp[i].cfg.port_range = (pj_uint16_t)cfg->port_range;
	    if (cfg->port != 0 && ice_cfg.turn_tp[i].cfg.port_range == 0)
	        ice_cfg.turn_tp[i].cfg.port_range = 
				 (pj_uint16_t)(pjsua_var.ua_cfg.max_calls * 10);

	    /* Configure max packet size */
	    ice_cfg.turn_tp[i].cfg.max_pkt_size = PJMEDIA_MAX_MRU;

#if PJ_HAS_SSL_SOCK
	    if (ice_cfg.turn_tp[i].conn_type == PJ_TURN_TP_TLS) {
		pj_memcpy(&ice_cfg.turn_tp[i].cfg.tls_cfg, 
			  &acc_cfg->turn_cfg.turn_tls_setting,
			  sizeof(ice_cfg.turn_tp[i].cfg.tls_cfg));
	    }
#endif
	}
    }

    pj_bzero(&ice_cb, sizeof(pjmedia_ice_cb));
    ice_cb.on_ice_complete = &on_ice_complete;
    pj_ansi_snprintf(name, sizeof(name), "icetp%02d", call_med->idx);
    call_med->tp_ready = trickle? PJ_SUCCESS : PJ_EPENDING;

    comp_cnt = 1;
    if (PJMEDIA_ADVERTISE_RTCP && !acc_cfg->ice_cfg.ice_no_rtcp)
	++comp_cnt;

    status = pjmedia_ice_create3(pjsua_var.med_endpt, name, comp_cnt,
				 &ice_cfg, &ice_cb, PJSUA_ICE_TRANSPORT_OPTION,
                                 call_med, &call_med->tp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create ICE media transport",
		     status);
	goto on_error;
    }

    /* Wait until transport is initialized, or time out */
    if (!async && !trickle) {
	pj_bool_t has_pjsua_lock = PJSUA_LOCK_IS_LOCKED();
	pjsip_dialog *dlg = call_med->call->inv ?
				call_med->call->inv->dlg : NULL;
        if (has_pjsua_lock)
	    PJSUA_UNLOCK();
        if (dlg) {
            /* Don't lock otherwise deadlock:
             * https://trac.pjsip.org/repos/ticket/1737
             */
	    pjsip_dlg_inc_session(dlg, &pjsua_var.mod);
            pjsip_dlg_dec_lock(dlg);
        }
        while (call_med->tp_ready == PJ_EPENDING) {
	    pjsua_handle_events(100);
        }
        if (dlg) {
            pjsip_dlg_inc_lock(dlg);
	    pjsip_dlg_dec_session(dlg, &pjsua_var.mod);
        }
	if (has_pjsua_lock)
	    PJSUA_LOCK();
    }

    if (!call_med->tp) {
	/* Call has been disconnected, and media transports have been cleared
	 * (see ticket #1759).
	 */
	PJ_LOG(4,(THIS_FILE, "Media transport initialization cancelled "
		             "because call has been disconnected"));
	status = PJ_ECANCELLED;
	goto on_error;
    } else if (async && call_med->tp_ready == PJ_EPENDING) {
        return PJ_EPENDING;
    } else if (call_med->tp_ready != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing ICE media transport",
		     call_med->tp_ready);
	status = call_med->tp_ready;
	goto on_error;
    }

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_ENCODING,
				    pjsua_var.media_cfg.tx_drop_pct);

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_DECODING,
				    pjsua_var.media_cfg.rx_drop_pct);
    
    return PJ_SUCCESS;

on_error:
    if (call_med->tp != NULL) {
	pjmedia_transport_close(call_med->tp);
	call_med->tp = NULL;
    }

    return status;
}

#if DISABLED_FOR_TICKET_1185
/* Create ICE media transports (when ice is enabled) */
static pj_status_t create_ice_media_transports(pjsua_transport_config *cfg)
{
    unsigned i;
    pj_status_t status;

    for (i=0; i < pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    status = create_ice_media_transport(cfg, call_med);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
    }

    return PJ_SUCCESS;

on_error:
    for (i=0; i < pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    if (call_med->tp) {
		pjmedia_transport_close(call_med->tp);
		call_med->tp = NULL;
	    }
	}
    }
    return status;
}
#endif

#if DISABLED_FOR_TICKET_1185
/*
 * Create media transports for all the calls. This function creates
 * one UDP media transport for each call.
 */
PJ_DEF(pj_status_t) pjsua_media_transports_create(
			const pjsua_transport_config *app_cfg)
{
    pjsua_transport_config cfg;
    unsigned i;
    pj_status_t status;


    /* Make sure pjsua_init() has been called */
    PJ_ASSERT_RETURN(pjsua_var.ua_cfg.max_calls>0, PJ_EINVALIDOP);

    PJSUA_LOCK();

    /* Delete existing media transports */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    if (call_med->tp && call_med->tp_auto_del) {
		pjmedia_transport_close(call_med->tp);
		call_med->tp = NULL;
		call_med->tp_orig = NULL;
	    }
	}
    }

    /* Copy config */
    pjsua_transport_config_dup(pjsua_var.pool, &cfg, app_cfg);

    /* Create the transports */
    if (pjsua_var.ice_cfg.enable_ice) {
	status = create_ice_media_transports(&cfg);
    } else {
	status = create_udp_media_transports(&cfg);
    }

    /* Set media transport auto_delete to True */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    call_med->tp_auto_del = PJ_TRUE;
	}
    }

    PJSUA_UNLOCK();

    return status;
}

/*
 * Attach application's created media transports.
 */
PJ_DEF(pj_status_t) pjsua_media_transports_attach(pjsua_media_transport tp[],
						  unsigned count,
						  pj_bool_t auto_delete)
{
    unsigned i;

    PJ_ASSERT_RETURN(tp && count==pjsua_var.ua_cfg.max_calls, PJ_EINVAL);

    /* Assign the media transports */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    if (call_med->tp && call_med->tp_auto_del) {
		pjmedia_transport_close(call_med->tp);
		call_med->tp = NULL;
		call_med->tp_orig = NULL;
	    }
	}

	PJ_TODO(remove_pjsua_media_transports_attach);

	call->media[0].tp = tp[i].transport;
	call->media[0].tp_auto_del = auto_delete;
    }

    return PJ_SUCCESS;
}
#endif

/* Go through the list of media in the SDP, find acceptable media, and
 * sort them based on the below criteria, and store the indexes
 * in the specified array. The criteria is as follows:
 * 1. enabled, i.e: SDP media port not zero
 * 2. transport protocol in the SDP matching account config's
 *    secure media transport usage (\a use_srtp field).
 * 3. active, i.e: SDP media direction is not "inactive"
 * 4. media order (according to the SDP).
 */
static void sort_media(const pjmedia_sdp_session *sdp,
		       const pj_str_t *type,
		       pjmedia_srtp_use	use_srtp,
		       pj_uint8_t midx[],
		       unsigned *p_count,
		       unsigned *p_total_count)
{
    unsigned i;
    unsigned count = 0;
    int score[PJSUA_MAX_CALL_MEDIA];

    pj_assert(*p_count >= PJSUA_MAX_CALL_MEDIA);
    pj_assert(*p_total_count >= PJSUA_MAX_CALL_MEDIA);

    *p_count = 0;
    *p_total_count = 0;
    for (i=0; i<PJSUA_MAX_CALL_MEDIA; ++i)
	score[i] = 1;

    /* Score each media */
    for (i=0; i<sdp->media_count && count<PJSUA_MAX_CALL_MEDIA; ++i) {
	const pjmedia_sdp_media *m = sdp->media[i];
	const pjmedia_sdp_conn *c;
	pj_uint32_t proto;

	/* Skip different media */
	if (pj_stricmp(&m->desc.media, type) != 0) {
	    score[count++] = -22000;
	    continue;
	}

	c = m->conn? m->conn : sdp->conn;

	/* Supported transports */
	proto = pjmedia_sdp_transport_get_proto(&m->desc.transport);
	if (PJMEDIA_TP_PROTO_HAS_FLAG(proto, PJMEDIA_TP_PROTO_RTP_SAVP))
	{
	    switch (use_srtp) {
	    case PJMEDIA_SRTP_MANDATORY:
	    case PJMEDIA_SRTP_OPTIONAL:
		++score[i];
		break;
	    case PJMEDIA_SRTP_DISABLED:
		//--score[i];
		score[i] -= 5;
		break;
	    }
	} else if (PJMEDIA_TP_PROTO_HAS_FLAG(proto, PJMEDIA_TP_PROTO_RTP_AVP))
	{
	    switch (use_srtp) {
	    case PJMEDIA_SRTP_MANDATORY:
		//--score[i];
		score[i] -= 5;
		break;
	    case PJMEDIA_SRTP_OPTIONAL:
		/* No change in score */
		break;
	    case PJMEDIA_SRTP_DISABLED:
		++score[i];
		break;
	    }
	} else {
	    score[i] -= 10;
	}

	/* Is media disabled? */
	if (m->desc.port == 0)
	    score[i] -= 10;

	/* Is media inactive? */
	if (pjmedia_sdp_media_find_attr2(m, "inactive", NULL) ||
	    pj_strcmp2(&c->addr, "0.0.0.0") == 0)
	{
	    //score[i] -= 10;
	    score[i] -= 1;
	}

	++count;
    }

    /* Created sorted list based on quality */
    for (i=0; i<count; ++i) {
	unsigned j;
	int best = 0;

	for (j=1; j<count; ++j) {
	    if (score[j] > score[best])
		best = j;
	}
	/* Don't put media with negative score, that media is unacceptable
	 * for us.
	 */
	midx[i] = (pj_uint8_t)best;
	if (score[best] >= 0)
	    (*p_count)++;
	if (score[best] > -22000)
	    (*p_total_count)++;

	score[best] = -22000;

    }
}


/* Go through the list of media in the call, find acceptable media, and
 * sort them based on the "quality" of the media, and store the indexes
 * in the specified array. Media with the best quality will be listed
 * first in the array.
 */
static void sort_media2(const pjsua_call_media *call_med,
			pj_bool_t check_tp,
			unsigned call_med_cnt,
			pjmedia_type type,
			pj_uint8_t midx[],
			unsigned *p_count,
			unsigned *p_total_count)
{
    unsigned i;
    unsigned count = 0;
    int score[PJSUA_MAX_CALL_MEDIA];

    pj_assert(*p_count >= PJSUA_MAX_CALL_MEDIA);
    pj_assert(*p_total_count >= PJSUA_MAX_CALL_MEDIA);

    *p_count = 0;
    *p_total_count = 0;
    for (i=0; i<PJSUA_MAX_CALL_MEDIA; ++i)
	score[i] = 1;

    /* Score each media */
    for (i=0; i<call_med_cnt && count<PJSUA_MAX_CALL_MEDIA; ++i) {

	/* Skip different media */
	if (call_med[i].type != type) {
	    score[count++] = -22000;
	    continue;
	}

	/* Is it active? */
	if (check_tp && !call_med[i].tp) {
	    score[i] -= 10;
	}

	++count;
    }

    /* Created sorted list based on quality */
    for (i=0; i<count; ++i) {
	unsigned j;
	int best = 0;

	for (j=1; j<count; ++j) {
	    if (score[j] > score[best])
		best = j;
	}
	/* Don't put media with negative score, that media is unacceptable
	 * for us.
	 */
	midx[i] = (pj_uint8_t)best;
	if (score[best] >= 0)
	    (*p_count)++;
	if (score[best] > -22000)
	    (*p_total_count)++;

	score[best] = -22000;

    }
}

/* Callback to receive global media events */
pj_status_t on_media_event(pjmedia_event *event, void *user_data)
{
    char ev_name[5];
    pj_status_t status = PJ_SUCCESS;

    PJ_UNUSED_ARG(user_data);

    pjmedia_fourcc_name(event->type, ev_name);
    PJ_LOG(4,(THIS_FILE, "Received media event type=%s, src=%p, epub=%p",
			 ev_name, event->src, event->epub));

    /* Forward the event */
    if (pjsua_var.ua_cfg.cb.on_media_event) {
	(*pjsua_var.ua_cfg.cb.on_media_event)(event);
    }

    return status;
}

/* Call on_call_media_event() callback using timer */
void call_med_event_cb(void *user_data)
{
    pjsua_event_list *eve = (pjsua_event_list *)user_data;
    
    (*pjsua_var.ua_cfg.cb.on_call_media_event)(eve->call_id,
					       eve->med_idx,
					       &eve->event);

    pj_mutex_lock(pjsua_var.timer_mutex);
    pj_list_push_back(&pjsua_var.event_list, eve);
    pj_mutex_unlock(pjsua_var.timer_mutex);
}

/* Callback to receive media events of a call */
pj_status_t call_media_on_event(pjmedia_event *event,
                                void *user_data)
{
    pjsua_call_media *call_med = (pjsua_call_media*)user_data;
    pjsua_call *call = call_med? call_med->call : NULL;
    char ev_name[5];
    pj_status_t status = PJ_SUCCESS;

    pjmedia_fourcc_name(event->type, ev_name);
    PJ_LOG(5,(THIS_FILE, "Call %d: Media %d: Received media event, type=%s, "
			 "src=%p, epub=%p",
			 call->index, call_med->idx, ev_name,
			 event->src, event->epub));

    switch(event->type) {
	case PJMEDIA_EVENT_KEYFRAME_MISSING:
	    if (call->opt.req_keyframe_method & PJSUA_VID_REQ_KEYFRAME_SIP_INFO)
	    {
		pj_timestamp now;

		pj_get_timestamp(&now);
		if (pj_elapsed_msec(&call_med->last_req_keyframe, &now) >=
		    PJSUA_VID_REQ_KEYFRAME_INTERVAL)
		{
		    pjsua_msg_data msg_data;
		    const pj_str_t SIP_INFO = {"INFO", 4};
		    const char *BODY_TYPE = "application/media_control+xml";
		    const char *BODY =
			"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
			"<media_control><vc_primitive><to_encoder>"
			"<picture_fast_update/>"
			"</to_encoder></vc_primitive></media_control>";

		    PJ_LOG(4,(THIS_FILE, 
			      "Sending video keyframe request via SIP INFO"));

		    pjsua_msg_data_init(&msg_data);
		    pj_cstr(&msg_data.content_type, BODY_TYPE);
		    pj_cstr(&msg_data.msg_body, BODY);
		    status = pjsua_call_send_request(call->index, &SIP_INFO, 
						     &msg_data);
		    if (status != PJ_SUCCESS) {
			PJ_PERROR(3,(THIS_FILE, status,
				  "Failed requesting keyframe via SIP INFO"));
		    } else {
			call_med->last_req_keyframe = now;
		    }
		}
	    }
	    break;

#if PJSUA_HAS_VIDEO
	case PJMEDIA_EVENT_FMT_CHANGED:
	    if (call_med->strm.v.rdr_win_id != PJSUA_INVALID_ID) {
		pjsua_vid_win *w = &pjsua_var.win[call_med->strm.v.rdr_win_id];
		if (event->epub == w->vp_rend) {
		    /* Renderer just changed format, update its
		     * conference bridge port.
		     */
		    pjsua_vid_conf_update_port(w->rend_slot);
		}
	    }

	    if (call_med->strm.v.strm_dec_slot != PJSUA_INVALID_ID) {
		/* Stream decoder changed format, update all conf listeners
		 * by reconnecting.
		 */
		pjmedia_port *strm_dec;

		status = pjmedia_vid_stream_get_port(call_med->strm.v.stream,
						     PJMEDIA_DIR_DECODING,
						     &strm_dec);
		if (status != PJ_SUCCESS)
		    break;

		/* Verify that the publisher is the stream decoder */
		if (event->epub != strm_dec)
		    break;

		/* Stream decoder just changed format, update its
		 * conference bridge port.
		 */
		pjsua_vid_conf_update_port(call_med->strm.v.strm_dec_slot);
	    }
	    break;

	case PJMEDIA_EVENT_VID_DEV_ERROR:
	    {
		PJ_PERROR(3,(THIS_FILE, event->data.vid_dev_err.status,
			     "Video device id=%d error for call %d",
			     event->data.vid_dev_err.id,
			     call->index));
	    }
	    break;
#endif

	default:
	    break;
    }

    if (pjsua_var.ua_cfg.cb.on_call_media_event) {
	pjsua_event_list *eve = NULL;
 
    	pj_mutex_lock(pjsua_var.timer_mutex);

    	if (pj_list_empty(&pjsua_var.event_list)) {
            eve = PJ_POOL_ALLOC_T(pjsua_var.timer_pool, pjsua_event_list);
    	} else {
            eve = pjsua_var.event_list.next;
            pj_list_erase(eve);
    	}

    	pj_mutex_unlock(pjsua_var.timer_mutex);
    	
    	if (call) {
    	    if (call->hanging_up)
    	    	return status;

    	    eve->call_id = call->index;
    	    eve->med_idx = call_med->idx;
    	} else {
	    /* Also deliver non call events such as audio device error */
    	    eve->call_id = PJSUA_INVALID_ID;
    	    eve->med_idx = 0;
    	}
    	pj_memcpy(&eve->event, event, sizeof(pjmedia_event));
    	pjsua_schedule_timer2(&call_med_event_cb, eve, 1);
    }

    return status;
}

/* Set media transport state and notify the application via the callback. */
void pjsua_set_media_tp_state(pjsua_call_media *call_med,
                              pjsua_med_tp_st tp_st)
{
    if (!call_med->call->hanging_up &&
        pjsua_var.ua_cfg.cb.on_call_media_transport_state &&
        call_med->tp_st != tp_st)
    {
        pjsua_med_tp_state_info info;

        pj_bzero(&info, sizeof(info));
        info.med_idx = call_med->idx;
        info.state = tp_st;
        info.status = call_med->tp_ready;
	(*pjsua_var.ua_cfg.cb.on_call_media_transport_state)(
            call_med->call->index, &info);
    }

    call_med->tp_st = tp_st;
}


/* This callback is called when SRTP negotiation completes */
static void on_srtp_nego_complete(pjmedia_transport *tp, 
				  pj_status_t result)
{
    pjsua_call_media *call_med = (pjsua_call_media*)tp->user_data;
    pjsua_call *call;

    if (!call_med)
	return;

    call = call_med->call;
    PJ_PERROR(4,(THIS_FILE, result,
		 "Call %d: Media %d: SRTP negotiation completes",
	         call->index, call_med->idx));

    if (result != PJ_SUCCESS) {
	call_med->state = PJSUA_CALL_MEDIA_ERROR;
	call_med->dir = PJMEDIA_DIR_NONE;
	if (call && !call->hanging_up &&
	    pjsua_var.ua_cfg.cb.on_call_media_state)
	{
	    /* Defer the callback to a timer */
	    pjsua_schedule_timer2(&ice_failed_nego_cb,
				  (void*)(pj_ssize_t)call->index, 1);
	}
    }
}


/* Callback to resume pjsua_call_media_init() after media transport
 * creation is completed.
 */
static pj_status_t call_media_init_cb(pjsua_call_media *call_med,
                                      pj_status_t status,
                                      int security_level,
                                      int *sip_err_code)
{
    pjsua_acc *acc = &pjsua_var.acc[call_med->call->acc_id];
    pjmedia_transport_info tpinfo;
    int err_code = 0;

    if (status != PJ_SUCCESS) {
	err_code = PJSIP_SC_TEMPORARILY_UNAVAILABLE;
        goto on_return;
    }

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_ENCODING,
				    pjsua_var.media_cfg.tx_drop_pct);

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_DECODING,
				    pjsua_var.media_cfg.rx_drop_pct);

    if (call_med->tp_st == PJSUA_MED_TP_CREATING)
        pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_IDLE);

    if (!call_med->tp_orig &&
        pjsua_var.ua_cfg.cb.on_create_media_transport)
    {
        call_med->use_custom_med_tp = PJ_TRUE;
    } else
        call_med->use_custom_med_tp = PJ_FALSE;

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* This function may be called when SRTP transport already exists
     * (e.g: in re-invite, update), don't need to destroy/re-create.
     */
    if (!call_med->tp_orig) {
	pjmedia_srtp_setting srtp_opt;
	pjsua_srtp_opt *acc_srtp_opt = &acc->cfg.srtp_opt;
	pjmedia_transport *srtp = NULL;
	unsigned i;

	/* Check if SRTP requires secure signaling */
	if (acc->cfg.use_srtp != PJMEDIA_SRTP_DISABLED) {
	    if (security_level < acc->cfg.srtp_secure_signaling) {
		err_code = PJSIP_SC_NOT_ACCEPTABLE;
		status = PJSIP_ESESSIONINSECURE;
		goto on_return;
	    }
	}

	/* Always create SRTP adapter */
	pjmedia_srtp_setting_default(&srtp_opt);
	srtp_opt.close_member_tp = PJ_TRUE;
	srtp_opt.cb.on_srtp_nego_complete = &on_srtp_nego_complete;
	srtp_opt.user_data = call_med;

	/* Get crypto and keying settings from account settings */
	srtp_opt.crypto_count = acc_srtp_opt->crypto_count;
	for (i = 0; i < srtp_opt.crypto_count; ++i)
	    srtp_opt.crypto[i] = acc_srtp_opt->crypto[i];
	srtp_opt.keying_count = acc_srtp_opt->keying_count;
	for (i = 0; i < srtp_opt.keying_count; ++i)
	    srtp_opt.keying[i] = acc_srtp_opt->keying[i];

	/* If media session has been ever established, let's use remote's 
	 * preference in SRTP usage policy, especially when it is stricter.
	 */
	if (call_med->rem_srtp_use > acc->cfg.use_srtp)
	    srtp_opt.use = call_med->rem_srtp_use;
	else
	    srtp_opt.use = acc->cfg.use_srtp;

	if (pjsua_var.ua_cfg.cb.on_create_media_transport_srtp) {
	    pjmedia_srtp_setting srtp_opt2 = srtp_opt;
	    pjsua_call *call = call_med->call;

	    /* Warn that this callback is deprecated (see also #2100) */
	    PJ_LOG(1,(THIS_FILE, "Warning: on_create_media_transport_srtp "
				 "is deprecated and will be removed in the "
				 "future release"));

	    (*pjsua_var.ua_cfg.cb.on_create_media_transport_srtp)
		(call->index, call_med->idx, &srtp_opt2);

	    /* Only apply SRTP usage policy if this is initial INVITE */
	    if (call->inv && call->inv->state < PJSIP_INV_STATE_CONFIRMED) {
		srtp_opt.use = srtp_opt2.use;
	    }

	    /* Apply crypto and keying settings from callback */
	    srtp_opt.crypto_count = srtp_opt2.crypto_count;
	    for (i = 0; i < srtp_opt.crypto_count; ++i)
		srtp_opt.crypto[i] = srtp_opt2.crypto[i];
	    srtp_opt.keying_count = srtp_opt2.keying_count;
	    for (i = 0; i < srtp_opt.keying_count; ++i)
		srtp_opt.keying[i] = srtp_opt2.keying[i];
    	}

	status = pjmedia_transport_srtp_create(pjsua_var.med_endpt,
					       call_med->tp,
					       &srtp_opt, &srtp);
	if (status != PJ_SUCCESS) {
	    err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
	    goto on_return;
	}

	/* Set SRTP as current media transport */
	call_med->tp_orig = call_med->tp;
	call_med->tp = srtp;
    }
#else
    call_med->tp_orig = call_med->tp;
    PJ_UNUSED_ARG(security_level);
#endif


    pjmedia_transport_info_init(&tpinfo);
    pjmedia_transport_get_info(call_med->tp, &tpinfo);

    pj_sockaddr_cp(&call_med->rtp_addr, &tpinfo.sock_info.rtp_addr_name);


on_return:
    if (status != PJ_SUCCESS) {
	if (call_med->tp) {
	    pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_NULL);
	    pjmedia_transport_close(call_med->tp);
	    call_med->tp = NULL;
	}

	if (err_code == 0)
	    err_code = PJSIP_ERRNO_TO_SIP_STATUS(status);

	if (sip_err_code)
	    *sip_err_code = err_code;
    }

    if (call_med->med_init_cb) {
        pjsua_med_tp_state_info info;

        pj_bzero(&info, sizeof(info));
        info.status = status;
        info.state = call_med->tp_st;
        info.med_idx = call_med->idx;
        info.sip_err_code = err_code;
        (*call_med->med_init_cb)(call_med->call->index, &info);
    }

    return status;
}

/* Determine if call's media is being changed, for example when video is being
 * added. Then we can reject incoming re-INVITE, for example. This is the
 * solution for https://trac.pjsip.org/repos/ticket/1738
 */
pj_bool_t  pjsua_call_media_is_changing(pjsua_call *call)
{
    /* The problem in #1738 occurs because we do handle_events() loop while
     * adding media, which could cause incoming re-INVITE to be processed and
     * cause havoc. Since the handle_events() loop only happens while adding
     * media, it is sufficient to only check if "prov > cnt" for now.
     */
    return call->med_prov_cnt > call->med_cnt;
}

/* Initialize the media line */
pj_status_t pjsua_call_media_init(pjsua_call_media *call_med,
                                  pjmedia_type type,
				  const pjsua_transport_config *tcfg,
				  int security_level,
				  int *sip_err_code,
                                  pj_bool_t async,
                                  pjsua_med_tp_state_cb cb)
{
    pj_status_t status = PJ_SUCCESS;

    /*
     * Note: this function may be called when the media already exists
     * (e.g. in reinvites, updates, etc.)
     */
    call_med->type = type;

    /* Create the media transport for initial call. Here are the possible
     * media transport state and the action needed:
     * - PJSUA_MED_TP_NULL or call_med->tp==NULL, create one.
     * - PJSUA_MED_TP_RUNNING, do nothing.
     * - PJSUA_MED_TP_DISABLED, re-init (media_create(), etc). Currently,
     *   this won't happen as media_channel_update() will always clean up
     *   the unused transport of a disabled media.
     */
    if (call_med->tp == NULL) {
    	pjsua_acc *acc = &pjsua_var.acc[call_med->call->acc_id];

        /* Initializations. If media transport creation completes immediately, 
         * we don't need to call the callbacks.
         */
        call_med->med_init_cb = NULL;
        call_med->med_create_cb = NULL;

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
	/* While in initial call, set default video devices */
	if (type == PJMEDIA_TYPE_VIDEO) {
	    status = pjsua_vid_channel_init(call_med);
	    if (status != PJ_SUCCESS)
		return status;
	}
#endif

        pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_CREATING);

	if (acc->cfg.use_loop_med_tp) {
	    status = create_loop_media_transport(tcfg, call_med);
	} else if (acc->cfg.ice_cfg.enable_ice) {
	    status = create_ice_media_transport(tcfg, call_med, async);
            if (async && status == PJ_EPENDING) {
	        /* We will resume call media initialization in the
	         * on_ice_complete() callback.
	         */
                call_med->med_create_cb = &call_media_init_cb;
                call_med->med_init_cb = cb;
                
	        return PJ_EPENDING;
	    }
	} else {
	    status = create_udp_media_transport(tcfg, call_med);
	}

        if (status != PJ_SUCCESS) {
	    if (sip_err_code)
		*sip_err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
	    call_med->tp_ready = status;
	    pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_NULL);
	    pjsua_perror(THIS_FILE, "Error creating media transport", status);
	    return status;
	}

    } else if (call_med->tp_st == PJSUA_MED_TP_DISABLED) {
	/* Media is being reenabled. */
	//pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_IDLE);

	pj_assert(!"Currently no media transport reuse");
    }

    return call_media_init_cb(call_med, status, security_level,
                              sip_err_code);
}

/* Callback to resume pjsua_media_channel_init() after media transport
 * initialization is completed.
 */
static pj_status_t media_channel_init_cb(pjsua_call_id call_id,
                                         const pjsua_med_tp_state_info *info)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    pj_status_t status = (info? info->status : PJ_SUCCESS);
    unsigned mi;

    if (info) {
        pj_mutex_lock(call->med_ch_mutex);

        /* Set the callback to NULL to indicate that the async operation
         * has completed.
         */
        call->media_prov[info->med_idx].med_init_cb = NULL;

        /* In case of failure, save the information to be returned
         * by the last media transport to finish.
         */
        if (info->status != PJ_SUCCESS)
            pj_memcpy(&call->med_ch_info, info, sizeof(*info));

        /* Check whether all the call's medias have finished calling their
         * callbacks.
         */
        for (mi=0; mi < call->med_prov_cnt; ++mi) {
            pjsua_call_media *call_med = &call->media_prov[mi];

            if (call_med->med_init_cb) {
                pj_mutex_unlock(call->med_ch_mutex);
                return PJ_SUCCESS;
            }

            if (call_med->tp_ready != PJ_SUCCESS)
                status = call_med->tp_ready;
        }

        /* OK, we are called by the last media transport finished. */
        pj_mutex_unlock(call->med_ch_mutex);
    }

    if (call->med_ch_mutex) {
        pj_mutex_destroy(call->med_ch_mutex);
        call->med_ch_mutex = NULL;
    }

    PJ_PERROR(5,(THIS_FILE, status,
		 "Call %d: media transport initialization complete", call_id));

    if (status != PJ_SUCCESS) {
	if (call->med_ch_info.status == PJ_SUCCESS) {
	    call->med_ch_info.status = status;
	    call->med_ch_info.sip_err_code = PJSIP_SC_TEMPORARILY_UNAVAILABLE;
	}

	/* Revert back provisional media. */
	pjsua_media_prov_revert(call_id);

	goto on_return;
    }

    /* Tell the media transport of a new offer/answer session */
    for (mi=0; mi < call->med_prov_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media_prov[mi];

	/* Note: tp may be NULL if this media line is disabled */
	if (call_med->tp && call_med->tp_st == PJSUA_MED_TP_IDLE) {
            pj_pool_t *tmp_pool = call->async_call.pool_prov;
            
            if (!tmp_pool) {
                tmp_pool = (call->inv? call->inv->pool_prov:
                            call->async_call.dlg->pool);
            }

            if (call_med->use_custom_med_tp) {
                unsigned custom_med_tp_flags = PJSUA_MED_TP_CLOSE_MEMBER;

                /* Use custom media transport returned by the application */
                call_med->tp =
                    (*pjsua_var.ua_cfg.cb.on_create_media_transport)
                        (call_id, mi, call_med->tp,
                         custom_med_tp_flags);
                if (!call_med->tp) {
                    status =
                        PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_TEMPORARILY_UNAVAILABLE);
                }
            }

            if (call_med->tp) {
            	unsigned options = (call_med->enable_rtcp_mux?
            			    PJMEDIA_TPMED_RTCP_MUX: 0);
                status = pjmedia_transport_media_create(
                             call_med->tp, tmp_pool,
                             options, call->async_call.rem_sdp, mi);
            }
	    if (status != PJ_SUCCESS) {
                call->med_ch_info.status = status;
                call->med_ch_info.med_idx = mi;
                call->med_ch_info.state = call_med->tp_st;
                call->med_ch_info.sip_err_code = PJSIP_SC_TEMPORARILY_UNAVAILABLE;

		/* Revert back provisional media. */
		pjsua_media_prov_revert(call_id);

		goto on_return;
	    }

	    pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_INIT);
	}
    }

    call->med_ch_info.status = PJ_SUCCESS;

on_return:
    if (call->med_ch_cb)
        (*call->med_ch_cb)(call->index, &call->med_ch_info);

    return status;
}


/* Clean up media transports in provisional media that is not used by
 * call media.
 */
void pjsua_media_prov_clean_up(pjsua_call_id call_id)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    unsigned i;

    if (call->med_prov_cnt > call->med_cnt) {
        PJ_LOG(4,(THIS_FILE, "Call %d: cleaning up provisional media, "
        		     "prov_med_cnt=%d, med_cnt=%d",
			     call_id, call->med_prov_cnt, call->med_cnt));
    }

    for (i = 0; i < call->med_prov_cnt; ++i) {
	pjsua_call_media *call_med = &call->media_prov[i];
	unsigned j;
	pj_bool_t used = PJ_FALSE;

	if (call_med->tp == NULL)
	    continue;

	for (j = 0; j < call->med_cnt; ++j) {
	    if (call->media[j].tp == call_med->tp) {
		used = PJ_TRUE;
		break;
	    }
	}

	if (!used) {
	    if (call_med->tp_st > PJSUA_MED_TP_IDLE) {
		pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_IDLE);
		pjmedia_transport_media_stop(call_med->tp);
	    }
	    pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_NULL);
	    pjmedia_transport_close(call_med->tp);
	    call_med->tp = call_med->tp_orig = NULL;
	}
    }
    
    // Cleaning up unused media transports should not change provisional
    // media count.
    //call->med_prov_cnt = 0;
}


/* Revert back provisional media. */
void pjsua_media_prov_revert(pjsua_call_id call_id)
{
    pjsua_call *call = &pjsua_var.calls[call_id];

    /* Clean up unused media transport */
    pjsua_media_prov_clean_up(call_id);

    /* Copy provisional media from active media */
    pj_memcpy(call->media_prov, call->media,
	      sizeof(call->media[0]) * call->med_cnt);
    call->med_prov_cnt = call->med_cnt;
}


pj_status_t pjsua_media_channel_init(pjsua_call_id call_id,
				     pjsip_role_e role,
				     int security_level,
				     pj_pool_t *tmp_pool,
				     const pjmedia_sdp_session *rem_sdp,
				     int *sip_err_code,
                                     pj_bool_t async,
                                     pjsua_med_tp_state_cb cb)
{
    const pj_str_t STR_AUDIO = { "audio", 5 };
    const pj_str_t STR_VIDEO = { "video", 5 };
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjsua_acc *acc = &pjsua_var.acc[call->acc_id];
    pj_uint8_t maudidx[PJSUA_MAX_CALL_MEDIA];
    unsigned maudcnt = PJ_ARRAY_SIZE(maudidx);
    unsigned mtotaudcnt = PJ_ARRAY_SIZE(maudidx);
    pj_uint8_t mvididx[PJSUA_MAX_CALL_MEDIA];
    unsigned mvidcnt = PJ_ARRAY_SIZE(mvididx);
    unsigned mtotvidcnt = PJ_ARRAY_SIZE(mvididx);
    unsigned mi;
    pj_bool_t pending_med_tp = PJ_FALSE;
    pj_bool_t reinit = PJ_FALSE;
    pj_status_t status;

    PJ_UNUSED_ARG(role);

    /*
     * Note: this function may be called when the media already exists
     * (e.g. in reinvites, updates, etc).
     */

    if (pjsua_get_state() != PJSUA_STATE_RUNNING) {
        if (sip_err_code)
	    *sip_err_code = PJSIP_SC_SERVICE_UNAVAILABLE;
	return PJ_EBUSY;
    }

    if (async) {
        pj_pool_t *tmppool = (call->inv? call->inv->pool_prov:
                              call->async_call.dlg->pool);

        status = pj_mutex_create_simple(tmppool, NULL, &call->med_ch_mutex);
	if (status != PJ_SUCCESS) {
	    if (sip_err_code)
		*sip_err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
            return status;
	}
    }

    if (call->inv && call->inv->state == PJSIP_INV_STATE_CONFIRMED)
	reinit = PJ_TRUE;

    PJ_LOG(4,(THIS_FILE, "Call %d: %sinitializing media..",
			 call_id, (reinit?"re-":"") ));

    pj_log_push_indent();

    /* Init provisional media state */
    if (call->med_cnt == 0) {
	/* New media session, just copy whole from call media state. */
	pj_memcpy(call->media_prov, call->media, sizeof(call->media));
    } else {
	/* Clean up any unused transports. Note that when local SDP reoffer
	 * is rejected by remote, there may be any initialized transports that
	 * are not used by call media and currently there is no notification
	 * from PJSIP level regarding the reoffer rejection.
	 */
	pjsua_media_prov_clean_up(call_id);

	/* Updating media session, copy from call media state. */
	pj_memcpy(call->media_prov, call->media,
		  sizeof(call->media[0]) * call->med_cnt);
    }
    call->med_prov_cnt = call->med_cnt;

#if DISABLED_FOR_TICKET_1185
    /* Return error if media transport has not been created yet
     * (e.g. application is starting)
     */
    for (i=0; i<call->med_cnt; ++i) {
	if (call->media[i].tp == NULL) {
	    status = PJ_EBUSY;
	    goto on_error;
	}
    }
#endif

    /* Get media count for each media type */
    if (rem_sdp) {

	/* We are sending answer, check media count for each media type
	 * from the remote SDP.
	 */
	sort_media(rem_sdp, &STR_AUDIO, acc->cfg.use_srtp,
		   maudidx, &maudcnt, &mtotaudcnt);

#if PJMEDIA_HAS_VIDEO
	sort_media(rem_sdp, &STR_VIDEO, acc->cfg.use_srtp,
		   mvididx, &mvidcnt, &mtotvidcnt);
#else
	mvidcnt = mtotvidcnt = 0;
	PJ_UNUSED_ARG(STR_VIDEO);
#endif

	if (maudcnt + mvidcnt == 0) {
	    /* Expecting audio or video in the offer */
	    if (sip_err_code)
		*sip_err_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;
	    status = PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE_HERE);
	    goto on_error;
	}

	/* Update media count only when remote add any media, this media count
	 * must never decrease. Also note that we shouldn't apply the media
	 * count setting (of the call setting) before the SDP negotiation.
	 */
	if (call->med_prov_cnt < rem_sdp->media_count)
	    call->med_prov_cnt = PJ_MIN(rem_sdp->media_count,
					PJSUA_MAX_CALL_MEDIA);

	call->rem_offerer = PJ_TRUE;
	call->rem_aud_cnt = maudcnt;
	call->rem_vid_cnt = mvidcnt;

    } else {

	/* If call is already established, adjust the existing call media list
	 * to media count setting in call setting, e.g: re-enable/disable/add
	 * media from existing media.
	 * Otherwise, apply media count from the call setting directly.
	 */
	if (reinit) {
	    pj_bool_t sort_check_tp;

	    /* Media sorting below will check transport, i.e: media without
	     * transport will have lower priority. If PJSUA_CALL_REINIT_MEDIA
	     * is set, we must not check transport.
	     */
	    sort_check_tp = !(call->opt.flag & PJSUA_CALL_REINIT_MEDIA);

	    /* We are sending reoffer, check media count for each media type
	     * from the existing call media list.
	     */
	    sort_media2(call->media_prov, sort_check_tp, call->med_prov_cnt,
			PJMEDIA_TYPE_AUDIO, maudidx, &maudcnt, &mtotaudcnt);

	    /* No need to assert if there's no media. */
	    //pj_assert(maudcnt > 0);

	    sort_media2(call->media_prov, sort_check_tp, call->med_prov_cnt,
			PJMEDIA_TYPE_VIDEO, mvididx, &mvidcnt, &mtotvidcnt);

	    /* Call setting may add or remove media. Adding media is done by
	     * enabling any disabled/port-zeroed media first, then adding new
	     * media whenever needed. Removing media is done by disabling
	     * media with the lowest 'quality'.
	     */

	    /* Check if we need to add new audio */
	    if (maudcnt < call->opt.aud_cnt &&
		mtotaudcnt < call->opt.aud_cnt)
	    {
		for (mi = 0; mi < call->opt.aud_cnt - mtotaudcnt; ++mi)
		    maudidx[maudcnt++] = (pj_uint8_t)call->med_prov_cnt++;
		
		mtotaudcnt = call->opt.aud_cnt;
	    }
	    maudcnt = call->opt.aud_cnt;

	    /* Check if we need to add new video */
	    if (mvidcnt < call->opt.vid_cnt &&
		mtotvidcnt < call->opt.vid_cnt)
	    {
		for (mi = 0; mi < call->opt.vid_cnt - mtotvidcnt; ++mi)
		    mvididx[mvidcnt++] = (pj_uint8_t)call->med_prov_cnt++;

		mtotvidcnt = call->opt.vid_cnt;
	    }
	    mvidcnt = call->opt.vid_cnt;

	    /* In case of media reinit, 'med_prov_cnt' may be decreased
	     * because the new call->opt says so. As media count should
	     * never decrease, we should verify 'med_prov_cnt' to be
	     * at least equal to 'med_cnt' (see also #1987).
	     */
	    if ((call->opt.flag & PJSUA_CALL_REINIT_MEDIA) &&
		call->med_prov_cnt < call->med_cnt)
	    {
		call->med_prov_cnt = call->med_cnt;
	    }

	} else {

	    maudcnt = mtotaudcnt = call->opt.aud_cnt;
	    for (mi=0; mi<maudcnt; ++mi) {
		maudidx[mi] = (pj_uint8_t)mi;
	    }
	    mvidcnt = mtotvidcnt = call->opt.vid_cnt;
	    for (mi=0; mi<mvidcnt; ++mi) {
		mvididx[mi] = (pj_uint8_t)(maudcnt + mi);
	    }
	    call->med_prov_cnt = maudcnt + mvidcnt;

	    /* Need to publish supported media? */
	    if (call->opt.flag & PJSUA_CALL_INCLUDE_DISABLED_MEDIA) {
		if (mtotaudcnt == 0) {
		    mtotaudcnt = 1;
		    maudidx[0] = (pj_uint8_t)call->med_prov_cnt++;
		}
#if PJMEDIA_HAS_VIDEO
		if (mtotvidcnt == 0) {
		    mtotvidcnt = 1;
		    mvididx[0] = (pj_uint8_t)call->med_prov_cnt++;
		}
#endif
	    }
	}

	call->rem_offerer = PJ_FALSE;
    }

    if (call->med_prov_cnt == 0) {
	/* Expecting at least one media */
	if (sip_err_code)
	    *sip_err_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;
	status = PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE_HERE);
	goto on_error;
    }

    if (async) {
        call->med_ch_cb = cb;
    }

    if (rem_sdp) {
        call->async_call.rem_sdp =
            pjmedia_sdp_session_clone(call->inv->pool_prov, rem_sdp);
    } else {
	call->async_call.rem_sdp = NULL;
    }

    call->async_call.pool_prov = tmp_pool;

    /* Initialize each media line */
    for (mi=0; mi < call->med_prov_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media_prov[mi];
	pj_bool_t enabled = PJ_FALSE;
	pjmedia_type media_type = PJMEDIA_TYPE_UNKNOWN;

	if (pj_memchr(maudidx, mi, mtotaudcnt * sizeof(maudidx[0]))) {
	    media_type = PJMEDIA_TYPE_AUDIO;
	    if (call->opt.aud_cnt &&
		pj_memchr(maudidx, mi, maudcnt * sizeof(maudidx[0])))
	    {
		enabled = PJ_TRUE;
	    }
	} else if (pj_memchr(mvididx, mi, mtotvidcnt * sizeof(mvididx[0]))) {
	    media_type = PJMEDIA_TYPE_VIDEO;
	    if (call->opt.vid_cnt &&
		pj_memchr(mvididx, mi, mvidcnt * sizeof(mvididx[0])))
	    {
		enabled = PJ_TRUE;
	    }
	}

	if (call->opt.flag & PJSUA_CALL_SET_MEDIA_DIR) {
	    call_med->def_dir = call->opt.media_dir[mi];
    	    PJ_LOG(4,(THIS_FILE, "Call %d: setting media direction "
    	    			 "#%d to %d.", call_id, mi,
    	    			 call_med->def_dir));
	} else if (!reinit) {
	    /* Initialize default initial media direction as bidirectional */
	    call_med->def_dir = PJMEDIA_DIR_ENCODING_DECODING;
	}

	if (enabled) {
	    call_med->enable_rtcp_mux = acc->cfg.enable_rtcp_mux;

	    status = pjsua_call_media_init(call_med, media_type,
	                                   &acc->cfg.rtp_cfg,
					   security_level, sip_err_code,
                                           async,
                                           (async? &media_channel_init_cb:
                                            NULL));
            if (status == PJ_EPENDING) {
                pending_med_tp = PJ_TRUE;
            } else if (status != PJ_SUCCESS) {
                if (pending_med_tp) {
                    /* Save failure information. */
                    call_med->tp_ready = status;
                    pj_bzero(&call->med_ch_info, sizeof(call->med_ch_info));
                    call->med_ch_info.status = status;
                    call->med_ch_info.state = call_med->tp_st;
                    call->med_ch_info.med_idx = call_med->idx;
                    if (sip_err_code)
                        call->med_ch_info.sip_err_code = *sip_err_code;

                    /* We will return failure in the callback later. */
                    return PJ_EPENDING;
                }

		/* Revert back provisional media. */
		pjsua_media_prov_revert(call_id);

		goto on_error;
	    }

	    /* Find and save "a=mid". Currently this is for trickle ICE.
	     * Trickle ICE match media in SDP of SIP INFO by comparing this
	     * attribute, so remote SDP must be received first before remote
	     * SDP in SIP INFO can be processed.
	     */
	    if (rem_sdp && call_med->rem_mid.slen == 0) {
		const pjmedia_sdp_media *m = rem_sdp->media[mi];
		pjmedia_sdp_attr *a;

		a = pjmedia_sdp_media_find_attr2(m, "mid", NULL);
		if (a)
		    call_med->rem_mid = a->value;
	    }

	} else {
	    /* By convention, the media is disabled if transport is NULL 
	     * or transport state is PJSUA_MED_TP_DISABLED.
	     */
	    if (call_med->tp) {
		// Don't close transport here, as SDP negotiation has not been
		// done and stream may be still active. Once SDP negotiation
		// is done (channel_update() invoked), this transport will be
		// closed there.
		//pjmedia_transport_close(call_med->tp);
		//call_med->tp = NULL;
		pj_assert(call_med->tp_st == PJSUA_MED_TP_INIT || 
			  call_med->tp_st == PJSUA_MED_TP_RUNNING);
		pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_DISABLED);
	    }

	    /* Put media type just for info if not yet defined */
	    if (call_med->type == PJMEDIA_TYPE_NONE)
		call_med->type = media_type;
	}
    }

    if (maudcnt > 0) {
    	call->audio_idx = maudidx[0];

    	PJ_LOG(4,(THIS_FILE, "Media index %d selected for audio call %d",
	      	  call->audio_idx, call->index));
    } else {
    	call->audio_idx = -1;
    }

    if (pending_med_tp) {
        /* We shouldn't use temporary pool anymore. */
        call->async_call.pool_prov = NULL;
        /* We have a pending media transport initialization. */
        pj_log_pop_indent();
        return PJ_EPENDING;
    }

    /* Media transport initialization completed immediately, so 
     * we don't need to call the callback.
     */
    call->med_ch_cb = NULL;

    status = media_channel_init_cb(call_id, NULL);
    if (status != PJ_SUCCESS && sip_err_code) {
	if (call->med_ch_info.sip_err_code)
	    *sip_err_code = call->med_ch_info.sip_err_code;
	else
	    *sip_err_code = PJSIP_ERRNO_TO_SIP_STATUS(status);
    }

    pj_log_pop_indent();
    return status;

on_error:
    if (call->med_ch_mutex) {
        pj_mutex_destroy(call->med_ch_mutex);
        call->med_ch_mutex = NULL;
    }

    if (sip_err_code && *sip_err_code == 0)
	*sip_err_code = PJSIP_ERRNO_TO_SIP_STATUS(status);

    pj_log_pop_indent();
    return status;
}


/* Create SDP based on the current media channel. Note that, this function
 * will not modify the media channel, so when receiving new offer or
 * updating media count (via call setting), media channel must be reinit'd
 * (using pjsua_media_channel_init()) first before calling this function.
 */
pj_status_t pjsua_media_channel_create_sdp(pjsua_call_id call_id, 
					   pj_pool_t *pool,
					   const pjmedia_sdp_session *rem_sdp,
					   pjmedia_sdp_session **p_sdp,
					   int *sip_err_code)
{
    enum { MAX_MEDIA = PJSUA_MAX_CALL_MEDIA };
    pjmedia_sdp_session *sdp;
    pj_sockaddr origin;
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjsua_acc *acc = &pjsua_var.acc[call->acc_id];
    pjmedia_sdp_neg_state sdp_neg_state = PJMEDIA_SDP_NEG_STATE_NULL;
    unsigned mi;
    unsigned tot_bandw_tias = 0;
    pj_status_t status;

    if (pjsua_get_state() != PJSUA_STATE_RUNNING) {
	status = PJ_EBUSY;
	goto on_error;
    }

#if 0
    // This function should not really change the media channel.
    if (rem_sdp) {
	/* If this is a re-offer, let's re-initialize media as remote may
	 * add or remove media
	 */
	if (call->inv && call->inv->state == PJSIP_INV_STATE_CONFIRMED) {
	    status = pjsua_media_channel_init(call_id, PJSIP_ROLE_UAS,
					      call->secure_level, pool,
					      rem_sdp, sip_err_code,
                                              PJ_FALSE, NULL);
	    if (status != PJ_SUCCESS)
		return status;
	}
    } else {
	/* Audio is first in our offer, by convention */
	// The audio_idx should not be changed here, as this function may be
	// called in generating re-offer and the current active audio index
	// can be anywhere.
	//call->audio_idx = 0;
    }
#endif

#if 0
    // Since r3512, old-style hold should have got transport, created by 
    // pjsua_media_channel_init() in initial offer/answer or remote reoffer.
    /* Create media if it's not created. This could happen when call is
     * currently on-hold (with the old style hold)
     */
    if (call->media[call->audio_idx].tp == NULL) {
	pjsip_role_e role;
	role = (rem_sdp ? PJSIP_ROLE_UAS : PJSIP_ROLE_UAC);
	status = pjsua_media_channel_init(call_id, role, call->secure_level, 
					  pool, rem_sdp, sip_err_code);
	if (status != PJ_SUCCESS)
	    return status;
    }
#endif

    /* Get SDP negotiator state */
    if (call->inv && call->inv->neg)
	sdp_neg_state = pjmedia_sdp_neg_get_state(call->inv->neg);

    PJ_UNUSED_ARG(sdp_neg_state);

    /* Get one address to use in the origin field */
    pj_bzero(&origin, sizeof(origin));
    for (mi=0; mi<call->med_prov_cnt; ++mi) {
	pjmedia_transport_info tpinfo;

	if (call->media_prov[mi].tp == NULL)
	    continue;

	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(call->media_prov[mi].tp, &tpinfo);
	pj_sockaddr_cp(&origin, &tpinfo.sock_info.rtp_addr_name);
	break;
    }

    /* Create the base (blank) SDP */
    status = pjmedia_endpt_create_base_sdp(pjsua_var.med_endpt, pool, NULL,
                                           &origin, &sdp);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Process each media line */
    for (mi=0; mi<call->med_prov_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media_prov[mi];
	pjmedia_sdp_media *m = NULL;
	pjmedia_transport_info tpinfo;
	pjmedia_endpt_create_sdp_param param;
	unsigned i;

	if (rem_sdp && mi >= rem_sdp->media_count) {
	    /* Remote might have removed some media lines. */
	    /* Note that we must not modify the current active media
	     * (e.g: stop stream, close/cleanup media transport), as if
	     * SDP nego fails, the current active media should be maintained.
	     * Also note that our media count should never decrease, even when
	     * remote removed some media lines.
	     */
	    break;
	}

	if (call_med->tp == NULL || call_med->tp_st == PJSUA_MED_TP_DISABLED)
	{
	    /*
	     * This media is disabled. Just create a valid SDP with zero
	     * port.
	     */
	    if (rem_sdp) {
		/* Just clone the remote media and deactivate it */
		m = pjmedia_sdp_media_clone_deactivate(pool,
						       rem_sdp->media[mi]);
	    } else {
		m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
		m->desc.transport = pj_str("RTP/AVP");
		m->desc.fmt_count = 1;

		switch (call_med->type) {
		case PJMEDIA_TYPE_AUDIO:
		    m->desc.media = pj_str("audio");
		    m->desc.fmt[0] = pj_str("0");
		    break;
		case PJMEDIA_TYPE_VIDEO:
		    m->desc.media = pj_str("video");
		    m->desc.fmt[0] = pj_str("31");
		    break;
		default:
		    /* This must be us generating re-offer, and some unknown
		     * media may exist, so just clone from active local SDP
		     * (and it should have been deactivated already).
		     */
		    pj_assert(call->inv && call->inv->neg &&
			      sdp_neg_state == PJMEDIA_SDP_NEG_STATE_DONE);
		    {
			const pjmedia_sdp_session *s_;
			pjmedia_sdp_neg_get_active_local(call->inv->neg, &s_);

			if (mi < s_->media_count) {
			    m = pjmedia_sdp_media_clone(pool, s_->media[mi]);
			    m->desc.port = 0;
			} else {
			    /* Remote may have removed some media lines in
			     * previous negotiations. However, since our
			     * media count may never decrease (as per
			     * the RFC), we'll just offer unknown media here.
			     */
		    	    m->desc.media = pj_str("unknown");
		            m->desc.fmt[0] = pj_str("0");
			}
		    }
		    break;
		}
	    }

	    /* Add connection line, if none */
	    if (m->conn == NULL && sdp->conn == NULL) {
		pj_bool_t use_ipv6;
		pj_bool_t use_nat64;

		use_ipv6 = (pjsua_var.acc[call->acc_id].cfg.ipv6_media_use !=
			    PJSUA_IPV6_DISABLED);
		use_nat64 = (pjsua_var.acc[call->acc_id].cfg.nat64_opt !=
			     PJSUA_NAT64_DISABLED);

		m->conn = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
		m->conn->net_type = pj_str("IN");
		if (use_ipv6 && !use_nat64) {
		    m->conn->addr_type = pj_str("IP6");
		    m->conn->addr = pj_str("::1");
		} else {
		    m->conn->addr_type = pj_str("IP4");
		    m->conn->addr = pj_str("127.0.0.1");
		}
	    }

	    sdp->media[sdp->media_count++] = m;
	    continue;
	}

	/* Get transport address info */
	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(call_med->tp, &tpinfo);

	/* Ask pjmedia endpoint to create SDP media line */
	pjmedia_endpt_create_sdp_param_default(&param);
	param.dir = call_med->def_dir;
	switch (call_med->type) {
	case PJMEDIA_TYPE_AUDIO:
	    status = pjmedia_endpt_create_audio_sdp(pjsua_var.med_endpt, pool,
                                                    &tpinfo.sock_info,
                                                    &param, &m);
	    break;
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
	case PJMEDIA_TYPE_VIDEO:
	    status = pjmedia_endpt_create_video_sdp(pjsua_var.med_endpt, pool,
	                                            &tpinfo.sock_info,
	                                            &param, &m);
	    break;
#endif
	default:
	    pj_assert(!"Invalid call_med media type");
	    status = PJ_EBUG;
	}

	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Add generated media to SDP session */
	sdp->media[sdp->media_count++] = m;

	/* Disable media if it has zero format/codec */
	if (m->desc.fmt_count == 0) {
	    m->desc.fmt[m->desc.fmt_count++] = pj_str("0");
	    pjmedia_sdp_media_deactivate(pool, m);
	    PJ_LOG(3,(THIS_FILE,
		      "Call %d media %d: Disabled due to no active codec",
		      call_id, mi));
	    continue;
	}

    	/* Add ssrc and cname attribute */
    	m->attr[m->attr_count++] = pjmedia_sdp_attr_create_ssrc(pool,
    								call_med->ssrc,
    								&call->cname);

	/* Give to transport */
	status = pjmedia_transport_encode_sdp(call_med->tp, pool,
					      sdp, rem_sdp, mi);
	if (status != PJ_SUCCESS) {
	    if (sip_err_code) *sip_err_code = PJSIP_SC_NOT_ACCEPTABLE;
	    goto on_error;
	}

#if PJSUA_SDP_SESS_HAS_CONN
	/* Copy c= line of the first media to session level,
	 * if there's none.
	 */
	if (sdp->conn == NULL) {
	    sdp->conn = pjmedia_sdp_conn_clone(pool, m->conn);
	}
#endif

	
	/* Find media bandwidth info */
	for (i = 0; i < m->bandw_count; ++i) {
	    const pj_str_t STR_BANDW_MODIFIER_TIAS = { "TIAS", 4 };
	    if (!pj_stricmp(&m->bandw[i]->modifier, &STR_BANDW_MODIFIER_TIAS))
	    {
		tot_bandw_tias += m->bandw[i]->value;
		break;
	    }
	}

	/* Setup RTCP-FB */
	{
	    pjmedia_rtcp_fb_setting rtcp_cfg;
	    pjmedia_rtcp_fb_setting_default(&rtcp_cfg);

	    /* Add RTCP-FB PLI if PJSUA_VID_REQ_KEYFRAME_RTCP_PLI is set */
	    if (call_med->type == PJMEDIA_TYPE_VIDEO &&
		(call->opt.req_keyframe_method &
		 PJSUA_VID_REQ_KEYFRAME_RTCP_PLI))
	    {
		rtcp_cfg.cap_count = 1;
		pj_strset2(&rtcp_cfg.caps[0].codec_id, (char*)"*");
		rtcp_cfg.caps[0].type = PJMEDIA_RTCP_FB_NACK;
		pj_strset2(&rtcp_cfg.caps[0].param, (char*)"pli");
	    }

	    /* Should we put "RTP/AVPF" in SDP?*/
	    if (rem_sdp) {
		/* For answer, match remote offer */
		unsigned rem_proto = 0;
		rem_proto = pjmedia_sdp_transport_get_proto(
					&rem_sdp->media[mi]->desc.transport);
		rtcp_cfg.dont_use_avpf =
			!PJMEDIA_TP_PROTO_HAS_FLAG(rem_proto, 
						PJMEDIA_TP_PROFILE_RTCP_FB);
	    } else {
		/* For offer, check account setting */
		rtcp_cfg.dont_use_avpf = acc->cfg.rtcp_fb_cfg.dont_use_avpf ||
					 (acc->cfg.rtcp_fb_cfg.cap_count == 0
					  && rtcp_cfg.cap_count == 0);
	    }

	    status = pjmedia_rtcp_fb_encode_sdp(pool, pjsua_var.med_endpt,
						&rtcp_cfg, sdp,
						mi, rem_sdp);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(3,(THIS_FILE, status,
			     "Call %d media %d: Failed to encode RTCP-FB PLI "
			     "setting to SDP",
			     call_id, mi));
	    }

	    /* Add any other RTCP-FB setting configured in account setting */
	    if (acc->cfg.rtcp_fb_cfg.cap_count) {
		pj_bool_t tmp = rtcp_cfg.dont_use_avpf;
		rtcp_cfg = acc->cfg.rtcp_fb_cfg;
		rtcp_cfg.dont_use_avpf = tmp;
		status = pjmedia_rtcp_fb_encode_sdp(pool, pjsua_var.med_endpt,
						    &rtcp_cfg, sdp,
						    mi, rem_sdp);
		if (status != PJ_SUCCESS) {
		    PJ_PERROR(3,(THIS_FILE, status,
				 "Call %d media %d: Failed to encode account "
				 "RTCP-FB setting to SDP",
				 call_id, mi));
		}
	    }
	}

	/* Find and save "a=mid". Currently this is for trickle ICE. Trickle
	 * ICE match media in SDP of SIP INFO by comparing this attribute,
	 * so remote SDP must be received first before remote SDP in SIP INFO
	 * can be processed.
	 */
	if (call_med->rem_mid.slen == 0) {
	    pjmedia_sdp_attr *a;

	    a = pjmedia_sdp_media_find_attr2(m, "mid", NULL);
	    if (a)
		call_med->rem_mid = a->value;
	}
    }

    /* Add NAT info in the SDP */
    if (pjsua_var.ua_cfg.nat_type_in_sdp) {
	pjmedia_sdp_attr *a;
	pj_str_t value;
	char nat_info[80];

	value.ptr = nat_info;
	if (pjsua_var.ua_cfg.nat_type_in_sdp == 1) {
	    value.slen = pj_ansi_snprintf(nat_info, sizeof(nat_info),
					  "%d", pjsua_var.nat_type);
	} else {
	    const char *type_name = pj_stun_get_nat_name(pjsua_var.nat_type);
	    value.slen = pj_ansi_snprintf(nat_info, sizeof(nat_info),
					  "%d %s",
					  pjsua_var.nat_type,
					  type_name);
	}

	a = pjmedia_sdp_attr_create(pool, "X-nat", &value);

	pjmedia_sdp_attr_add(&sdp->attr_count, sdp->attr, a);

    }


    /* Add bandwidth info in session level using bandwidth modifier "AS". */
    if (tot_bandw_tias) {
	unsigned bandw;
	const pj_str_t STR_BANDW_MODIFIER_AS = { "AS", 2 };
	pjmedia_sdp_bandw *b;

	/* AS bandwidth = RTP bitrate + RTCP bitrate.
	 * RTP bitrate  = payload bitrate (total TIAS) + overheads (~16kbps).
	 * RTCP bitrate = est. 5% of RTP bitrate.
	 * Note that AS bandwidth is in kbps.
	 */
	bandw = tot_bandw_tias + 16000;
	bandw += bandw * 5 / 100;
	b = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_bandw);
	b->modifier = STR_BANDW_MODIFIER_AS;
	b->value = bandw / 1000;
	sdp->bandw[sdp->bandw_count++] = b;
    }

#if DISABLED_FOR_TICKET_1185 && defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* Check if SRTP is in optional mode and configured to use duplicated
     * media, i.e: secured and unsecured version, in the SDP offer.
     */
    if (!rem_sdp &&
	pjsua_var.acc[call->acc_id].cfg.use_srtp == PJMEDIA_SRTP_OPTIONAL &&
	pjsua_var.acc[call->acc_id].cfg.srtp_optional_dup_offer)
    {
	unsigned i;

	for (i = 0; i < sdp->media_count; ++i) {
	    pjmedia_sdp_media *m = sdp->media[i];

	    /* Check if this media is unsecured but has SDP "crypto"
	     * attribute.
	     */
	    if (pj_stricmp2(&m->desc.transport, "RTP/AVP") == 0 &&
		pjmedia_sdp_media_find_attr2(m, "crypto", NULL) != NULL)
	    {
		if (i == (unsigned)call->audio_idx &&
		    sdp_neg_state == PJMEDIA_SDP_NEG_STATE_DONE)
		{
		    /* This is a session update, and peer has chosen the
		     * unsecured version, so let's make this unsecured too.
		     */
		    pjmedia_sdp_media_remove_all_attr(m, "crypto");
		} else {
		    /* This is new offer, duplicate media so we'll have
		     * secured (with "RTP/SAVP" transport) and and unsecured
		     * versions.
		     */
		    pjmedia_sdp_media *new_m;

		    /* Duplicate this media and apply secured transport */
		    new_m = pjmedia_sdp_media_clone(pool, m);
		    pj_strdup2(pool, &new_m->desc.transport, "RTP/SAVP");

		    /* Remove the "crypto" attribute in the unsecured media */
		    pjmedia_sdp_media_remove_all_attr(m, "crypto");

		    /* Insert the new media before the unsecured media */
		    if (sdp->media_count < PJMEDIA_MAX_SDP_MEDIA) {
			pj_array_insert(sdp->media, sizeof(new_m),
					sdp->media_count, i, &new_m);
			++sdp->media_count;
			++i;
		    }
		}
	    }
	}
    }
#endif

    call->rem_offerer = (rem_sdp != NULL);

    /* Notify application */
    if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_call_sdp_created) {
	(*pjsua_var.ua_cfg.cb.on_call_sdp_created)(call_id, sdp,
						   pool, rem_sdp);
    }

    *p_sdp = sdp;
    return PJ_SUCCESS;

on_error:
    if (sip_err_code && *sip_err_code == 0)
	*sip_err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;

    return status;
}


static void stop_media_stream(pjsua_call *call, unsigned med_idx)
{
    pjsua_call_media *call_med;
    
    if (pjsua_call_media_is_changing(call)) {
    	call_med = &call->media_prov[med_idx];
    	if (med_idx >= call->med_prov_cnt)
	    return;
    } else {
    	call_med = &call->media[med_idx];
        if (med_idx >= call->med_cnt)
	    return;
    }

    pj_log_push_indent();

    call_med->prev_type = call_med->type;
    if (call_med->type == PJMEDIA_TYPE_AUDIO) {
	pjsua_aud_stop_stream(call_med);
    }

#if PJMEDIA_HAS_VIDEO
    else if (call_med->type == PJMEDIA_TYPE_VIDEO) {
	pjsua_vid_stop_stream(call_med);
    }
#endif

    PJ_LOG(4,(THIS_FILE, "Media stream call%02d:%d is destroyed",
			 call->index, med_idx));
    call_med->prev_state = call_med->state;
    call_med->state = PJSUA_CALL_MEDIA_NONE;

    /* Try to sync recent changes to provisional media */
    if (med_idx < call->med_prov_cnt && 
	call->media_prov[med_idx].tp == call_med->tp)
    {
	pjsua_call_media *prov_med = &call->media_prov[med_idx];

	/* Media state */
	prov_med->prev_state = call_med->prev_state;
	prov_med->state	     = call_med->state;

	/* RTP seq/ts */
	prov_med->rtp_tx_seq_ts_set = call_med->rtp_tx_seq_ts_set;
	prov_med->rtp_tx_seq	    = call_med->rtp_tx_seq;
	prov_med->rtp_tx_ts	    = call_med->rtp_tx_ts;

	/* Saved media type and stream info */
	prov_med->prev_type = call_med->prev_type;
	prov_med->prev_aud_si = call_med->prev_aud_si;
	prov_med->prev_vid_si = call_med->prev_vid_si;

	/* Stream */
	if (call_med->type == PJMEDIA_TYPE_AUDIO) {
	    prov_med->strm.a.conf_slot = call_med->strm.a.conf_slot;
	    prov_med->strm.a.stream    = call_med->strm.a.stream;
	}
#if PJMEDIA_HAS_VIDEO
	else if (call_med->type == PJMEDIA_TYPE_VIDEO) {
	    prov_med->strm.v.cap_win_id = call_med->strm.v.cap_win_id;
	    prov_med->strm.v.rdr_win_id = call_med->strm.v.rdr_win_id;
	    prov_med->strm.v.stream	= call_med->strm.v.stream;
	}
#endif
    }

    pj_log_pop_indent();
}

static void stop_media_session(pjsua_call_id call_id)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    unsigned mi;

    for (mi=0; mi<call->med_cnt; ++mi) {
	stop_media_stream(call, mi);
    }
}


/*
 * Print log of call states. Since call states may be too long for logger,
 * printing it is a bit tricky, it should be printed part by part as long 
 * as the logger can accept.
 */
static void log_call_dump(int call_id) 
{
    pj_pool_t *pool;
    unsigned call_dump_len;
    unsigned part_len;
    unsigned part_idx;
    unsigned log_decor;
    char *buf;
    enum { BUF_LEN = 10*1024 };
    pj_status_t status;

    pool = pjsua_pool_create("tmp", 1024, 1024);
    buf = pj_pool_alloc(pool, sizeof(char) * BUF_LEN);

    status = pjsua_call_dump(call_id, PJ_TRUE, buf, BUF_LEN, "  ");
    if (status != PJ_SUCCESS)
	goto on_return;

    call_dump_len = (unsigned)pj_ansi_strlen(buf);

    log_decor = pj_log_get_decor();
    pj_log_set_decor(log_decor & ~(PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR));
    PJ_LOG(3,(THIS_FILE, "\n"));
    pj_log_set_decor(0);

    part_idx = 0;
    part_len = PJ_LOG_MAX_SIZE-80;
    while (part_idx < call_dump_len) {
	char p_orig, *p;

	p = buf + part_idx;
	if (part_idx + part_len > call_dump_len)
	    part_len = call_dump_len - part_idx;
	p_orig = p[part_len];
	p[part_len] = '\0';
	PJ_LOG(3,(THIS_FILE, "%s", p));
	p[part_len] = p_orig;
	part_idx += part_len;
    }
    pj_log_set_decor(log_decor);

on_return:
    if (pool)
	pj_pool_release(pool);
}


pj_status_t pjsua_media_channel_deinit(pjsua_call_id call_id)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjsip_dialog *dlg;
    unsigned mi;

    for (mi=0; mi<call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];

        if (call_med->tp_st == PJSUA_MED_TP_CREATING) {
            /* We will do the deinitialization after media transport
             * creation is completed.
             */
            call->async_call.med_ch_deinit = PJ_TRUE;
            return PJ_SUCCESS;
        }
    }

    PJ_LOG(4,(THIS_FILE, "Call %d: deinitializing media..", call_id));
    pj_log_push_indent();

    /* Print call dump first */
    dlg = (call->inv? call->inv->dlg : call->async_call.dlg);
    if (dlg)
    	log_call_dump(call_id);

    stop_media_session(call_id);

    /* Stop trickle ICE timer */
    if (call->trickle_ice.trickling > PJSUA_OP_STATE_NULL) {
	call->trickle_ice.trickling = PJSUA_OP_STATE_NULL;
	pjsua_cancel_timer(&call->trickle_ice.timer);
    }
    call->trickle_ice.enabled = PJ_FALSE;
    call->trickle_ice.pending_info = PJ_FALSE;
    call->trickle_ice.remote_sup = PJ_FALSE;
    call->trickle_ice.retrans18x_count = 0;

    /* Clean up media transports */
    pjsua_media_prov_clean_up(call_id);
    call->med_prov_cnt = 0;
    for (mi=0; mi<call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];

        if (call_med->tp_st > PJSUA_MED_TP_IDLE) {
    	    pjmedia_transport_info tpinfo;
    	    pjmedia_srtp_info *srtp_info;
    	    pjmedia_ice_transport_info *ice_info;

    	    pjmedia_transport_info_init(&tpinfo);
    	    pjmedia_transport_get_info(call_med->tp, &tpinfo);
    	    srtp_info = (pjmedia_srtp_info *)
    	    		pjmedia_transport_info_get_spc_info(
	            	    &tpinfo, PJMEDIA_TRANSPORT_TYPE_SRTP);
    	    ice_info = (pjmedia_ice_transport_info *)
    	    	       pjmedia_transport_info_get_spc_info(
	            	   &tpinfo, PJMEDIA_TRANSPORT_TYPE_ICE);

	    call_med->prev_srtp_use = (srtp_info && srtp_info->active)?
	    			      PJ_TRUE: PJ_FALSE;
	    if (call_med->prev_srtp_use)
	    	call_med->prev_srtp_info = *srtp_info;
	    call_med->prev_ice_use = (ice_info && ice_info->active)?
	    			     PJ_TRUE: PJ_FALSE;
	    if (call_med->prev_ice_use)
	    	call_med->prev_ice_info = *ice_info;

    	    /* Try to sync recent changes to provisional media */
    	    if (mi < call->med_prov_cnt && 
		call->media_prov[mi].tp == call_med->tp)
    	    {
		pjsua_call_media *prov_med = &call->media_prov[mi];

		prov_med->prev_ice_use = call_med->prev_ice_use;
		prov_med->prev_ice_info = call_med->prev_ice_info;
		prov_med->prev_srtp_use = call_med->prev_srtp_use;
		prov_med->prev_srtp_info = call_med->prev_srtp_info;
	    }

	    pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_IDLE);
	    pjmedia_transport_media_stop(call_med->tp);
	}

	if (call_med->tp) {
	    pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_NULL);
	    pjmedia_transport_close(call_med->tp);
	    call_med->tp = call_med->tp_orig = NULL;
	}
        call_med->tp_orig = NULL;
        call_med->rem_srtp_use = PJMEDIA_SRTP_UNKNOWN;
    }

    pj_log_pop_indent();

    return PJ_SUCCESS;
}


/* Match codec fmtp. This will compare the values and the order. */
static pj_bool_t match_codec_fmtp(const pjmedia_codec_fmtp *fmtp1,
				  const pjmedia_codec_fmtp *fmtp2)
{
    unsigned i;

    if (fmtp1->cnt != fmtp2->cnt)
	return PJ_FALSE;

    for (i = 0; i < fmtp1->cnt; ++i) {
	if (pj_stricmp(&fmtp1->param[i].name, &fmtp2->param[i].name))
	    return PJ_FALSE;
	if (pj_stricmp(&fmtp1->param[i].val, &fmtp2->param[i].val))
	    return PJ_FALSE;
    }

    return PJ_TRUE;
}

#if PJSUA_MEDIA_HAS_PJMEDIA || PJSUA_THIRD_PARTY_STREAM_HAS_GET_INFO

static pj_bool_t is_ice_running(pjmedia_transport *tp)
{
    pjmedia_transport_info tpinfo;
    pjmedia_ice_transport_info *ice_info;

    pjmedia_transport_info_init(&tpinfo);
    pjmedia_transport_get_info(tp, &tpinfo);
    ice_info = (pjmedia_ice_transport_info*)
	       pjmedia_transport_info_get_spc_info(&tpinfo,
						   PJMEDIA_TRANSPORT_TYPE_ICE);
    return (ice_info && ice_info->sess_state == PJ_ICE_STRANS_STATE_RUNNING);
}


#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)

static void check_srtp_roc(pjsua_call *call,
			   unsigned med_idx,
			   const pjsua_stream_info *new_si_,
    			   const pjmedia_sdp_media *local_sdp,
    			   const pjmedia_sdp_media *remote_sdp)
{
    pjsua_call_media *call_med = &call->media[med_idx];
    pjmedia_transport_info tpinfo;
    pjmedia_srtp_info *srtp_info;
    pjmedia_ice_transport_info *ice_info;
    const pjmedia_stream_info *prev_aud_si = NULL;
    pjmedia_stream_info aud_si;
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    const pjmedia_vid_stream_info *prev_vid_si = NULL;
    pjmedia_vid_stream_info vid_si;
#endif
    pj_bool_t local_change = PJ_FALSE, rem_change = PJ_FALSE;
    pjmedia_srtp_setting setting;

    pjmedia_transport_info_init(&tpinfo);
    pjmedia_transport_get_info(call_med->tp, &tpinfo);
    srtp_info = (pjmedia_srtp_info *) pjmedia_transport_info_get_spc_info(
	            &tpinfo, PJMEDIA_TRANSPORT_TYPE_SRTP);
    /* We are not using SRTP. */
    if (!srtp_info)
    	return;

    ice_info = (pjmedia_ice_transport_info*)
	       pjmedia_transport_info_get_spc_info(&tpinfo,
	           PJMEDIA_TRANSPORT_TYPE_ICE);

    /* RFC 3711 section 3.3.1: 
     * After a re-keying occurs (changing to a new master key), the rollover
     * counter always maintains its sequence of values, i.e., it MUST NOT be
     * reset to zero. 
     *
     * RFC 4568 section 7.1.4:
     * If the offerer includes an IP address and/or port that differs from
     * that used previously for a media stream (or FEC stream), the offerer
     * MUST include a new master key with the offer (and in so doing, it
     * will be creating a new crypto context where the ROC is set to zero).
     * Similarly, if the answerer includes an IP address and/or port that
     * differs from that used previously for a media stream (or FEC stream),
     * the answerer MUST include a new master key with the answer (and hence
     * create a new crypto context with the ROC set to zero).
     */
    if (call->opt.flag & PJSUA_CALL_REINIT_MEDIA) {
    	if (!call_med->prev_srtp_use) return;
    	
    	/* The stream has been deinitialized by now, so we need to retrieve
    	 * the previous stream info from the stored data.
    	 */
        if (call_med->prev_type == PJMEDIA_TYPE_AUDIO)
            prev_aud_si = &call_med->prev_aud_si;
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
	else if (call_med->prev_type == PJMEDIA_TYPE_VIDEO)
	    prev_vid_si = &call_med->prev_vid_si;
#endif
    } else {
    	call_med->prev_srtp_use = PJ_TRUE;
	call_med->prev_srtp_info = *srtp_info;
	call_med->prev_ice_use = (ice_info && ice_info->active)?
	    			 PJ_TRUE: PJ_FALSE;
	if (call_med->prev_ice_use)
	    call_med->prev_ice_info = *ice_info;

    	if (call_med->type == PJMEDIA_TYPE_AUDIO) {
	    /* Get current active audio stream info */
	    if (call_med->strm.a.stream) {
	        pjmedia_stream_get_info(call_med->strm.a.stream, &aud_si);
	        prev_aud_si = &aud_si;
	    }
    	} 
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    	else if (call_med->type == PJMEDIA_TYPE_VIDEO) {
	    /* Get current active video stream info */
	    if (call_med->strm.v.stream) {
	        pjmedia_vid_stream_get_info(call_med->strm.v.stream, &vid_si);
	        prev_vid_si = &vid_si;
	    }
	}
#endif
    }
    
#if 0
    PJ_LOG(4, (THIS_FILE, "SRTP TX ROC %d %d",
    			  call_med->prev_srtp_info.tx_roc.ssrc,
    			  call_med->prev_srtp_info.tx_roc.roc));
    PJ_LOG(4, (THIS_FILE, "SRTP RX ROC %d %d",
    			  call_med->prev_srtp_info.rx_roc.ssrc,
    			  call_med->prev_srtp_info.rx_roc.roc));
#endif
    
    if (prev_aud_si) {
	const pjmedia_stream_info *new_si = &new_si_->info.aud;

	/* Local IP address changes */
	if (pj_sockaddr_cmp(&prev_aud_si->local_addr, &new_si->local_addr))
	    local_change = PJ_TRUE;
	/* Remote IP address changes */
	if (pj_sockaddr_cmp(&prev_aud_si->rem_addr, &new_si->rem_addr))
	    rem_change = PJ_TRUE;
    }
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    if (prev_vid_si) {
	const pjmedia_vid_stream_info *new_si = &new_si_->info.vid;
	
	/* Local IP address changes */
	if (pj_sockaddr_cmp(&prev_vid_si->local_addr, &new_si->local_addr))
	    local_change = PJ_TRUE;
	/* Remote IP address changes */
	if (pj_sockaddr_cmp(&prev_vid_si->rem_addr, &new_si->rem_addr))
	    rem_change = PJ_TRUE;
    }
#endif

    /* There are some complications if we are using ICE, because default
     * IP address can change after negotiation. In this case, we'll consider
     * this as a change of IP address only if ICE is restarted as well
     * (i.e. if ufrag changes).
     */
    if (ice_info && call_med->prev_ice_use) {
	const pj_str_t STR_ICE_UFRAG = { "ice-ufrag", 9 };
	pjmedia_sdp_attr *attr;

	if (local_change) {
	    attr = pjmedia_sdp_attr_find(local_sdp->attr_count,
	    				 local_sdp->attr, &STR_ICE_UFRAG,
	    				 NULL);
	    if (!pj_strcmp(&call_med->prev_ice_info.loc_ufrag,
	    		   &attr->value))
	    {
	    	PJ_LOG(4, (THIS_FILE, "ICE unchanged, SRTP TX ROC "
	    	    		      "maintained"));
	    	local_change = PJ_FALSE;
	    }
	}

	if (rem_change) {
	    attr = pjmedia_sdp_attr_find(remote_sdp->attr_count,
	    				 remote_sdp->attr, &STR_ICE_UFRAG,
	    				 NULL);
	    if (!pj_strcmp(&call_med->prev_ice_info.rem_ufrag,
	    		   &attr->value))
	    {
	    	PJ_LOG(4, (THIS_FILE, "ICE unchanged, SRTP RX ROC "
	    	    		      "maintained"));
	    	rem_change = PJ_FALSE;
	    }
	 }	    
    }

    pjmedia_transport_srtp_get_setting(call_med->tp, &setting);
    setting.tx_roc = call_med->prev_srtp_info.tx_roc;
    setting.rx_roc = call_med->prev_srtp_info.rx_roc;
    if (local_change) {
	PJ_LOG(4, (THIS_FILE, "Local address change detected, "
			      "resetting SRTP TX ROC"));
    	setting.tx_roc.roc = 0;
    	/* Depending on the interpretation of the RFC, remote
    	 * may or may not reset its ROC as well. So we anticipate both.
    	 */
    	setting.prev_rx_roc = call_med->prev_srtp_info.rx_roc;
    	setting.prev_rx_roc.roc = 0;
    }
    if (rem_change) {
	PJ_LOG(4, (THIS_FILE, "Remote address change detected, "
			      "resetting SRTP RX ROC"));
    	setting.rx_roc.roc = 0;
    	/* There is a possibility that remote's IP address in the SDP
    	 * changes, but its actual IP address actually doesn't change,
    	 * such as when using ICE. So the only thing we can do here
    	 * is to anticipate if remote doesn't reset the ROC.
    	 */
    	setting.prev_rx_roc = call_med->prev_srtp_info.rx_roc;
#if PJSUA_RESET_SRTP_ROC_ON_REM_ADDRESS_CHANGE
 	if (!local_change) {
	    PJ_LOG(4, (THIS_FILE, "Remote address change detected, "
			      	  "resetting SRTP TX ROC"));
 	    setting.tx_roc.roc = 0;
 	}
#endif 
    }
    pjmedia_transport_srtp_modify_setting(call_med->tp, &setting);
}
#endif

static pj_bool_t is_media_changed(const pjsua_call *call,
				  unsigned med_idx,
				  const pjsua_stream_info *new_si_)
{
    const pjsua_call_media *call_med = &call->media[med_idx];

    /* Check for newly added media */
    if (med_idx >= call->med_cnt)
	return PJ_TRUE;

    /* Compare media type */
    if (call_med->type != new_si_->type)
	return PJ_TRUE;

    /* Audio update checks */
    if (call_med->type == PJMEDIA_TYPE_AUDIO) {
	pjmedia_stream_info the_old_si;
	const pjmedia_stream_info *old_si = NULL;
	const pjmedia_stream_info *new_si = &new_si_->info.aud;
	const pjmedia_codec_info *old_ci = NULL;
	const pjmedia_codec_info *new_ci = &new_si->fmt;
	const pjmedia_codec_param *old_cp = NULL;
	const pjmedia_codec_param *new_cp = new_si->param;

	/* Compare media direction */
	if (call_med->dir != new_si->dir)
	    return PJ_TRUE;

	/* Get current active stream info */
	if (call_med->strm.a.stream) {
	    pjmedia_stream_get_info(call_med->strm.a.stream, &the_old_si);
	    old_si = &the_old_si;
	    old_ci = &old_si->fmt;
	    old_cp = old_si->param;
	} else {
	    /* The stream is inactive. */
	    return (new_si->dir != PJMEDIA_DIR_NONE);
	}

	if (old_si->rtcp_mux != new_si->rtcp_mux)
	    return PJ_TRUE;

	/* Compare remote RTP address. If ICE is running, change in default
	 * address can happen after negotiation, this can be handled
	 * internally by ICE and does not need to cause media restart.
	 */
	if (!is_ice_running(call_med->tp) &&
	    pj_sockaddr_cmp(&old_si->rem_addr, &new_si->rem_addr))
	{
	    return PJ_TRUE;
	}

	/* Compare codec info */
	if (pj_stricmp(&old_ci->encoding_name, &new_ci->encoding_name) ||
	    old_ci->clock_rate != new_ci->clock_rate ||
	    old_ci->channel_cnt != new_ci->channel_cnt ||
	    old_si->rx_pt != new_si->rx_pt ||
	    old_si->tx_pt != new_si->tx_pt ||
	    old_si->rx_event_pt != new_si->tx_event_pt ||
	    old_si->tx_event_pt != new_si->tx_event_pt)
	{
	    return PJ_TRUE;
	}

	/* Compare codec param */
	if (old_cp->setting.frm_per_pkt != new_cp->setting.frm_per_pkt ||
	    old_cp->setting.vad != new_cp->setting.vad ||
	    old_cp->setting.cng != new_cp->setting.cng ||
	    old_cp->setting.plc != new_cp->setting.plc ||
	    old_cp->setting.penh != new_cp->setting.penh ||
	    !match_codec_fmtp(&old_cp->setting.dec_fmtp,
			      &new_cp->setting.dec_fmtp) ||
	    !match_codec_fmtp(&old_cp->setting.enc_fmtp,
			      &new_cp->setting.enc_fmtp))
	{
	    return PJ_TRUE;
	}
    }

#if PJMEDIA_HAS_VIDEO
    else if (call_med->type == PJMEDIA_TYPE_VIDEO) {
	pjmedia_vid_stream_info the_old_si;
	const pjmedia_vid_stream_info *old_si = NULL;
	const pjmedia_vid_stream_info *new_si = &new_si_->info.vid;
	const pjmedia_vid_codec_info *old_ci = NULL;
	const pjmedia_vid_codec_info *new_ci = &new_si->codec_info;
	const pjmedia_vid_codec_param *old_cp = NULL;
	const pjmedia_vid_codec_param *new_cp = new_si->codec_param;

	/* Compare media direction */
	if (call_med->dir != new_si->dir)
	    return PJ_TRUE;

	/* Get current active stream info */
	if (call_med->strm.v.stream) {
	    pjmedia_vid_stream_get_info(call_med->strm.v.stream, &the_old_si);
	    old_si = &the_old_si;
	    old_ci = &old_si->codec_info;
	    old_cp = old_si->codec_param;
	} else {
	    /* The stream is inactive. */
	    return (new_si->dir != PJMEDIA_DIR_NONE);
	}

	/* Compare remote RTP address. If ICE is running, change in default
	 * address can happen after negotiation, this can be handled
	 * internally by ICE and does not need to cause media restart.
	 */
	if (old_si->rtcp_mux != new_si->rtcp_mux)
	    return PJ_TRUE;
	if (!is_ice_running(call_med->tp) &&
	    pj_sockaddr_cmp(&old_si->rem_addr, &new_si->rem_addr))
	{
	    return PJ_TRUE;
	}

	/* Compare codec info */
	if (pj_stricmp(&old_ci->encoding_name, &new_ci->encoding_name) ||
	    old_si->rx_pt != new_si->rx_pt ||
	    old_si->tx_pt != new_si->tx_pt)
	{
	    return PJ_TRUE;
	}

	/* Compare codec param */
	if (/* old_cp->enc_mtu != new_cp->enc_mtu || */
	    pj_memcmp(&old_cp->enc_fmt.det, &new_cp->enc_fmt.det,
		      sizeof(pjmedia_video_format_detail)) ||
	    !match_codec_fmtp(&old_cp->dec_fmtp, &new_cp->dec_fmtp) ||
	    !match_codec_fmtp(&old_cp->enc_fmtp, &new_cp->enc_fmtp))
	{
	    return PJ_TRUE;
	}
    }

#endif

    else {
	/* Just return PJ_TRUE for other media type */
	return PJ_TRUE;
    }

    return PJ_FALSE;
}

#else /* PJSUA_MEDIA_HAS_PJMEDIA || PJSUA_THIRD_PARTY_STREAM_HAS_GET_INFO */

static pj_bool_t is_media_changed(const pjsua_call *call,
				  unsigned med_idx,
				  const pjsua_stream_info *new_si_)
{
    PJ_UNUSED_ARG(call);
    PJ_UNUSED_ARG(med_idx);
    PJ_UNUSED_ARG(new_si_);
    /* Always assume that media has been changed */
    return PJ_TRUE;
}

#endif /* PJSUA_MEDIA_HAS_PJMEDIA || PJSUA_THIRD_PARTY_STREAM_HAS_GET_INFO */


pj_status_t pjsua_media_channel_update(pjsua_call_id call_id,
				       const pjmedia_sdp_session *local_sdp,
				       const pjmedia_sdp_session *remote_sdp)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjsua_acc *acc = &pjsua_var.acc[call->acc_id];
    pj_pool_t *tmp_pool = call->inv->pool_prov;
    unsigned mi;
    pj_bool_t got_media = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;

    const pj_str_t STR_AUDIO = { "audio", 5 };
    const pj_str_t STR_VIDEO = { "video", 5 };
    pj_uint8_t maudidx[PJSUA_MAX_CALL_MEDIA];
    unsigned maudcnt = PJ_ARRAY_SIZE(maudidx);
    unsigned mtotaudcnt = PJ_ARRAY_SIZE(maudidx);
    pj_uint8_t mvididx[PJSUA_MAX_CALL_MEDIA];
    unsigned mvidcnt = PJ_ARRAY_SIZE(mvididx);
    unsigned mtotvidcnt = PJ_ARRAY_SIZE(mvididx);
    pj_bool_t need_renego_sdp = PJ_FALSE;

    if (pjsua_get_state() != PJSUA_STATE_RUNNING)
	return PJ_EBUSY;

    PJ_LOG(4,(THIS_FILE, "Call %d: updating media..", call_id));
    pj_log_push_indent();

    /* Destroy existing media session, if any. */
    //stop_media_session(call->index);

    /* Call media count must be at least equal to SDP media. Note that
     * it may not be equal when remote removed any SDP media line.
     */
    pj_assert(call->med_prov_cnt >= local_sdp->media_count);

    /* Reset audio_idx first */
    call->audio_idx = -1;

    /* Sort audio/video based on "quality" */
    sort_media(local_sdp, &STR_AUDIO, acc->cfg.use_srtp,
	       maudidx, &maudcnt, &mtotaudcnt);
#if PJMEDIA_HAS_VIDEO
    sort_media(local_sdp, &STR_VIDEO, acc->cfg.use_srtp,
	       mvididx, &mvidcnt, &mtotvidcnt);
#else
    PJ_UNUSED_ARG(STR_VIDEO);
    mvidcnt = mtotvidcnt = 0;
#endif

    /* We need to re-nego SDP or modify our answer when:
     * - media count exceeds the configured limit,
     * - RTCP-FB is enabled (so a=rtcp-fb will only be printed for negotiated
     *   codecs)
     */
    if (!pjmedia_sdp_neg_was_answer_remote(call->inv->neg) &&
	((maudcnt > call->opt.aud_cnt || mvidcnt > call->opt.vid_cnt) ||
	(acc->cfg.rtcp_fb_cfg.cap_count)))
    {
	pjmedia_sdp_session *local_sdp_renego = NULL;

	local_sdp_renego = pjmedia_sdp_session_clone(tmp_pool, local_sdp);
	local_sdp = local_sdp_renego;
	need_renego_sdp = PJ_TRUE;

	/* Add RTCP-FB info into local SDP answer */
	if (acc->cfg.rtcp_fb_cfg.cap_count) {
	    for (mi=0; mi < local_sdp_renego->media_count; ++mi) {
		status = pjmedia_rtcp_fb_encode_sdp(
					tmp_pool, pjsua_var.med_endpt,
					&acc->cfg.rtcp_fb_cfg,
					local_sdp_renego, mi, remote_sdp);
		if (status != PJ_SUCCESS) {
		    PJ_PERROR(3,(THIS_FILE, status,
				 "Call %d media %d: Failed to encode RTCP-FB "
				 "setting to SDP",
				 call_id, mi));
		}
	    }
	}

	/* Applying media count limitation. Note that in generating SDP
	 * answer, no media count limitation applied as we didn't know yet
	 * which media would pass the SDP negotiation.
	 */
	if (maudcnt > call->opt.aud_cnt || mvidcnt > call->opt.vid_cnt)
	{
	    maudcnt = PJ_MIN(maudcnt, call->opt.aud_cnt);
	    mvidcnt = PJ_MIN(mvidcnt, call->opt.vid_cnt);

	    for (mi=0; mi < local_sdp_renego->media_count; ++mi) {
		pjmedia_sdp_media *m = local_sdp_renego->media[mi];

		if (m->desc.port == 0 ||
		    pj_memchr(maudidx, mi, maudcnt*sizeof(maudidx[0])) ||
		    pj_memchr(mvididx, mi, mvidcnt*sizeof(mvididx[0])))
		{
		    continue;
		}
    	    
		/* Deactivate this excess media */
		pjmedia_sdp_media_deactivate(tmp_pool, m);
	    }
	}
    }

    /* Update call media from provisional media */
    call->med_cnt = call->med_prov_cnt;
    pj_memcpy(call->media, call->media_prov,
	      sizeof(call->media_prov[0]) * call->med_prov_cnt);

    /* Process each media stream */
    for (mi=0; mi < call->med_cnt; ++mi) {
    	const char *STR_SENDRECV = "sendrecv";
    	const char *STR_SENDONLY = "sendonly";
     	const char *STR_RECVONLY = "recvonly";
     	const char *STR_INACTIVE = "inactive";
     	pjsua_call_media *call_med = &call->media[mi];
	pj_bool_t media_changed = PJ_FALSE;

	if (mi >= local_sdp->media_count ||
	    mi >= remote_sdp->media_count)
	{
	    /* This may happen when remote removed any SDP media lines in
	     * its re-offer.
	     */

	    /* Stop stream */
	    stop_media_stream(call, mi);

	    /* Close the media transport */
	    if (call_med->tp) {
		pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_NULL);
		pjmedia_transport_close(call_med->tp);
		call_med->tp = call_med->tp_orig = NULL;
	    }
	    continue;
#if 0
	    /* Something is wrong */
	    PJ_LOG(1,(THIS_FILE, "Error updating media for call %d: "
		      "invalid media index %d in SDP", call_id, mi));
	    status = PJMEDIA_SDP_EINSDP;
	    goto on_error;
#endif
	}

	/* Apply media update action */
	if (call_med->type==PJMEDIA_TYPE_AUDIO) {
	    pjmedia_stream_info the_si, *si = &the_si;
	    pjsua_stream_info stream_info;

	    status = pjmedia_stream_info_from_sdp(
					si, tmp_pool, pjsua_var.med_endpt,
	                                local_sdp, remote_sdp, mi);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(1,(THIS_FILE, status,
			     "pjmedia_stream_info_from_sdp() failed "
			         "for call_id %d media %d",
			     call_id, mi));
		goto on_check_med_status;
	    }

	    /* Check if remote wants RTP and RTCP multiplexing,
	     * but we don't enable it.
	     */
	    if (si->rtcp_mux && !call_med->enable_rtcp_mux) {
	        si->rtcp_mux = PJ_FALSE;
	    }

            /* Codec parameter of stream info (si->param) can be NULL if
             * the stream is rejected or disabled.
             */
	    /* Override ptime, if this option is specified. */
	    if (pjsua_var.media_cfg.ptime != 0 && si->param) {
	        si->param->setting.frm_per_pkt = (pj_uint8_t)
		    (pjsua_var.media_cfg.ptime / si->param->info.frm_ptime);
	        if (si->param->setting.frm_per_pkt == 0)
		    si->param->setting.frm_per_pkt = 1;
	    }

	    /* Disable VAD, if this option is specified. */
	    if (pjsua_var.media_cfg.no_vad && si->param) {
	        si->param->setting.vad = 0;
	    }

	    if (!pjmedia_sdp_neg_was_answer_remote(call->inv->neg) &&
	    	si->dir != PJMEDIA_DIR_NONE)
	    {
    		pjmedia_dir dir = si->dir;
    		
    		if (call->opt.flag & PJSUA_CALL_SET_MEDIA_DIR) {
    		    call_med->def_dir = call->opt.media_dir[mi];
    		    PJ_LOG(4,(THIS_FILE, "Call %d: setting audio media "
    		    			 "direction #%d to %d.",
			  		 call_id, mi, call_med->def_dir));
    		}

 		/* If the default direction specifies we do not wish
 		 * encoding/decoding, clear that direction.
 		 */
     		if ((call_med->def_dir & PJMEDIA_DIR_ENCODING) == 0) {
 		    dir &= ~PJMEDIA_DIR_ENCODING;
     		}
     		if ((call_med->def_dir & PJMEDIA_DIR_DECODING) == 0) {
 		    dir &= ~PJMEDIA_DIR_DECODING;
     		}

     		if (dir != si->dir) {
     		    const char *str_attr = NULL;
 	    	    pjmedia_sdp_attr *attr;
 	    	    pjmedia_sdp_media *m;

     		    if (!need_renego_sdp) {
 			pjmedia_sdp_session *local_sdp_renego;
 			local_sdp_renego =
 			    pjmedia_sdp_session_clone(tmp_pool, local_sdp);
 			local_sdp = local_sdp_renego;
 			need_renego_sdp = PJ_TRUE;
     		    }

     		    si->dir = dir;
     		    m = local_sdp->media[mi];

 	    	    /* Remove existing directions attributes */
 	    	    pjmedia_sdp_media_remove_all_attr(m, STR_SENDRECV);
 	    	    pjmedia_sdp_media_remove_all_attr(m, STR_SENDONLY);
 	    	    pjmedia_sdp_media_remove_all_attr(m, STR_RECVONLY);

 		    if (si->dir == PJMEDIA_DIR_ENCODING_DECODING) {
 		    	str_attr = STR_SENDRECV;
 		    } else if (si->dir == PJMEDIA_DIR_ENCODING) {
 		    	str_attr = STR_SENDONLY;
 		    } else if (si->dir == PJMEDIA_DIR_DECODING) {
 		    	str_attr = STR_RECVONLY;
 		    } else {
 		    	str_attr = STR_INACTIVE;
 		    }
 		    attr = pjmedia_sdp_attr_create(tmp_pool, str_attr, NULL);
 		    pjmedia_sdp_media_add_attr(m, attr);
 		}
     	    }

	    stream_info.type = PJMEDIA_TYPE_AUDIO;
	    stream_info.info.aud = the_si;

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
	    /* Check if we need to reset or maintain SRTP ROC */
	    check_srtp_roc(call, mi, &stream_info,
	    		   local_sdp->media[mi], remote_sdp->media[mi]);
#endif

	    /* Check if this media is changed */
	    if (pjsua_var.media_cfg.no_smart_media_update ||
		is_media_changed(call, mi, &stream_info))
	    {
		media_changed = PJ_TRUE;
		/* Stop the media */
		stop_media_stream(call, mi);
	    } else {
		PJ_LOG(4,(THIS_FILE, "Call %d: stream #%d (audio) unchanged.",
			  call_id, mi));
	    }

	    /* Check if no media is active */
	    if (local_sdp->media[mi]->desc.port == 0) {

		/* Update call media state and direction */
		call_med->state = PJSUA_CALL_MEDIA_NONE;
		call_med->dir = PJMEDIA_DIR_NONE;

	    } else if (call_med->tp) {
		pjmedia_transport_info tp_info;
		pjmedia_srtp_info *srtp_info;

		/* Call media direction */
		call_med->dir = si->dir;

		/* Call media state */
		if (call->local_hold)
		    call_med->state = PJSUA_CALL_MEDIA_LOCAL_HOLD;
		else if (call_med->dir == PJMEDIA_DIR_DECODING)
		    call_med->state = PJSUA_CALL_MEDIA_REMOTE_HOLD;
		else
		    call_med->state = PJSUA_CALL_MEDIA_ACTIVE;

		if (call->inv->following_fork) {
		    unsigned options = (call_med->enable_rtcp_mux?
            			        PJMEDIA_TPMED_RTCP_MUX: 0);
		    /* Normally media transport will automatically restart
		     * itself (if needed, based on info from the SDP) in
		     * pjmedia_transport_media_start(), however in "following
		     * forked media" case (see #1644), we need to explicitly
		     * restart it as it cannot detect fork scenario from
		     * the SDP only.
		     */
		    status = pjmedia_transport_media_stop(call_med->tp);
		    if (status != PJ_SUCCESS) {
			PJ_PERROR(1,(THIS_FILE, status,
				     "pjmedia_transport_media_stop() failed "
				     "for call_id %d media %d",
				     call_id, mi));
			goto on_check_med_status;
		    }
		    status = pjmedia_transport_media_create(call_med->tp,
							    tmp_pool,
							    options, NULL, mi);
		    if (status != PJ_SUCCESS) {
			PJ_PERROR(1,(THIS_FILE, status,
				     "pjmedia_transport_media_create() failed "
				     "for call_id %d media %d",
				     call_id, mi));
			goto on_check_med_status;
		    }
		}

		/* Start/restart media transport based on info in SDP */
		status = pjmedia_transport_media_start(call_med->tp,
						       tmp_pool, local_sdp,
						       remote_sdp, mi);
		if (status != PJ_SUCCESS) {
		    PJ_PERROR(1,(THIS_FILE, status,
				 "pjmedia_transport_media_start() failed "
				     "for call_id %d media %d",
				 call_id, mi));
		    goto on_check_med_status;
		}

		pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_RUNNING);

		/* Get remote SRTP usage policy */
		pjmedia_transport_info_init(&tp_info);
		pjmedia_transport_get_info(call_med->tp, &tp_info);
		srtp_info = (pjmedia_srtp_info*)
			    pjmedia_transport_info_get_spc_info(
				    &tp_info, PJMEDIA_TRANSPORT_TYPE_SRTP);
		if (srtp_info) {
		    call_med->rem_srtp_use = srtp_info->peer_use;
		}

		/* Update audio channel */
		if (media_changed) {
		    status = pjsua_aud_channel_update(call_med,
						      call->inv->pool, si,
						      local_sdp, remote_sdp);
		    if (status != PJ_SUCCESS) {
			PJ_PERROR(1,(THIS_FILE, status,
				     "pjsua_aud_channel_update() failed "
					 "for call_id %d media %d",
				     call_id, mi));
			goto on_check_med_status;
		    }

		    if (pjmedia_transport_info_get_spc_info(
				    &tp_info, PJMEDIA_TRANSPORT_TYPE_LOOP))
		    {
			pjmedia_transport_loop_disable_rx(
				call_med->tp, call_med->strm.a.stream,
				!acc->cfg.enable_loopback);
		    }
		}
	    }

	    /* Print info. */
	    if (status == PJ_SUCCESS) {
		char info[80];
		int info_len = 0;
		int len;
		const char *dir;

		switch (si->dir) {
		case PJMEDIA_DIR_NONE:
		    dir = "inactive";
		    break;
		case PJMEDIA_DIR_ENCODING:
		    dir = "sendonly";
		    break;
		case PJMEDIA_DIR_DECODING:
		    dir = "recvonly";
		    break;
		case PJMEDIA_DIR_ENCODING_DECODING:
		    dir = "sendrecv";
		    break;
		default:
		    dir = "unknown";
		    break;
		}
		len = pj_ansi_sprintf( info+info_len,
				       ", stream #%d: %.*s (%s)", mi,
				       (int)si->fmt.encoding_name.slen,
				       si->fmt.encoding_name.ptr,
				       dir);
		if (len > 0)
		    info_len += len;
		PJ_LOG(4,(THIS_FILE,"Audio updated%s", info));
	    }


	    if (call->audio_idx==-1 && status==PJ_SUCCESS &&
		si->dir != PJMEDIA_DIR_NONE)
	    {
		call->audio_idx = mi;
	    }

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
	} else if (call_med->type==PJMEDIA_TYPE_VIDEO) {
	    pjmedia_vid_stream_info the_si, *si = &the_si;
	    pjsua_stream_info stream_info;

	    status = pjmedia_vid_stream_info_from_sdp(
					si, tmp_pool, pjsua_var.med_endpt,
					local_sdp, remote_sdp, mi);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(1,(THIS_FILE, status,
			     "pjmedia_vid_stream_info_from_sdp() failed "
			         "for call_id %d media %d",
			     call_id, mi));
		goto on_check_med_status;
	    }

	    /* Check if remote wants RTP and RTCP multiplexing,
	     * but we don't enable it.
	     */
	    if (si->rtcp_mux && !call_med->enable_rtcp_mux) {
	        si->rtcp_mux = PJ_FALSE;
	    }

	    if (!pjmedia_sdp_neg_was_answer_remote(call->inv->neg) &&
	    	si->dir != PJMEDIA_DIR_NONE)
	    {
    		pjmedia_dir dir = si->dir;
    		
    		if (call->opt.flag & PJSUA_CALL_SET_MEDIA_DIR) {
    		    call_med->def_dir = call->opt.media_dir[mi];
    		    PJ_LOG(4,(THIS_FILE, "Call %d: setting video media "
    		    			 "direction #%d to %d.",
			  		 call_id, mi, call_med->def_dir));
    		}

 		/* If the default direction specifies we do not wish
 		 * encoding/decoding, clear that direction.
 		 */
     		if ((call_med->def_dir & PJMEDIA_DIR_ENCODING) == 0) {
 		    dir &= ~PJMEDIA_DIR_ENCODING;
     		}
     		if ((call_med->def_dir & PJMEDIA_DIR_DECODING) == 0) {
 		    dir &= ~PJMEDIA_DIR_DECODING;
     		}

     		if (dir != si->dir) {
     		    const char *str_attr = NULL;
 	    	    pjmedia_sdp_attr *attr;
 	    	    pjmedia_sdp_media *m;

     		    if (!need_renego_sdp) {
 			pjmedia_sdp_session *local_sdp_renego;
 			local_sdp_renego =
 			    pjmedia_sdp_session_clone(tmp_pool, local_sdp);
 			local_sdp = local_sdp_renego;
 			need_renego_sdp = PJ_TRUE;
     		    }

     		    si->dir = dir;
     		    m = local_sdp->media[mi];

 	    	    /* Remove existing directions attributes */
 	    	    pjmedia_sdp_media_remove_all_attr(m, STR_SENDRECV);
 	    	    pjmedia_sdp_media_remove_all_attr(m, STR_SENDONLY);
 	    	    pjmedia_sdp_media_remove_all_attr(m, STR_RECVONLY);

 		    if (si->dir == PJMEDIA_DIR_ENCODING_DECODING) {
 		    	str_attr = STR_SENDRECV;
 		    } else if (si->dir == PJMEDIA_DIR_ENCODING) {
 		    	str_attr = STR_SENDONLY;
 		    } else if (si->dir == PJMEDIA_DIR_DECODING) {
 		    	str_attr = STR_RECVONLY;
 		    } else {
 		    	str_attr = STR_INACTIVE;
 		    }
 		    attr = pjmedia_sdp_attr_create(tmp_pool, str_attr, NULL);
 		    pjmedia_sdp_media_add_attr(m, attr);
 		}
     	    }

	    /* Check if this media is changed */
	    stream_info.type = PJMEDIA_TYPE_VIDEO;
	    stream_info.info.vid = the_si;
	    if (is_media_changed(call, mi, &stream_info)) {
		media_changed = PJ_TRUE;
		/* Stop the media */
		stop_media_stream(call, mi);
	    } else {
		PJ_LOG(4,(THIS_FILE, "Call %d: stream #%d (video) unchanged.",
			  call_id, mi));
	    }

	    /* Check if no media is active */
	    if (si->dir == PJMEDIA_DIR_NONE) {

		/* Update call media state and direction */
		call_med->state = PJSUA_CALL_MEDIA_NONE;
		call_med->dir = PJMEDIA_DIR_NONE;

	    } else if (call_med->tp) {
		pjmedia_transport_info tp_info;
		pjmedia_srtp_info *srtp_info;

		/* Call media direction */
		call_med->dir = si->dir;

		/* Call media state */
		if (call->local_hold)
		    call_med->state = PJSUA_CALL_MEDIA_LOCAL_HOLD;
		else if (call_med->dir == PJMEDIA_DIR_DECODING)
		    call_med->state = PJSUA_CALL_MEDIA_REMOTE_HOLD;
		else
		    call_med->state = PJSUA_CALL_MEDIA_ACTIVE;

		/* Start/restart media transport */
		status = pjmedia_transport_media_start(call_med->tp,
						       tmp_pool, local_sdp,
						       remote_sdp, mi);
		if (status != PJ_SUCCESS) {
		    PJ_PERROR(1,(THIS_FILE, status,
				 "pjmedia_transport_media_start() failed "
				     "for call_id %d media %d",
				 call_id, mi));
		    goto on_check_med_status;
		}

		pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_RUNNING);

		/* Get remote SRTP usage policy */
		pjmedia_transport_info_init(&tp_info);
		pjmedia_transport_get_info(call_med->tp, &tp_info);
		srtp_info = (pjmedia_srtp_info*)
			    pjmedia_transport_info_get_spc_info(
				    &tp_info, PJMEDIA_TRANSPORT_TYPE_SRTP);
		if (srtp_info) {
		    call_med->rem_srtp_use = srtp_info->peer_use;
		}

		/* Update video channel */
		if (media_changed) {
		    status = pjsua_vid_channel_update(call_med,
						      call->inv->pool, si,
						      local_sdp, remote_sdp);
		    if (status != PJ_SUCCESS) {
			PJ_PERROR(1,(THIS_FILE, status,
				     "pjsua_vid_channel_update() failed "
					 "for call_id %d media %d",
				     call_id, mi));
			goto on_check_med_status;
		    }
		}
	    }

	    /* Print info. */
	    {
		char info[80];
		int info_len = 0;
		int len;
		const char *dir;

		switch (si->dir) {
		case PJMEDIA_DIR_NONE:
		    dir = "inactive";
		    break;
		case PJMEDIA_DIR_ENCODING:
		    dir = "sendonly";
		    break;
		case PJMEDIA_DIR_DECODING:
		    dir = "recvonly";
		    break;
		case PJMEDIA_DIR_ENCODING_DECODING:
		    dir = "sendrecv";
		    break;
		default:
		    dir = "unknown";
		    break;
		}
		len = pj_ansi_sprintf( info+info_len,
				       ", stream #%d: %.*s (%s)", mi,
				       (int)si->codec_info.encoding_name.slen,
				       si->codec_info.encoding_name.ptr,
				       dir);
		if (len > 0)
		    info_len += len;
		PJ_LOG(4,(THIS_FILE,"Video updated%s", info));
	    }

#endif
	} else {
	    status = PJMEDIA_EUNSUPMEDIATYPE;
	}

	/* Close the transport of deactivated media, need this here as media
	 * can be deactivated by the SDP negotiation and the max media count
	 * (account) setting.
	 */
	if (local_sdp->media[mi]->desc.port==0 && call_med->tp) {
	    pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_NULL);
	    pjmedia_transport_close(call_med->tp);
	    call_med->tp = call_med->tp_orig = NULL;
	}

on_check_med_status:
	if (status != PJ_SUCCESS) {
	    /* Stop stream */
	    stop_media_stream(call, mi);

	    /* Close the media transport */
	    if (call_med->tp) {
		pjsua_set_media_tp_state(call_med, PJSUA_MED_TP_NULL);
		pjmedia_transport_close(call_med->tp);
		call_med->tp = call_med->tp_orig = NULL;
	    }

	    /* Update media states */
	    call_med->state = PJSUA_CALL_MEDIA_ERROR;
	    call_med->dir = PJMEDIA_DIR_NONE;

	    if (status != PJMEDIA_EUNSUPMEDIATYPE) {
		PJ_PERROR(1, (THIS_FILE, status, "Error updating media "
		              "call%02d:%d", call_id, mi));
	    } else {
		PJ_PERROR(3, (THIS_FILE, status, "Skipped updating media "
		              "call%02d:%d (media type=%s)", call_id, mi, 
			      pjmedia_type_name(call_med->type)));
	    }

	} else {
	    /* Only set 'got_media' flag if this media is not disabled */
	    if (local_sdp->media[mi]->desc.port != 0)
		got_media = PJ_TRUE;
	}
    }

    /* Sync provisional media to call media */
    call->med_prov_cnt = call->med_cnt;
    pj_memcpy(call->media_prov, call->media,
	      sizeof(call->media[0]) * call->med_cnt);

    /* Perform SDP re-negotiation. */
    if (got_media && need_renego_sdp) {
	pjmedia_sdp_neg *neg = call->inv->neg;

	status = pjmedia_sdp_neg_set_remote_offer(tmp_pool, neg, remote_sdp);
	if (status != PJ_SUCCESS)
	    goto on_error;

	status = pjmedia_sdp_neg_set_local_answer(tmp_pool, neg, local_sdp);
	if (status != PJ_SUCCESS)
	    goto on_error;

	status = pjmedia_sdp_neg_negotiate(tmp_pool, neg, 0);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    pj_log_pop_indent();
    return (got_media? PJ_SUCCESS : PJMEDIA_SDPNEG_ENOMEDIA);

on_error:
    pj_log_pop_indent();
    return status;
}

/*****************************************************************************
 * Codecs.
 */

/*
 * Enum all supported codecs in the system.
 */
PJ_DEF(pj_status_t) pjsua_enum_codecs( pjsua_codec_info id[],
				       unsigned *p_count )
{
    pjmedia_codec_mgr *codec_mgr;
    pjmedia_codec_info info[32];
    unsigned i, count, prio[32];
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);
    count = PJ_ARRAY_SIZE(info);
    status = pjmedia_codec_mgr_enum_codecs( codec_mgr, &count, info, prio);
    if (status != PJ_SUCCESS) {
	*p_count = 0;
	return status;
    }

    if (count > *p_count) count = *p_count;

    for (i=0; i<count; ++i) {
	pj_bzero(&id[i], sizeof(pjsua_codec_info));

	pjmedia_codec_info_to_id(&info[i], id[i].buf_, sizeof(id[i].buf_));
	id[i].codec_id = pj_str(id[i].buf_);
	id[i].priority = (pj_uint8_t) prio[i];
    }

    *p_count = count;

    return PJ_SUCCESS;
}


/*
 * Change codec priority.
 */
PJ_DEF(pj_status_t) pjsua_codec_set_priority( const pj_str_t *codec_id,
					      pj_uint8_t priority )
{
    const pj_str_t all = { NULL, 0 };
    pjmedia_codec_mgr *codec_mgr;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);

    if (codec_id->slen==1 && *codec_id->ptr=='*')
	codec_id = &all;

    return pjmedia_codec_mgr_set_codec_priority(codec_mgr, codec_id, 
					        priority);
}


/*
 * Get codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_codec_get_param( const pj_str_t *codec_id,
					   pjmedia_codec_param *param )
{
    const pj_str_t all = { NULL, 0 };
    const pjmedia_codec_info *info;
    pjmedia_codec_mgr *codec_mgr;
    unsigned count = 1;
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);

    if (codec_id->slen==1 && *codec_id->ptr=='*')
	codec_id = &all;

    status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, codec_id,
						 &count, &info, NULL);
    if (status != PJ_SUCCESS)
	return status;

    if (count != 1)
	return (count > 1? PJ_ETOOMANY : PJ_ENOTFOUND);

    status = pjmedia_codec_mgr_get_default_param( codec_mgr, info, param);
    return status;
}


/*
 * Set codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_codec_set_param( const pj_str_t *codec_id,
					   const pjmedia_codec_param *param)
{
    const pjmedia_codec_info *info[2];
    pjmedia_codec_mgr *codec_mgr;
    unsigned count = 2;
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);

    status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, codec_id,
						 &count, info, NULL);
    if (status != PJ_SUCCESS)
	return status;

    /* Codec ID should be specific, except for G.722.1 */
    if (count > 1 && 
	pj_strnicmp2(codec_id, "G7221/16", 8) != 0 &&
	pj_strnicmp2(codec_id, "G7221/32", 8) != 0)
    {
	pj_assert(!"Codec ID is not specific");
	return PJ_ETOOMANY;
    }

    status = pjmedia_codec_mgr_set_default_param(codec_mgr, info[0], param);
    return status;
}


pj_status_t pjsua_media_apply_xml_control(pjsua_call_id call_id,
					  const pj_str_t *xml_st)
{
#if PJMEDIA_HAS_VIDEO
    pjsua_call *call = &pjsua_var.calls[call_id];
    const pj_str_t PICT_FAST_UPDATE = {"picture_fast_update", 19};

    if (pj_strstr(xml_st, &PICT_FAST_UPDATE)) {
	unsigned i;

	PJ_LOG(4,(THIS_FILE, "Received keyframe request via SIP INFO"));

	for (i = 0; i < call->med_cnt; ++i) {
	    pjsua_call_media *cm = &call->media[i];
	    if (cm->type != PJMEDIA_TYPE_VIDEO || !cm->strm.v.stream)
		continue;

	    pjmedia_vid_stream_send_keyframe(cm->strm.v.stream);
	}

	return PJ_SUCCESS;
    }
#endif

    /* Just to avoid compiler warning of unused var */
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(xml_st);

    return PJ_ENOTSUP;
}

