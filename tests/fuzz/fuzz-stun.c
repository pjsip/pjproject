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

#include <pjlib.h>
#include <pj/compat/socket.h>

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>

#define kMinInputLength 10
#define kMaxInputLength 5120

pj_pool_factory *mem;

int stun_parse(char *DataFx,size_t Size){
    int ret = 0;
    pj_pool_t *pool;
    pj_stun_msg *msg;
    pj_status_t status;

    pool = pj_pool_create(mem, "decode_test", 1024, 1024, NULL);

    status = pj_stun_msg_decode(pool, (pj_uint8_t*)DataFx,Size,
                            PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET, 
                        &msg, NULL, NULL);

    if(status)
        ret = 1;

    pj_pool_release(pool);

    return ret;
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{/*pjproject/pjnath/src/pjnath-test/stun.c*/

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

    mem = &caching_pool.factory;

    pj_log_set_level(0);

    pj_init();

    pj_dump_config();
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0 );

    pjlib_util_init();
    pjnath_init();

/*call*/
    ret = stun_parse(DataFx,Size); 

    free(DataFx);
    return ret;
}
