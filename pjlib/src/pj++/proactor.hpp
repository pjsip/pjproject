/* $Header: /pjproject/pjlib/src/pj++/proactor.hpp 3     8/24/05 10:29a Bennylp $ */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __PJPP_EVENT_HANDLER_H__
#define __PJPP_EVENT_HANDLER_H__

#include <pj/ioqueue.h>
#include <pj++/pool.hpp>
#include <pj++/sock.hpp>
#include <pj++/timer.hpp>

class PJ_Proactor;


class PJ_Event_Handler
{
    friend class PJ_Proactor;
public:
    PJ_Event_Handler();
    virtual ~PJ_Event_Handler();

    virtual pj_oshandle_t get_handle() = 0;

    bool read(void *buf, pj_size_t len);
    bool recvfrom(void *buf, pj_size_t len, PJ_INET_Addr *addr);
    bool write(const void *data, pj_size_t len);
    bool sendto(const void *data, pj_size_t len, const PJ_INET_Addr &addr);
#if PJ_HAS_TCP
    bool connect(const PJ_INET_Addr &addr);
    bool accept(PJ_Socket *sock, PJ_INET_Addr *local=NULL, PJ_INET_Addr *remote=NULL);
#endif

protected:
    //
    // Overridables
    //
    virtual void on_timeout(int data) {}
    virtual void on_read_complete(pj_ssize_t bytes_read) {}
    virtual void on_write_complete(pj_ssize_t bytes_sent) {}
#if PJ_HAS_TCP
    virtual void on_connect_complete(int status) {}
    virtual void on_accept_complete(int status) {}
#endif

private:
    PJ_Proactor	     *proactor_;
    pj_ioqueue_key_t *key_;
    pj_timer_entry    timer_;
    int		      tmp_recvfrom_addr_len;

public:
    // Internal IO Queue/timer callback.
    static void timer_callback( pj_timer_heap_t *timer_heap, struct pj_timer_entry *entry);
    static void read_complete_cb(pj_ioqueue_key_t *key, pj_ssize_t bytes_read);
    static void write_complete_cb(pj_ioqueue_key_t *key, pj_ssize_t bytes_sent);
    static void accept_complete_cb(pj_ioqueue_key_t *key, int status);
    static void connect_complete_cb(pj_ioqueue_key_t *key, int status);
};

class PJ_Proactor
{
public:
    static PJ_Proactor *create(PJ_Pool *pool, pj_size_t max_fd, 
			       pj_size_t timer_entry_count, unsigned timer_flags=0);

    void destroy();

    bool register_handler(PJ_Pool *pool, PJ_Event_Handler *handler);
    void unregister_handler(PJ_Event_Handler *handler);

    static bool schedule_timer( pj_timer_heap_t *timer, PJ_Event_Handler *handler,
				const PJ_Time_Val &delay, int id=-1);
    bool schedule_timer(PJ_Event_Handler *handler, const PJ_Time_Val &delay, int id=-1);
    bool cancel_timer(PJ_Event_Handler *handler);

    bool handle_events(PJ_Time_Val *timeout);

    pj_ioqueue_t *get_io_queue();
    pj_timer_heap_t *get_timer_heap();

private:
    pj_ioqueue_t *ioq_;
    pj_timer_heap_t *th_;

    PJ_Proactor() {}
};

#endif	/* __PJPP_EVENT_HANDLER_H__ */
