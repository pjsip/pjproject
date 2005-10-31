/* $Header: /pjproject-0.3/pjlib/src/pj++/proactor.cpp 7     10/29/05 11:51a Bennylp $ */
#include <pj++/proactor.hpp>
#include <pj/string.h>	// memset

static struct pj_ioqueue_callback ioqueue_cb =
{
    &PJ_Event_Handler::read_complete_cb,
    &PJ_Event_Handler::write_complete_cb,
    &PJ_Event_Handler::accept_complete_cb,
    &PJ_Event_Handler::connect_complete_cb,
};

PJ_Event_Handler::PJ_Event_Handler()
: proactor_(NULL), key_(NULL)
{
    pj_memset(&timer_, 0, sizeof(timer_));
    timer_.user_data = this;
    timer_.cb = &timer_callback;
}

PJ_Event_Handler::~PJ_Event_Handler()
{
}

#if PJ_HAS_TCP
bool PJ_Event_Handler::connect(const PJ_INET_Addr &addr)
{
    pj_assert(key_ != NULL && proactor_ != NULL);

    if (key_ == NULL || proactor_ == NULL)
	return false;

    int status = pj_ioqueue_connect(proactor_->get_io_queue(), key_, 
				    &addr, sizeof(PJ_INET_Addr));
    if (status == 0) {
	on_connect_complete(0);
	return true;
    } else if (status == PJ_IOQUEUE_PENDING) {
	return true;
    } else {
	return false;
    }
}

bool PJ_Event_Handler::accept(PJ_Socket *sock, PJ_INET_Addr *local, PJ_INET_Addr *remote)
{
    pj_assert(key_ != NULL && proactor_ != NULL);

    if (key_ == NULL || proactor_ == NULL)
	return false;

    int status = pj_ioqueue_accept(proactor_->get_io_queue(), key_, 
				   &sock->get_handle(), 
				   local_addr, remote, 
				   (remote? sizeof(*remote) : 0));
    if (status == 0) {
	on_accept_complete(0);
	return true;
    } else if (status == PJ_IOQUEUE_PENDING) {
	return true;
    } else {
	return false;
    }
}

#endif

bool PJ_Event_Handler::read(void *buf, pj_size_t len)
{
    pj_assert(key_ != NULL && proactor_ != NULL);

    if (key_ == NULL || proactor_ == NULL)
	return false;

    int bytes_status = pj_ioqueue_read(proactor_->get_io_queue(), 
				       key_, buf, len);
    if (bytes_status >= 0) {
	on_read_complete(bytes_status);
	return true;
    } else if (bytes_status == PJ_IOQUEUE_PENDING) {
	return true;
    } else {
	return false;
    }
}

bool PJ_Event_Handler::recvfrom(void *buf, pj_size_t len, PJ_INET_Addr *addr)
{
    pj_assert(key_ != NULL && proactor_ != NULL);

    if (key_ == NULL || proactor_ == NULL)
	return false;


    tmp_recvfrom_addr_len = sizeof(PJ_INET_Addr);

    int bytes_status = pj_ioqueue_recvfrom(proactor_->get_io_queue(), 
					   key_, buf, len,
					   addr,
					   (addr? &tmp_recvfrom_addr_len : NULL));
    if (bytes_status >= 0) {
	on_read_complete(bytes_status);
	return true;
    } else if (bytes_status == PJ_IOQUEUE_PENDING) {
	return true;
    } else {
	return false;
    }
}

bool PJ_Event_Handler::write(const void *data, pj_size_t len)
{
    pj_assert(key_ != NULL && proactor_ != NULL);

    if (key_ == NULL || proactor_ == NULL)
	return false;

    int bytes_status = pj_ioqueue_write(proactor_->get_io_queue(), 
					key_, data, len);
    if (bytes_status >= 0) {
	on_write_complete(bytes_status);
	return true;
    } else if (bytes_status == PJ_IOQUEUE_PENDING) {
	return true;
    } else {
	return false;
    }
}

bool PJ_Event_Handler::sendto(const void *data, pj_size_t len, const PJ_INET_Addr &addr)
{
    pj_assert(key_ != NULL && proactor_ != NULL);

    if (key_ == NULL || proactor_ == NULL)
	return false;

    int bytes_status = pj_ioqueue_sendto(proactor_->get_io_queue(), 
					 key_, data, len, 
					 &addr, sizeof(PJ_INET_Addr));
    if (bytes_status >= 0) {
	on_write_complete(bytes_status);
	return true;
    } else if (bytes_status == PJ_IOQUEUE_PENDING) {
	return true;
    } else {
	return false;
    }
}


