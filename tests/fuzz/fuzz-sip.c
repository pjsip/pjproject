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
#include <pjlib-util.h>
#include <pjsip.h>
#include <pjsip/sip_types.h>

#include <pjsip.h>
#include <pjlib.h>


pjsip_endpoint *endpt;
pj_caching_pool caching_pool;

#define POOL_SIZE	8000
#define PJSIP_TEST_MEM_SIZE	    (2*1024*1024)

#define kMinInputLength 10
#define kMaxInputLength 5120

int sipParser(char *DataFx,size_t Size){

    int ret = 0;
    pj_pool_t *pool;
    pjsip_msg *parsed_msg = NULL;
    pjsip_parser_err_report err_list;

    pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);


    pj_list_init(&err_list);

    parsed_msg = pjsip_parse_msg(pool, DataFx, Size, &err_list);
    
    if (parsed_msg == NULL)
        ret = 1;

    pjsip_endpt_release_pool(endpt, pool);

    return ret;
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{/*/home/Ez/project/pjproject/pjsip/src/test/msg_test.c*/

    if (Size < kMinInputLength || Size > kMaxInputLength){
        return 1;
    }

/*Add Extra byte */
    char *DataFx;
    DataFx = (char *)calloc((Size+1),sizeof(char));
    memcpy((void *)DataFx,(void *)Data,Size);


/*init*/
    pj_status_t rc;
    //pj_status_t status;

    pj_log_set_level(0);

    rc=pj_init();
    rc=pjlib_util_init();

    pj_dump_config();

    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 
			  PJSIP_TEST_MEM_SIZE );

    rc = pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt);

    /* Start transaction layer module. */
    rc = pjsip_tsx_layer_init_module(endpt);

    rc = pjsip_loop_start(endpt, NULL);

/*Calls*/
    rc = sipParser(DataFx,Size);

    pjsip_endpt_destroy(endpt);
    pj_caching_pool_destroy(&caching_pool);

    free(DataFx);

    return rc;
}
