/* $Header: /pjproject/pjlib/src/test/os_test.cpp 3     5/12/05 9:53p Bennylp $
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
#include <pj/os.h>
#include <pj/pool.h>
#include <stdio.h>
#include "libpj_test.h"


#define LOOP	1024

static unsigned int shared_var;
static pj_mutex_t *mutex;

static void * PJ_THREAD_FUNC thread_proc(void *arg)
{
    int i;
    unsigned int *pvar = (unsigned int*)arg;

    for (i=0; i<LOOP; ++i) {
	pj_mutex_lock(mutex);
	(*pvar)++;
	shared_var++;
	pj_mutex_unlock(mutex);
	pj_thread_sleep(0);
    }
    return 0;
}

int os_test()
{
    pj_pool_t *pool;
    int err=0;
    unsigned var1=0, var2=0, var3=0;
    pj_thread_t *t1, *t2, *t3;
    unsigned starvation_level = 0;

    pool = (*mem->create_pool)(mem, NULL, 4096, 0, NULL);
    mutex = pj_mutex_create(pool, NULL, 0);
    if (!mutex)
	return -1;

    t1 = pj_thread_create(pool, NULL, &thread_proc, &var1, 0, NULL, 0);
    t2 = pj_thread_create(pool, NULL, &thread_proc, &var2, 0, NULL, 0);
    t3 = pj_thread_create(pool, NULL, &thread_proc, &var3, 0, NULL, 0);

    if (!t1 || !t2 || !t3) {
	puts("...Error:Unable to create thread");
	return -1;
    }

    do {
	int break_flag = 0;
	unsigned lowest = 0xFFFFFFFF, highest = 0;
	pj_thread_sleep(0);

	pj_mutex_lock(mutex);
	if (shared_var == LOOP * 3)
	    break_flag = 1;
	if (var1 < lowest) lowest = var1;
	if (var2 < lowest) lowest = var2;
	if (var3 < lowest) lowest = var3;
	if (var1 > highest) highest = var1;
	if (var2 > highest) highest = var2;
	if (var3 > highest) highest = var3;
	pj_mutex_unlock(mutex);

	if (highest - lowest > 1)
	    starvation_level += (highest - lowest - 1);

	if (break_flag)
	    break;
    } while(1);

    pj_thread_join(t1); pj_thread_destroy(t1);
    pj_thread_join(t2); pj_thread_destroy(t2);
    pj_thread_join(t3); pj_thread_destroy(t3);

    if (var1 != var2 || var2 != var3 || var3 != LOOP) {
	printf("...Error: var1=%d, var2=%d, var3=%d\n", var1, var2, var3);
	return -1;
    }

    if (shared_var != LOOP * 3) {
	printf("...Error: shared_var=%d\n", shared_var);
	return -1;
    }

    printf("...ok, starvation_level:%d\n", starvation_level);
    return err;
}