void PJ_Event_Handler::read_complete_cb(pj_ioqueue_key_t *key, pj_ssize_t bytes_read)
{
    PJ_Event_Handler *handler = 
	(PJ_Event_Handler*) pj_ioqueue_get_user_data(key);

    handler->on_read_complete(bytes_read);
}

void PJ_Event_Handler::write_complete_cb(pj_ioqueue_key_t *key, pj_ssize_t bytes_sent)
{
    PJ_Event_Handler *handler = 
	(PJ_Event_Handler*) pj_ioqueue_get_user_data(key);

    handler->on_write_complete(bytes_sent);
}

void PJ_Event_Handler::accept_complete_cb(pj_ioqueue_key_t *key, int status)
{
#if PJ_HAS_TCP
    PJ_Event_Handler *handler = 
	(PJ_Event_Handler*) pj_ioqueue_get_user_data(key);

    handler->on_accept_complete(status);
#endif
}

void PJ_Event_Handler::connect_complete_cb(pj_ioqueue_key_t *key, int status)
{
#if PJ_HAS_TCP
    PJ_Event_Handler *handler = 
	(PJ_Event_Handler*) pj_ioqueue_get_user_data(key);

    handler->on_connect_complete(status);
#endif
}

void PJ_Event_Handler::timer_callback( pj_timer_heap_t *timer_heap,
				       struct pj_timer_entry *entry)
{
    PJ_Event_Handler *handler = (PJ_Event_Handler*) entry->user_data;
    handler->on_timeout(entry->id);
}


PJ_Proactor *PJ_Proactor::create(PJ_Pool *pool, pj_size_t max_fd, 
				 pj_size_t timer_entry_count, unsigned timer_flags)
{
    PJ_Proactor *p = (PJ_Proactor*) pool->calloc(1, sizeof(PJ_Proactor));
    if (!p) return NULL;

    p->ioq_ = pj_ioqueue_create(pool->pool_(), max_fd);
    if (!p->ioq_) return NULL;

    p->th_ = pj_timer_heap_create(pool->pool_(), timer_entry_count, timer_flags);
    if (!p->th_) return NULL;

    return p;
}

void PJ_Proactor::destroy()
{
    pj_ioqueue_destroy(ioq_);
}

bool PJ_Proactor::register_handler(PJ_Pool *pool, PJ_Event_Handler *handler)
{
    pj_assert(handler->key_ == NULL && handler->proactor_ == NULL);

    if (handler->key_ != NULL) 
	return false;

    handler->key_ = pj_ioqueue_register_sock(pool->pool_(), ioq_, 
                                             handler->get_handle(), 
					     handler, &ioqueue_cb);
    if (handler->key_ != NULL) {
	handler->proactor_ = this;
	return true;
    } else {
	return false;
    }
}

void PJ_Proactor::unregister_handler(PJ_Event_Handler *handler)
{
    if (handler->key_ == NULL) return;
    pj_ioqueue_unregister(ioq_, handler->key_);
    handler->key_ = NULL;
    handler->proactor_ = NULL;
}

bool PJ_Proactor::schedule_timer( pj_timer_heap_t *timer, PJ_Event_Handler *handler,
				  const PJ_Time_Val &delay, int id)
{
    handler->timer_.id = id;
    return pj_timer_heap_schedule(timer, &handler->timer_, &delay) == 0;
}

bool PJ_Proactor::schedule_timer(PJ_Event_Handler *handler, const PJ_Time_Val &delay, 
				 int id)
{
    return schedule_timer(th_, handler, delay, id);
}

bool PJ_Proactor::cancel_timer(PJ_Event_Handler *handler)
{
    return pj_timer_heap_cancel(th_, &handler->timer_) == 1;
}

bool PJ_Proactor::handle_events(PJ_Time_Val *max_timeout)
{
    pj_time_val timeout;

    timeout.sec = timeout.msec = 0; /* timeout is 'out' var. */

    if (pj_timer_heap_poll( th_, &timeout ) > 0)
	return true;

    if (timeout.sec < 0) timeout.sec = PJ_MAXINT32;

    /* If caller specifies maximum time to wait, then compare the value with
     * the timeout to wait from timer, and use the minimum value.
     */
    if (max_timeout && PJ_TIME_VAL_GT(timeout, *max_timeout)) {
	timeout = *max_timeout;
    }

    /* Poll events in ioqueue. */
    int result;

    result = pj_ioqueue_poll(ioq_, &timeout);
    if (result != 1)
	return false;

    return true;
}

pj_ioqueue_t *PJ_Proactor::get_io_queue()
{
    return ioq_;
}

pj_timer_heap_t *PJ_Proactor::get_timer_heap()
{
    return th_;
}

