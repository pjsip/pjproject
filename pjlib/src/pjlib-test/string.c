/* $Id$
 */
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/log.h>
#include "test.h"

/**
 * \page page_pjlib_string_test Test: String
 *
 * This file provides implementation of \b string_test(). It tests the
 * functionality of the string API.
 *
 * \section sleep_test_sec Scope of the Test
 *
 * API tested:
 *  - pj_str()
 *  - pj_strcmp()
 *  - pj_strcmp2()
 *  - pj_stricmp()
 *  - pj_strlen()
 *  - pj_strncmp()
 *  - pj_strnicmp()
 *  - pj_strchr()
 *  - pj_strdup()
 *  - pj_strdup2()
 *  - pj_strcpy()
 *  - pj_strcat()
 *  - pj_strtrim()
 *  - pj_utoa()
 *  - pj_strtoul()
 *  - pj_create_random_string()
 *
 *
 * This file is <b>pjlib-test/string.c</b>
 *
 * \include pjlib-test/string.c
 */

#if INCLUDE_STRING_TEST

#ifdef _MSC_VER
#   pragma warning(disable: 4204)
#endif

#define HELLO_WORLD	"Hello World"
#define JUST_HELLO	"Hello"
#define UL_VALUE	3456789012UL

int string_test(void)
{
    const pj_str_t hello_world = { HELLO_WORLD, strlen(HELLO_WORLD) };
    const pj_str_t just_hello = { JUST_HELLO, strlen(JUST_HELLO) };
    pj_str_t s1, s2, s3, s4, s5;
    enum { RCOUNT = 10, RLEN = 16 };
    pj_str_t random[RCOUNT];
    pj_pool_t *pool;
    int i;

    pool = pj_pool_create(mem, NULL, 4096, 0, NULL);
    if (!pool) return -5;
    
    /* 
     * pj_str(), pj_strcmp(), pj_stricmp(), pj_strlen(), 
     * pj_strncmp(), pj_strchr() 
     */
    s1 = pj_str(HELLO_WORLD);
    if (pj_strcmp(&s1, &hello_world) != 0)
	return -10;
    if (pj_stricmp(&s1, &hello_world) != 0)
	return -20;
    if (pj_strcmp(&s1, &just_hello) <= 0)
	return -30;
    if (pj_stricmp(&s1, &just_hello) <= 0)
	return -40;
    if (pj_strlen(&s1) != strlen(HELLO_WORLD))
	return -50;
    if (pj_strncmp(&s1, &hello_world, 5) != 0)
	return -60;
    if (pj_strnicmp(&s1, &hello_world, 5) != 0)
	return -70;
    if (pj_strchr(&s1, HELLO_WORLD[1]) != s1.ptr+1)
	return -80;

    /* 
     * pj_strdup() 
     */
    if (!pj_strdup(pool, &s2, &s1))
	return -100;
    if (pj_strcmp(&s1, &s2) != 0)
	return -110;
    
    /* 
     * pj_strcpy(), pj_strcat() 
     */
    s3.ptr = pj_pool_alloc(pool, 256);
    if (!s3.ptr) 
	return -200;
    pj_strcpy(&s3, &s2);
    pj_strcat(&s3, &just_hello);

    if (pj_strcmp2(&s3, HELLO_WORLD JUST_HELLO) != 0)
	return -210;

    /* 
     * pj_strdup2(), pj_strtrim(). 
     */
    pj_strdup2(pool, &s4, " " HELLO_WORLD "\t ");
    pj_strtrim(&s4);
    if (pj_strcmp2(&s4, HELLO_WORLD) != 0)
	return -250;

    /* 
     * pj_utoa() 
     */
    s5.ptr = pj_pool_alloc(pool, 16);
    if (!s5.ptr)
	return -270;
    s5.slen = pj_utoa(UL_VALUE, s5.ptr);

    /* 
     * pj_strtoul() 
     */
    if (pj_strtoul(&s5) != UL_VALUE)
	return -280;

    /* 
     * pj_create_random_string() 
     * Check that no duplicate strings are returned.
     */
    for (i=0; i<RCOUNT; ++i) {
	int j;
	
	random[i].ptr = pj_pool_alloc(pool, RLEN);
	if (!random[i].ptr)
	    return -320;

        random[i].slen = RLEN;
	pj_create_random_string(random[i].ptr, RLEN);

	for (j=0; j<i; ++j) {
	    if (pj_strcmp(&random[i], &random[j])==0)
		return -330;
	}
    }

    /* Done. */
    pj_pool_release(pool);
    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_string_test;
#endif	/* INCLUDE_STRING_TEST */

