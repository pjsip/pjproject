/* $Header: /pjproject-0.3/pjlib/src/pjlib-test/echo_srv.c 3     10/29/05 10:23p Bennylp $ */
/*
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/echo_srv.c $
 * 
 * 3     10/29/05 10:23p Bennylp
 * Changed ioqueue accept specification.
 * 
 * 2     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 1     10/24/05 11:28a Bennylp
 * Created.
 *
 */
#include "test.h"
#include <pjlib.h>
#include <pj/compat/high_precision.h>

#if INCLUDE_ECHO_SERVER

static pj_bool_t thread_quit_flag;

struct server
{
    pj_pool_t          *pool;
    int                 sock_type;
    int                 thread_count;
    pj_ioqueue_t       *ioqueue;
    pj_sock_t           sock;
    pj_sock_t           client_sock;
    pj_ioqueue_key_t   *key;
    pj_ioqueue_callback cb;
    char               *buf;
    pj_size_t           bufsize;
    pj_sockaddr_in      addr;
    int                 addrlen;
    pj_size_t           bytes_recv;
    pj_timestamp        start_time;
};

static void on_read_complete(pj_ioqueue_key_t *key, pj_ssize_t bytes_read)
{
    struct server *server = pj_ioqueue_get_user_data(key);
    pj_status_t rc;

    if (server->sock_type == PJ_SOCK_DGRAM) {
        if (bytes_read > 0) {
            /* Send data back to sender. */
            rc = pj_ioqueue_sendto( server->ioqueue, server->key, 
                                    server->buf, bytes_read, 0,
                                    &server->addr, server->addrlen);
            if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
                app_perror("...sendto() error", rc);
            }
        } else {
            PJ_LOG(3,("", "...read error (bytes_read=%d)", bytes_read));
        }

        /* Start next receive. */
        rc = pj_ioqueue_recvfrom( server->ioqueue, server->key,
                                  server->buf, server->bufsize, 0,
                                  &server->addr, &server->addrlen);
        if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
            app_perror("...recvfrom() error", rc);
        }

    } 
    else if (server->sock_type == PJ_SOCK_STREAM) {
        if (bytes_read > 0) {
            /* Send data back to sender. */
            rc = pj_ioqueue_send( server->ioqueue, server->key,
                                  server->buf, bytes_read, 0);
            if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
                app_perror("...send() error", rc);
                bytes_read = 0;
            }
        }

        if (bytes_read <= 0) {
            PJ_LOG(3,("", "...tcp closed"));
            pj_ioqueue_unregister( server->ioqueue, server->key );
            pj_sock_close( server->sock );
            pj_pool_release( server->pool );
            return;
        }

        /* Start next receive. */
        rc = pj_ioqueue_recv( server->ioqueue, server->key,
                              server->buf, server->bufsize, 0);
        if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
            app_perror("...recv() error", rc);
        }
    }

    /* Add counter. */
    if (bytes_read > 0) {
        if (server->bytes_recv == 0) {
            pj_get_timestamp(&server->start_time);
            server->bytes_recv += bytes_read;
        } else {
            enum { USECS_IN_SECOND = 1000000 };
            pj_timestamp now;
            pj_uint32_t usec_elapsed;

            server->bytes_recv += bytes_read;

            pj_get_timestamp(&now);
            usec_elapsed = pj_elapsed_usec(&server->start_time, &now);
            if (usec_elapsed > USECS_IN_SECOND) {
                if (usec_elapsed < 2 * USECS_IN_SECOND) {
                    pj_highprec_t bw;
                    pj_uint32_t bw32;
                    const char *type_name;

                    /* bandwidth(bw) = server->bytes_recv * USECS/elapsed */
                    bw = server->bytes_recv;
                    pj_highprec_mul(bw, USECS_IN_SECOND);
                    pj_highprec_div(bw, usec_elapsed);

                    bw32 = (pj_uint32_t) bw;

                    if (server->sock_type==PJ_SOCK_STREAM)
                        type_name = "tcp";
                    else if (server->sock_type==PJ_SOCK_DGRAM)
                        type_name = "udp";
                    else
                        type_name = "???";

                    PJ_LOG(3,("",
                              "...[%s:%d (%d threads)] Current bandwidth=%u KBps",
                              type_name, 
                              ECHO_SERVER_START_PORT+server->thread_count,
                              server->thread_count,
                              bw32/1024));
                }
                server->start_time = now;
                server->bytes_recv = 0;
            }
        }
    }
}

