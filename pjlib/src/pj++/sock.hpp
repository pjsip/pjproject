/* $Header: /pjproject/pjlib/src/pj++/sock.hpp 2     2/24/05 11:23a Bennylp $ */
#ifndef __PJPP_SOCK_H__
#define __PJPP_SOCK_H__

#include <pj/sock.h>

class PJ_Addr
{
};

class PJ_INET_Addr : public pj_sockaddr_in, public PJ_Addr
{
public:
    pj_uint16_t get_port_number() const
    {
	return pj_sockaddr_get_port(this);
    }

    void set_port_number(pj_uint16_t port)
    {
	sin_family = PJ_AF_INET;
	pj_sockaddr_set_port(this, port);
    }

    pj_uint32_t get_ip_address() const
    {
	return pj_sockaddr_get_addr(this);
    }

    const char *get_address() const
    {
	return pj_sockaddr_get_str_addr(this);
    }

    void set_ip_address(pj_uint32_t addr)
    {
	sin_family = PJ_AF_INET;
	pj_sockaddr_set_addr(this, addr);
    }

    pj_status_t set_address(const pj_str_t *addr)
    {
	return pj_sockaddr_set_str_addr(this, addr);
    }

    pj_status_t set_address(const char *addr)
    {
	return pj_sockaddr_set_str_addr2(this, addr);
    }

    int cmp(const PJ_INET_Addr &rhs) const
    {
	return pj_sockaddr_cmp(this, &rhs);
    }

    bool operator==(const PJ_INET_Addr &rhs) const
    {
	return cmp(rhs) == 0;
    }
};

class PJ_Socket
{
public:
    PJ_Socket() {}
    PJ_Socket(const PJ_Socket &rhs) : sock_(rhs.sock_) {}

    void set_handle(pj_sock_t sock)
    {
	sock_ = sock;
    }

    pj_sock_t get_handle() const
    {
	return sock_;
    }

    pj_sock_t& get_handle()
    {
	return sock_;
    }

    bool socket(int af, int type, int proto, pj_uint32_t flag=0)
    {
	sock_ = pj_sock_socket(af, type, proto, flag);
	return sock_ != -1;
    }

    bool bind(const PJ_INET_Addr &addr)
    {
	return pj_sock_bind(sock_, &addr, sizeof(PJ_INET_Addr)) == 0;
    }

    bool close()
    {
	return pj_sock_close(sock_) == 0;
    }

    bool getpeername(PJ_INET_Addr *addr)
    {
	int namelen;
	return pj_sock_getpeername(sock_, addr, &namelen) == 0;
    }

    bool getsockname(PJ_INET_Addr *addr)
    {
	int namelen;
	return pj_sock_getsockname(sock_, addr, &namelen) == 0;
    }

    bool getsockopt(int level, int optname, void *optval, int *optlen)
    {
	return pj_sock_getsockopt(sock_, level, optname, optval, optlen) == 0;
    }

    bool setsockopt(int level, int optname, const void *optval, int optlen)
    {
	return pj_sock_setsockopt(sock_, level, optname, optval, optlen) == 0;
    }

    bool ioctl(long cmd, pj_uint32_t *val)
    {
	return pj_sock_ioctl(sock_, cmd, val) == 0;
    }

    int recv(void *buf, int len, int flag = 0)
    {
	return pj_sock_recv(sock_, buf, len, flag);
    }

    int send(const void *buf, int len, int flag = 0)
    {
	return pj_sock_send(sock_, buf, len, flag);
    }

protected:
    pj_sock_t sock_;
};

#if PJ_HAS_TCP
class PJ_Sock_Stream : public PJ_Socket
{
public:
    PJ_Sock_Stream() {}
    PJ_Sock_Stream(const PJ_Sock_Stream &rhs) : PJ_Socket(rhs) {}
    PJ_Sock_Stream &operator=(const PJ_Sock_Stream &rhs) { sock_ = rhs.sock_; return *this; }

    bool listen(int backlog = 5)
    {
	return pj_sock_listen(sock_, backlog) == 0;
    }

    bool accept(PJ_Sock_Stream *new_sock, PJ_INET_Addr *addr, int *addrlen)
    {
	pj_sock_t s = pj_sock_accept(sock_, addr, addrlen);
	if (s == -1)
	    return false;
	new_sock->set_handle(s);
	return true;
    }

    bool connect(const PJ_INET_Addr &addr)
    {
	return pj_sock_connect(sock_, &addr, sizeof(PJ_INET_Addr)) == 0;
    }

    bool shutdown(int how)
    {
	return pj_sock_shutdown(sock_, how) == 0;
    }

};
#endif

class PJ_Sock_Dgram : public PJ_Socket
{
public:
    PJ_Sock_Dgram() {}
    PJ_Sock_Dgram(const PJ_Sock_Dgram &rhs) : PJ_Socket(rhs) {}
    PJ_Sock_Dgram &operator=(const PJ_Sock_Dgram &rhs) { sock_ = rhs.sock_; return *this; }

    int recvfrom(void *buf, int len, int flag, PJ_INET_Addr *fromaddr)
    {
	int addrlen;
	return pj_sock_recvfrom(sock_, buf, len, flag, fromaddr, &addrlen);
    }

    int sendto(const void *buf, int len, int flag, const PJ_INET_Addr &addr)
    {
	return pj_sock_sendto(sock_, buf, len, flag, &addr, sizeof(PJ_INET_Addr));
    }
};

#endif	/* __PJPP_SOCK_H__ */
