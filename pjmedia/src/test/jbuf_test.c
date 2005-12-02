/* $Header: /pjproject/pjmedia/src/test/jbuf_test.c 9     6/24/05 11:18p Bennylp $ */
/* 
 * PJMEDIA - Multimedia over IP Stack 
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
#include <stdio.h>
#include <ctype.h>
#include <pjmedia/jbuf.h>
#include <pj/pool.h>

#define JB_MIN	    1
#define JB_MAX	    8
#define JB_BUF_SIZE 10

#define REPORT
//#define PRINT_COMMENT

int jbuf_main(pj_pool_factory *pf)
{
    pj_jitter_buffer jb;
    FILE *input = fopen("JBTEST.DAT", "rt");
    unsigned lastseq;
    void *data = "Hello world";
    char line[1024], *p;
    int lastget = 0, lastput = 0;
    pj_pool_t *pool;

    pj_init();
    pool = pj_pool_create(pf, "JBPOOL", 256*16, 256*16, NULL);

    pj_jb_init(&jb, pool, JB_MIN, JB_MAX, JB_BUF_SIZE);

    lastseq = 1;

    while ((p=fgets(line, sizeof(line), input)) != NULL) {

	while (*p && isspace(*p))
	    ++p;

	if (!*p)
	    continue;

	if (*p == '#') {
#ifdef PRINT_COMMENT
	    printf("\n%s", p);
#endif
	    continue;
	}

	pj_jb_reset(&jb);
#ifdef REPORT
	printf( "Initial\t%c   size=%d  prefetch=%d level=%d\n", 
	        ' ', jb.lst.count, jb.prefetch, jb.level);
#endif

	while (*p) {
	    int c;
	    unsigned seq = 0;
	    void *thedata;
	    int status = 1234;
	    
	    c = *p++;
	    if (isspace(c))
		continue;
	    
	    if (c == '/') {
		char *end;

		printf("/*");

		do {
		    putchar(*++p);
		} while (*p != '/');

		putchar('\n');

		c = *++p;
		end = p;

	    }

	    if (isspace(c))
		continue;

	    if (isdigit(c)) {
		seq = c - '0';
		while (*p) {
		    c = *p++;
		    
		    if (isspace(c))
			continue;
		    
		    if (!isdigit(c))
			break;
		    
		    seq = seq * 10 + c - '0';
		}
	    }

	    if (!*p)
		break;

	    switch (toupper(c)) {
	    case 'G':
		seq = -1;
		status = pj_jb_get(&jb, &seq, &thedata);
		lastget = seq;
		break;
	    case 'P':
		if (seq == 0) 
		    seq = lastseq++;
		else
		    lastseq = seq;
		status = pj_jb_put(&jb, seq, data);
		if (status == 0)
		    lastput = seq;
		break;
	    default:
		printf("Unknown character '%c'\n", c);
		break;
	    }

#ifdef REPORT
	    printf("seq=%d\t%c rc=%d\tsize=%d\tpfch=%d\tlvl=%d\tmxl=%d\tdelay=%d\n", 
		    seq, toupper(c), status, jb.lst.count, jb.prefetch, jb.level, jb.max_level,
		    (lastget>0 && lastput>0) ? lastput-lastget : -1);
#endif
	}
    }

#ifdef REPORT
    printf("0\t%c   size=%d  prefetch=%d level=%d\n", 
	    ' ', jb.lst.count, jb.prefetch, jb.level);
#endif

    if (input != stdin)
	fclose(input);

    pj_pool_release(pool);
    return 0;
}
