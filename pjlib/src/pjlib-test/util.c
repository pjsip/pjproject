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
#include "test.h"
#include <pjlib.h>

#define THIS_FILE "util.c"

void app_perror(const char *msg, pj_status_t rc)
{
    char errbuf[PJ_ERR_MSG_SIZE];

    PJ_CHECK_STACK();

    pj_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(3,("test", "%s: [pj_status_t=%d] %s", msg, rc, errbuf));
}

#define SERVER 0
#define CLIENT 1

pj_status_t app_socket(int family, int type, int proto, int port,
                       pj_sock_t *ptr_sock)
{
    pj_sockaddr_in addr;
    pj_sock_t sock;
    pj_status_t rc;

    rc = pj_sock_socket(family, type, proto, &sock);
    if (rc != PJ_SUCCESS)
        return rc;

    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = (pj_uint16_t)family;
    addr.sin_port = (short)(port!=-1 ? pj_htons((pj_uint16_t)port) : 0);
    rc = pj_sock_bind(sock, &addr, sizeof(addr));
    if (rc != PJ_SUCCESS) {
        pj_sock_close(sock);
        return rc;
    }
    
#if PJ_HAS_TCP
    if (type == pj_SOCK_STREAM()) {
        rc = pj_sock_listen(sock, 5);
        if (rc != PJ_SUCCESS)
            return rc;
    }
#endif

    *ptr_sock = sock;
    return PJ_SUCCESS;
}

pj_status_t app_socketpair(int family, int type, int protocol,
                           pj_sock_t *serverfd, pj_sock_t *clientfd)
{
    pj_status_t status;
    pj_sock_t sv[2];

    status = pj_sock_socketpair(family, type, protocol, sv);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "socketpair error"));
        return status;
    }

    *serverfd = sv[0];
    *clientfd = sv[1];
    return PJ_SUCCESS;
}