static void on_accept_complete( pj_ioqueue_key_t *key, pj_sock_t sock,
				int status)
{
    struct server *server_server = pj_ioqueue_get_user_data(key);
    pj_status_t rc;

    PJ_UNUSED_ARG(sock);

    if (status == 0) {
        pj_pool_t *pool;
        struct server *new_server;

        pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
        new_server = pj_pool_zalloc(pool, sizeof(struct server));

        new_server->pool = pool;
        new_server->ioqueue = server_server->ioqueue;
        new_server->sock_type = server_server->sock_type;
        new_server->thread_count = server_server->thread_count;
        new_server->sock = server_server->client_sock;
        new_server->bufsize = 4096;
        new_server->buf = pj_pool_alloc(pool, new_server->bufsize);
        new_server->cb = server_server->cb;

        rc = pj_ioqueue_register_sock( new_server->pool, new_server->ioqueue, 
                                       new_server->sock, new_server,
                                       &server_server->cb, &new_server->key);
        if (rc != PJ_SUCCESS) {
            app_perror("...registering new tcp sock", rc);
            pj_sock_close(new_server->sock);
            pj_pool_release(pool);
            thread_quit_flag = 1;
            return;
        }

        rc = pj_ioqueue_recv( new_server->ioqueue, new_server->key, 
                              new_server->buf, new_server->bufsize, 0);
        if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
            app_perror("...recv() error", rc);
            pj_sock_close(new_server->sock);
            pj_pool_release(pool);
            thread_quit_flag = 1;
            return;
        }
    }

    rc = pj_ioqueue_accept( server_server->ioqueue, server_server->key,
                            &server_server->client_sock,
                            NULL, NULL, NULL);
    if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
        app_perror("...accept() error", rc);
        thread_quit_flag = 1;
    }
}

static int thread_proc(void *arg)
{
    pj_ioqueue_t *ioqueue = arg;

    while (!thread_quit_flag) {
        pj_time_val timeout;
        int count;

        timeout.sec = 0; timeout.msec = 50;
        count = pj_ioqueue_poll( ioqueue, &timeout );
        if (count > 0) {
            count = 0;
        }
    }

    return 0;
}

static int start_echo_server( int sock_type, int port, int thread_count )
{
    pj_pool_t *pool;
    struct server *server;
    int i;
    pj_status_t rc;


    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool) 
        return -10;

    server = pj_pool_zalloc(pool, sizeof(struct server));

    server->sock_type = sock_type;
    server->thread_count = thread_count;
    server->cb.on_read_complete = &on_read_complete;
    server->cb.on_accept_complete = &on_accept_complete;

    /* create ioqueue */
    rc = pj_ioqueue_create( pool, 32, thread_count, &server->ioqueue);
    if (rc != PJ_SUCCESS) {
        app_perror("...error creating ioqueue", rc);
        return -20;
    }
    
    /* create and register socket to ioqueue. */
    rc = app_socket(PJ_AF_INET, sock_type, 0, port, &server->sock);
    if (rc != PJ_SUCCESS) {
        app_perror("...error initializing socket", rc);
        return -30;
    }

    rc = pj_ioqueue_register_sock( pool, server->ioqueue, 
                                   server->sock, 
                                   server, &server->cb, 
                                   &server->key);
    if (rc != PJ_SUCCESS) {
        app_perror("...error registering socket to ioqueue", rc);
        return -40;
    }

    /* create receive buffer. */
    server->bufsize = 4096;
    server->buf = pj_pool_alloc(pool, server->bufsize);

    if (sock_type == PJ_SOCK_DGRAM) {
        server->addrlen = sizeof(server->addr);
        rc = pj_ioqueue_recvfrom( server->ioqueue, server->key,
                                  server->buf, server->bufsize,
                                  0,
                                  &server->addr, &server->addrlen);
        if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
            app_perror("...read error", rc);
            return -50;
        }
    } else {
        rc = pj_ioqueue_accept( server->ioqueue, server->key,
                                &server->client_sock, NULL, NULL, NULL );
        if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
            app_perror("...accept() error", rc);
            return -60;
        }
    }

    /* create threads. */
    
    for (i=0; i<thread_count; ++i) {
        pj_thread_t *thread;
        rc = pj_thread_create(pool, NULL, &thread_proc, server->ioqueue,
                              PJ_THREAD_DEFAULT_STACK_SIZE, 0, &thread);
        if (rc != PJ_SUCCESS) {
            app_perror("...unable to create thread", rc);
            return -70;
        }
    }
    
    /* Done. */
    return PJ_SUCCESS;
}

int echo_server(void)
{
    enum { MAX_THREADS = 4 };
    int sock_types[2];
    int i, j, rc;

    sock_types[0] = PJ_SOCK_DGRAM;
    sock_types[1] = PJ_SOCK_STREAM;

    for (i=0; i<2; ++i) {
        for (j=0; j<MAX_THREADS; ++j) {
            rc = start_echo_server(sock_types[i], ECHO_SERVER_START_PORT+j, j+1);
            if (rc != 0)
                return rc;
        }
    }

    pj_thread_sleep(100);
    PJ_LOG(3,("", "Echo server started in port %d - %d", 
              ECHO_SERVER_START_PORT, ECHO_SERVER_START_PORT + MAX_THREADS));

    PJ_LOG(3,("", "Press Ctrl-C to quit"));

    for (;!thread_quit_flag;) {
        pj_thread_sleep(1000);
    }

    return 0;
}


#else
int dummy_echo_server;
#endif  /* INCLUDE_ECHO_SERVER */

