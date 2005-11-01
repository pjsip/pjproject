/* $Header: /pjproject-0.3/pjlib/src/pjlib-test/pool_perf.c 2     10/14/05 12:26a Bennylp $ */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/pool_perf.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     10/05/05 5:13p Bennylp
 * Created.
 *
 */
#include "test.h"

#if INCLUDE_POOL_PERF_TEST

#include <pjlib.h>
#include <pj/compat/malloc.h>

#if !PJ_HAS_HIGH_RES_TIMER
# error Need high resolution timer for this test.
#endif

#define THIS_FILE   "test"

#define LOOP	    10
#define COUNT	    1024
static unsigned	    sizes[COUNT];
#define MIN_SIZE    4
#define MAX_SIZE    512
static unsigned total_size;

static int pool_test_pool()
{
    int i;
    pj_pool_t *pool = pj_pool_create(mem, NULL, total_size + 4*COUNT, 0, NULL);
    if (!pool)
	return -1;

    for (i=0; i<COUNT; ++i) {
	char *p;
	if ( (p=(char*)pj_pool_alloc(pool, sizes[i])) == NULL)
	    return -1;
	*p = '\0';
    }

    pj_pool_release(pool);
    return 0;
}

static int pool_test_malloc_free()
{
    char *p[COUNT];
    int i;

    for (i=0; i<COUNT; ++i) {
	p[i] = (char*)malloc(sizes[i]);
	if (!p[i]) {
	    // Don't care for memory leak in this test
	    return -1;
	}
	*p[i] = '\0';
    }

    for (i=0; i<COUNT; ++i) {
	free(p[i]);
    }

    return 0;
}

int pool_perf_test()
{
    unsigned i;
    pj_uint32_t pool_time=0, malloc_time=0, pool_time2=0;
    pj_timestamp start, end;
    pj_uint32_t best, worst;

    // Initialize sizes.
    for (i=0; i<COUNT; ++i) {
	sizes[i] = MIN_SIZE + pj_rand() % MAX_SIZE;
	total_size += sizes[i];
    }

    PJ_LOG(3, (THIS_FILE, "Benchmarking pool.."));

    // Warmup
    pool_test_pool();
    pool_test_malloc_free();

    for (i=0; i<LOOP; ++i) {
	pj_get_timestamp(&start);
	if (pool_test_pool()) {
	    return 1;
	}
	pj_get_timestamp(&end);
	pool_time += (end.u32.lo - start.u32.lo);

	pj_get_timestamp(&start);
	if (pool_test_malloc_free()) {
	    return 2;
	}
	pj_get_timestamp(&end);
	malloc_time += (end.u32.lo - start.u32.lo);

	pj_get_timestamp(&start);
	if (pool_test_pool()) {
	    return 4;
	}
	pj_get_timestamp(&end);
	pool_time2 += (end.u32.lo - start.u32.lo);
    }

    PJ_LOG(4, (THIS_FILE, "..LOOP count:                        %u", LOOP));
    PJ_LOG(4, (THIS_FILE, "..number of alloc/dealloc per loop:  %u", COUNT));
    PJ_LOG(4, (THIS_FILE, "..pool allocation/deallocation time: %u", pool_time));
    PJ_LOG(4, (THIS_FILE, "..malloc/free time:                  %u", malloc_time));
    PJ_LOG(4, (THIS_FILE, "..pool again, second invocation:     %u", pool_time2));

    if (pool_time2==0) pool_time2=1;
    if (pool_time < pool_time2)
	best = pool_time, worst = pool_time2;
    else
	best = pool_time2, worst = pool_time;

    PJ_LOG(3, (THIS_FILE, "..malloc Speedup best=%dx, worst=%dx", 
			  (int)(malloc_time/best),
			  (int)(malloc_time/worst)));
    return 0;
}


#endif	/* INCLUDE_POOL_PERF_TEST */

