/* 
 * Copyright (C) 2023 Teluu Inc. (http://www.teluu.com)
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>

#include <pjmedia/event.h>
#include <pjmedia/rtcp.h>

#define kMinInputLength 10
#define kMaxInputLength 5120

int rtcp_parser(char *data, size_t size)
{

    int ret = 0;
    pjmedia_rtcp_session_setting setting;
    pjmedia_rtcp_session session;

    pjmedia_rtcp_session_setting_default(&setting);
    setting.clock_rate = 8000;
    setting.samples_per_frame = 160;
    pjmedia_rtcp_init2(&session, &setting);

    pjmedia_rtcp_rx_rtcp(&session, data, size);

    return ret;
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    char *data;
    int ret = 0;
    pj_caching_pool caching_pool;
    pj_pool_t *pool;

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    /* Add null termination for the data */
    data = (char *)calloc((Size+1), sizeof(char));
    memcpy((void *)data, (void *)Data, Size);

    /* Init */
    pj_init();
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pool = pj_pool_create(&caching_pool.factory, "test", 1000, 1000, NULL);
    pj_log_set_level(0);

    pjmedia_event_mgr_create(pool, 0, NULL);

    /* Fuzz */
    ret = rtcp_parser(data, Size);

    free(data);
    pjmedia_event_mgr_destroy(pjmedia_event_mgr_instance());
    pj_pool_release(pool);
    pj_caching_pool_destroy(&caching_pool);

    return ret;
}
