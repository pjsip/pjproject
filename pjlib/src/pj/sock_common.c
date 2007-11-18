/* $Id$ */
/* 
 * Copyright (C)2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pj/sock.h>

PJ_DEF(pj_uint16_t) pj_AF_UNSPEC(void)
{
    return PJ_AF_UNSPEC;
}

PJ_DEF(pj_uint16_t) pj_AF_UNIX(void)
{
    return PJ_AF_UNIX;
}

PJ_DEF(pj_uint16_t) pj_AF_INET(void)
{
    return PJ_AF_INET;
}

PJ_DEF(pj_uint16_t) pj_AF_INET6(void)
{
    return PJ_AF_INET6;
}

PJ_DEF(pj_uint16_t) pj_AF_PACKET(void)
{
    return PJ_AF_PACKET;
}

PJ_DEF(pj_uint16_t) pj_AF_IRDA(void)
{
    return PJ_AF_IRDA;
}

PJ_DEF(int) pj_SOCK_STREAM(void)
{
    return PJ_SOCK_STREAM;
}

PJ_DEF(int) pj_SOCK_DGRAM(void)
{
    return PJ_SOCK_DGRAM;
}

PJ_DEF(int) pj_SOCK_RAW(void)
{
    return PJ_SOCK_RAW;
}

PJ_DEF(int) pj_SOCK_RDM(void)
{
    return PJ_SOCK_RDM;
}

PJ_DEF(pj_uint16_t) pj_SOL_SOCKET(void)
{
    return PJ_SOL_SOCKET;
}

PJ_DEF(pj_uint16_t) pj_SOL_IP(void)
{
    return PJ_SOL_IP;
}

PJ_DEF(pj_uint16_t) pj_SOL_TCP(void)
{
    return PJ_SOL_TCP;
}

PJ_DEF(pj_uint16_t) pj_SOL_UDP(void)
{
    return PJ_SOL_UDP;
}

PJ_DEF(pj_uint16_t) pj_SOL_IPV6(void)
{
    return PJ_SOL_IPV6;
}

PJ_DEF(int) pj_IP_TOS(void)
{
    return PJ_IP_TOS;
}

PJ_DEF(int) pj_IPTOS_LOWDELAY(void)
{
    return PJ_IPTOS_LOWDELAY;
}

PJ_DEF(int) pj_IPTOS_THROUGHPUT(void)
{
    return PJ_IPTOS_THROUGHPUT;
}

PJ_DEF(int) pj_IPTOS_RELIABILITY(void)
{
    return PJ_IPTOS_RELIABILITY;
}

PJ_DEF(int) pj_IPTOS_MINCOST(void)
{
    return PJ_IPTOS_MINCOST;
}

PJ_DEF(pj_uint16_t) pj_SO_TYPE(void)
{
    return PJ_SO_TYPE;
}

PJ_DEF(pj_uint16_t) pj_SO_RCVBUF(void)
{
    return PJ_SO_RCVBUF;
}

PJ_DEF(pj_uint16_t) pj_SO_SNDBUF(void)
{
    return PJ_SO_SNDBUF;
}

PJ_DEF(int) pj_MSG_OOB(void)
{
    return PJ_MSG_OOB;
}

PJ_DEF(int) pj_MSG_PEEK(void)
{
    return PJ_MSG_PEEK;
}

PJ_DEF(int) pj_MSG_DONTROUTE(void)
{
    return PJ_MSG_DONTROUTE;
}

