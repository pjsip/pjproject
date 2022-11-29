/* 
 * Copyright (C) 2022 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_UPNP_H__
#define __PJ_UPNP_H__

/**
 * @file upnp.h
 * @brief UPnP client.
 */

#include <pj/sock.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJNATH_UPNP Simple UPnP Client
 * @brief A simple UPnP client implementation.
 * @{
 *
 * This is a simple implementation of UPnP client. Its main function
 * is to request a port mapping from an Internet Gateway Device (IGD),
 * which will redirect communication received on a specified external
 * port to a local socket.
 */

/**
 * This structre describes the parameter to initialize UPnP.
 */
typedef struct pj_upnp_init_param
{
    /**
     * The pool factory where memory will be allocated from.
     */
    pj_pool_factory    *factory;

    /**
     * The interface name to use for all UPnP operations.
     *
     * If NULL, the library will use the first suitable interface found.
     */
    const char         *if_name;

    /**
     * The port number to use for all UPnP operations.
     *
     * If 0, the library will pick an arbitrary free port.
     */
    unsigned            port;
    
    /**
     * The time duration to search for IGD devices (in seconds).
     *
     * If 0, the library will use PJ_UPNP_DEFAULT_SEARCH_TIME.
     */
    int                 search_time;

    /**
     * The callback to notify application when the initialization
     * has completed.
     *
     * @param status    The initialization status.
     */
    void                (*upnp_cb)(pj_status_t status);

} pj_upnp_init_param;



/**
 * Initialize UPnP library and initiate the search for valid Internet
 * Gateway Devices (IGD) in the network.
 *
 * @param param         The UPnP initialization parameter.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error
 *                      status.
 */
PJ_DECL(pj_status_t) pj_upnp_init(const pj_upnp_init_param *param);


/**
 * Deinitialize UPnP library.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error
 *                      status.
 */
PJ_DECL(pj_status_t) pj_upnp_deinit(void);


/**
 * This is the main function to request a port mapping. If successful,
 * the Internet Gateway Device will redirect communication received on
 * the specified external ports to the local sockets.
 *
 * @param sock_cnt      Number of sockets in the socket array.
 * @param sock          Array of local UDP sockets that will be mapped.
 * @param ext_port      (Optional) Array of external port numbers. If NULL,
 *                      the external port numbers requested will be identical
 *                      to the sockets' local port numbers.
 * @param mapped_addr   Array to receive the mapped public addresses and
 *                      ports of the local UDP sockets, when the function
 *                      returns PJ_SUCCESS.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error
 *                      status.
 */
PJ_DECL(pj_status_t)pj_upnp_add_port_mapping(unsigned sock_cnt,
                                             const pj_sock_t sock[],
                                             unsigned ext_port[],
                                             pj_sockaddr mapped_addr[]);


/**
 * Send request to delete a port mapping.
 *
 * @param mapped_addr   The public address and external port mapping to
 *                      be deleted.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error
 *                      status.
 */
PJ_DECL(pj_status_t)pj_upnp_del_port_mapping(const pj_sockaddr *mapped_addr);


PJ_END_DECL

/**
 * @}
 */

#endif  /* __PJ_UPNP_H__ */
