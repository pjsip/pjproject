/* $Header: /pjproject/pjlib/src/pj++/ioqueue.hpp 4     8/24/05 10:29a Bennylp $ */
#ifndef __PJPP_IOQUEUE_H__
#define __PJPP_IOQUEUE_H__

#include <pj++/sock.hpp>
#include <pj++/pool.hpp>
#include <pj++/types.hpp>
#include <pj/ioqueue.h>

class PJ_IOQueue;

class PJ_IOQueue_Event_Handler
{
public:
    virtual ~PJ_IOQueue_Event_Handler()
    {
    }

    pj_ioqueue_key_t* get_key() const
    {
	return key_;
    }

protected:
    //
    // Override this to get notification from I/O Queue
    //
    virtual void on_read_complete(pj_ssize_t bytes_read)
    {
    }

    virtual void on_write_complete(pj_ssize_t bytes_sent)
    {
    }

    virtual void on_accept_complete(int status)
    {
    }

    virtual void on_connect_complete(int status)
    {
    }

protected:
    PJ_IOQueue_Event_Handler()
	: ioqueue_(NULL), key_(NULL)
    {
    }

private:
    PJ_IOQueue *ioqueue_;
    pj_ioqueue_key_t *key_;

    static void read_complete_cb(pj_ioqueue_key_t *key, pj_ssize_t bytes_read)
    {
	PJ_IOQueue_Event_Handler *handler = 
	    (PJ_IOQueue_Event_Handler*)pj_ioqueue_get_user_data(key);
	handler->on_read_complete(bytes_read);
    }

    static void write_complete_cb(pj_ioqueue_key_t *key, pj_ssize_t bytes_sent);
    static void accept_complete_cb(pj_ioqueue_key_t *key, int status);
    static void connect_complete_cb(pj_ioqueue_key_t *key, int status);

    friend class PJ_IOQueue;
};


class PJ_IOQueue
{
    typedef pj_ioqueue_t *B_;

public:
    typedef pj_ioqueue_key_t Key;

    enum Operation
    {
	OP_NONE	     = PJ_IOQUEUE_OP_NONE,
	OP_READ	     = PJ_IOQUEUE_OP_READ,
	OP_RECV_FROM = PJ_IOQUEUE_OP_RECV_FROM,
	OP_WRITE     = PJ_IOQUEUE_OP_WRITE,
	OP_SEND_TO   = PJ_IOQUEUE_OP_SEND_TO,
#if PJ_HAS_TCP
	OP_ACCEPT    = PJ_IOQUEUE_OP_ACCEPT,
	OP_CONNECT   = PJ_IOQUEUE_OP_CONNECT,
#endif
    };

    enum Status
    {
	IS_PENDING = PJ_IOQUEUE_PENDING
    };

    static PJ_IOQueue *create(PJ_Pool *pool, pj_size_t max_fd)
    {
	return (PJ_IOQueue*) pj_ioqueue_create(pool->pool_(), max_fd);
    }

    operator B_()
    {
	return (pj_ioqueue_t*)(PJ_IOQueue*)this;
    }

    pj_ioqueue_t *ioq_()
    {
	return (B_)this;
    }

    void destroy()
    {
	pj_ioqueue_destroy(this->ioq_());
    }

    Key *register_handle(PJ_Pool *pool, pj_oshandle_t hnd, void *user_data)
    {
	return pj_ioqueue_register(pool->pool_(), this->ioq_(), hnd, user_data);
    }

    Key *register_socket(PJ_Pool *pool, pj_sock_t hnd, void *user_data)
    {
	return pj_ioqueue_register(pool->pool_(), this->ioq_(), (pj_oshandle_t)hnd, user_data);
    }

    pj_status_t unregister(Key *key)
    {
	return pj_ioqueue_unregister(this->ioq_(), key);
    }

    void *get_user_data(Key *key)
    {
	return pj_ioqueue_get_user_data(key);
    }

    int poll(Key **key, pj_ssize_t *bytes_status, Operation *op, const PJ_Time_Val *timeout)
    {
	return pj_ioqueue_poll(this->ioq_(), key, bytes_status, (pj_ioqueue_operation_e*)op, timeout);
    }

#if PJ_HAS_TCP
    pj_status_t connect(Key *key, const pj_sockaddr_t *addr, int addrlen)
    {
	return pj_ioqueue_connect(this->ioq_(), key, addr, addrlen);
    }

    pj_status_t accept(Key *key, PJ_Socket *sock, pj_sockaddr_t *local, pj_sockaddr_t *remote, int *addrlen)
    {
	return pj_ioqueue_accept(this->ioq_(), key, &sock->get_handle(), local, remote, addrlen);
    }
#endif

    int read(Key *key, void *buf, pj_size_t len)
    {
	return pj_ioqueue_read(this->ioq_(), key, buf, len);
    }

    int recvfrom(Key *key, void *buf, pj_size_t len, pj_sockaddr_t *addr, int *addrlen)
    {
	return pj_ioqueue_recvfrom(this->ioq_(), key, buf, len, addr, addrlen);
    }

    int write(Key *key, const void *data, pj_size_t len)
    {
	return pj_ioqueue_write(this->ioq_(), key, data, len);
    }

    int sendto(Key *key, const void *data, pj_size_t len, const pj_sockaddr_t *addr, int addrlen)
    {
	return pj_ioqueue_sendto(this->ioq_(), key, data, len, addr, addrlen);
    }
};

#endif	/* __PJPP_IOQUEUE_H__ */
