/* $Id$
 *
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
