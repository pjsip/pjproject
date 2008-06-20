/* $Id$ */
/* 
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
#include <ctype.h>
#include <pj/pool.h>
#include "test.h"

#define JB_INIT_PREFETCH    0
#define JB_MIN_PREFETCH	    0
#define JB_MAX_PREFETCH	    10
#define JB_PTIME	    20
#define JB_BUF_SIZE	    20

#define REPORT
//#define PRINT_COMMENT

int jbuf_main(void)
{
    pjmedia_jbuf *jb;
    FILE *input = fopen("JBTEST.DAT", "rt");
    unsigned seq;
    char line[1024 * 10], *p;
    pj_pool_t *pool;
    pjmedia_jb_state state;
    pj_str_t jb_name = {"JBTEST", 6};

    pj_init();
    pool = pj_pool_create(mem, "JBPOOL", 256*16, 256*16, NULL);

    pjmedia_jbuf_create(pool, &jb_name, 1, JB_PTIME, JB_BUF_SIZE, &jb);
    pjmedia_jbuf_set_adaptive(jb, JB_INIT_PREFETCH, JB_MIN_PREFETCH, 
			      JB_MAX_PREFETCH);

    while ((p=fgets(line, sizeof(line), input)) != NULL) {

	while (*p && isspace(*p))
	    ++p;

	if (!*p)
	    continue;

	if (*p == '#') {
#ifdef PRINT_COMMENT
	    printf("%s", p);
#endif
	    continue;
	}

	pjmedia_jbuf_reset(jb);
	seq = 1;

#ifdef REPORT
	pjmedia_jbuf_get_state(jb, &state);
	printf("Initial\tsize=%d\tprefetch=%d\tmin.pftch=%d\tmax.pftch=%d\n", 
	    state.size, state.prefetch, state.min_prefetch, state.max_prefetch);
#endif

	while (*p) {
	    int c;
	    char frame[1];
	    char f_type;
	    
	    c = *p++;
	    if (isspace(c))
		continue;
	    
	    if (c == '/') {
		putchar('\n');

		while (*++p && *p != '/')
		    putchar(*p);

		putchar('\n');

		if (*++p == 0)
		    break;

		continue;
	    }

	    switch (toupper(c)) {
	    case 'G':
		pjmedia_jbuf_get_frame(jb, frame, &f_type);
		break;
	    case 'P':
		pjmedia_jbuf_put_frame(jb, (void*)frame, 1, seq);
		seq++;
		break;
	    case 'L':
		seq++;
		printf("Lost\n");
		break;
	    default:
		printf("Unknown character '%c'\n", c);
		break;
	    }

#ifdef REPORT
	    if (toupper(c) != 'L') {
		pjmedia_jbuf_get_state(jb, &state);
		printf("seq=%d\t%c\tsize=%d\tprefetch=%d\n", 
		       seq, toupper(c), state.size, state.prefetch);
	    }
#endif
	}
    }

    pjmedia_jbuf_destroy(jb);

    if (input != stdin)
	fclose(input);

    pj_pool_release(pool);
    return 0;
}
