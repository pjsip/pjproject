/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pj/os.h>
#include <pj/pool.h>
#include <pjmedia/sound.h>

pj_status_t session_test (pj_pool_factory *pf);
pj_status_t rtp_test (pj_pool_factory *pf);
pj_status_t sdp_test(pj_pool_factory *pf);
int jbuf_main(pj_pool_factory *pf);

int main()
{
    pj_caching_pool caching_pool;

    pj_init();
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);

    sdp_test (&caching_pool.factory);
    rtp_test(&caching_pool.factory);
    session_test (&caching_pool.factory);
    //jbuf_main(&caching_pool.factory);

    pj_caching_pool_destroy(&caching_pool);
    return 0;
}
