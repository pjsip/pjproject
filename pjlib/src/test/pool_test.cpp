/* $Header: /pjproject/pjlib/src/test/pool_test.cpp 6     5/12/05 9:53p Bennylp $
 */
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
#include "libpj_test.h"
#include <pj/pool.h>
#include <pj/os.h>
#include <malloc.h>
#include <new>
#include <stdlib.h>
#include <stdio.h>

#if !PJ_HAS_HIGH_RES_TIMER
# error Need high resolution timer for this test.
#endif


#define LOOP	    10
#define COUNT	    1024
static unsigned	    sizes[COUNT];
#define MIN_SIZE    4
#define MAX_SIZE    512
static unsigned total_size;

static int pool_test_pool()
{
    pj_pool_t *pool = (*mem->create_pool)(mem, NULL, total_size + 4*COUNT, 0, NULL);
    if (!pool)
	return -1;

    for (int i=0; i<COUNT; ++i) {
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

static int pool_test_new_delete()
{
    char *p[COUNT];
    int i;

    try {
	for (i=0; i<COUNT; ++i) {
	    p[i] = new char[sizes[i]];
	    if (!p[i])
		return -1;
	    *p[i] = '\0';
	}

	for (i=0; i<COUNT; ++i) {
	    delete [] p[i];
	}
    }
    catch (std::bad_alloc & ) {
	// Don't care for memory leak in this test
	return -1;
    }

    return 0;
}

int pool_test()
{
    unsigned i;
    pj_uint32_t pool_time=0, malloc_time=0, new_time=0, pool_time2=0;
    pj_hr_timestamp start, end;

    // Initialize sizes.
    for (i=0; i<COUNT; ++i) {
	sizes[i] = MIN_SIZE + rand() % MAX_SIZE;
	total_size += sizes[i];
    }

    printf("Benchmarking pool..\n");

    // Warmup
    pool_test_pool();
    pool_test_malloc_free();
    pool_test_new_delete();

    for (i=0; i<LOOP; ++i) {
	pj_hr_gettimestamp(&start);
	if (pool_test_pool()) {
	    printf("...Error: error in testing pool\n");
	    return -1;
	}
	pj_hr_gettimestamp(&end);
	pool_time += (end.u32.lo - start.u32.lo);

	pj_hr_gettimestamp(&start);
	if (pool_test_malloc_free()) {
	    printf("...Error: error in testing malloc\n");
	    return -1;
	}
	pj_hr_gettimestamp(&end);
	malloc_time += (end.u32.lo - start.u32.lo);

	pj_hr_gettimestamp(&start);
	if (pool_test_new_delete()) {
	    printf("...Error: error in testing new operator\n");
	    return -1;
	}
	pj_hr_gettimestamp(&end);
	new_time += (end.u32.lo - start.u32.lo);

	pj_hr_gettimestamp(&start);
	if (pool_test_pool()) {
	    printf("...Error: error in testing pool\n");
	    return -1;
	}
	pj_hr_gettimestamp(&end);
	pool_time2 += (end.u32.lo - start.u32.lo);
    }

    printf("..LOOP count:                        %u\n", LOOP);
    printf("..number of alloc/dealloc per loop:  %u\n", COUNT);
    printf("..pool allocation/deallocation time: %u\n", pool_time);
    printf("..malloc/free time:                  %u\n", malloc_time);
    printf("..new/delete time:                   %u\n", new_time);
    printf("..pool again, second invocation:     %u\n", pool_time);
    return 0;
}
