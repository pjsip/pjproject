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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjmedia.h>
#include <pjlib.h>

#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>

#define kMinInputLength 10
#define kMaxInputLength 5120

pj_pool_factory *mem;

int sdp_parser(char *DataFx,size_t Size){

    int ret = 0;
	pj_pool_t *pool;
    pjmedia_sdp_session *sdp;
    pj_status_t status;

    pool = pj_pool_create(mem, "sdp_neg_test", 4000, 4000, NULL);

    status = pjmedia_sdp_parse(pool, DataFx, Size,&sdp);

    if (status != PJ_SUCCESS){
        ret = 1;
        goto end;
    }

    status = pjmedia_sdp_validate(sdp);

end:
    pj_pool_release(pool);

    return ret;
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{/*pjproject/pjmedia/src/test/sdp_neg_test.c*/

    if (Size < kMinInputLength || Size > kMaxInputLength){
        return 1;
    }

/*Add Extra byte */
    char *DataFx;
    DataFx = (char *)calloc((Size+1),sizeof(char));
    memcpy((void *)DataFx,(void *)Data,Size);

/*init*/
    int ret = 0;
    pj_caching_pool caching_pool;
    pj_pool_t *pool;

    pj_init();
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pool = pj_pool_create(&caching_pool.factory, "test", 1000, 512, NULL);

    pj_log_set_level(0);

    mem = &caching_pool.factory;

    pjmedia_event_mgr_create(pool, 0, NULL);

/*Call*/
    ret = sdp_parser(DataFx,Size);

/*Free*/
    pjmedia_event_mgr_destroy(pjmedia_event_mgr_instance());
    pj_pool_release(pool);
    pj_caching_pool_destroy(&caching_pool);

    free(DataFx);
    return ret;
}
