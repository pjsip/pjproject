/* $Id$ */
/* 
 * Copyright (C) 2008-2010 Teluu Inc. (http://www.teluu.com)
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

/**
 * \page page_httpdemo_c Samples: HTTP Client demo
 *
 * This file is pjsip-apps/src/samples/httpdemo.c
 *
 * \includelineno httpdemo.c
 */

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjlib-util/http_client.h>
#include <pjsip.h>
#include <pjmedia.h>
#include <pjnath.h>
#include <pjsip_simple.h>

static pj_timer_heap_t *timer_heap;
static pj_ioqueue_t *ioqueue;
static pj_pool_t *pool;
static pj_http_req *http_req;
static pj_pool_factory *mem;
static FILE *f = NULL;

//#define VERBOSE
#define THIS_FILE	    "http_demo"

static void on_data_read(pj_http_req *hreq, void *data, pj_size_t size)
{
    PJ_UNUSED_ARG(hreq);

    if (size > 0) {
        fwrite(data, 1, size, f);
        fflush(f);
#ifdef VERBOSE
        PJ_LOG(3, (THIS_FILE, "\nData received: %d bytes\n", size));
        printf("%.*s\n", (int)size, (char *)data);
#endif
    }
}

static void on_complete(pj_http_req *hreq, pj_status_t status,
                        const pj_http_resp *resp)
{
    PJ_UNUSED_ARG(hreq);

    if (status == PJ_ECANCELLED) {
        PJ_LOG(3, (THIS_FILE, "Request cancelled\n"));
        return;
    } else if (status == PJ_ETIMEDOUT) {
        PJ_LOG(3, (THIS_FILE, "Request timed out!\n"));
        return;
    } else if (status != PJ_SUCCESS && status != PJ_EPENDING) {
        PJ_LOG(3, (THIS_FILE, "Error %d\n", status));
        return;
    }
    PJ_LOG(3, (THIS_FILE, "\nData completed: %d bytes\n", resp->size));
    if (resp->size > 0 && resp->data) {
#ifdef VERBOSE
        printf("%.*s\n", (int)resp->size, (char *)resp->data);
#endif
    }
}

pj_status_t getURL(const char *curl)
{
    pj_str_t url;
    pj_http_req_callback hcb;
    pj_status_t status;

    pj_bzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_data_read = &on_data_read;

    /* Create pool, timer, and ioqueue */
    pool = pj_pool_create(mem, NULL, 8192, 4096, NULL);
    if (pj_timer_heap_create(pool, 16, &timer_heap))
        return -31;
    if (pj_ioqueue_create(pool, 16, &ioqueue))
        return -32;

    pj_strdup2(pool, &url, curl);

    if ((status = pj_http_req_create(pool, &url, timer_heap, ioqueue, 
                           NULL, &hcb, &http_req)) != PJ_SUCCESS)
        return status;

    if ((status = pj_http_req_start(http_req)) != PJ_SUCCESS)
        return status;

    while (pj_http_req_is_running(http_req)) {
        pj_time_val delay = {0, 50};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer_heap, NULL);
    }

    pj_http_req_destroy(http_req);
    pj_ioqueue_destroy(ioqueue);
    pj_timer_heap_destroy(timer_heap);
    pj_pool_release(pool);

    return PJ_SUCCESS;
}
/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pj_status_t status;

    if (argc != 3) {
	puts("Usage: httpdemo URL filename");
	return 1;
    }

    pj_log_set_level(3);

    pj_init();
    pj_caching_pool_init(&cp, NULL, 0);
    mem = &cp.factory;
    pjlib_util_init();

    f = fopen(argv[2], "wb");
    status = getURL(argv[1]);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error"));
    }
    fclose(f);

    pj_shutdown();
    return 0;
}
