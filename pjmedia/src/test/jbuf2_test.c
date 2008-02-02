/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/jbuf2.h>
#include <pj/pool.h>

#pragma warning(disable: 4996)

#define REPORT
#define PRINT_COMMENT

#define SAMPLES_PER_FRAME   160
#define FRAME_SIZE	    (SAMPLES_PER_FRAME*2)

int jbuf2_main(pj_pool_factory *pf)
{
    pjmedia_jb2_t	*jb;
    pjmedia_jb2_setting  jb_setting;
    pjmedia_jb2_cb	 jb_cb;
    pjmedia_jb2_frame	 jb_get_frame;
    pjmedia_jb2_frame	 jb_put_frame;
    pjmedia_jb2_state	 jb_last_state;
    pjmedia_jb2_state	 jb_state;
    pjmedia_jb2_stat	 jb_stat;
    FILE *input = fopen("..\\bin\\JB2TEST.DAT", "rt");
    char line[1024*128], *p;
    pj_pool_t *pool;
    pj_status_t status;
    char dummy[640];

    pj_init();
    pool = pj_pool_create(pf, "JBPOOL", 256*16, 256*16, NULL);

    jb_setting.max_frames = 0;
    jb_setting.samples_per_frame = SAMPLES_PER_FRAME;
    jb_setting.frame_size = FRAME_SIZE;

    pj_bzero(&jb_cb, sizeof(jb_cb));
    pj_bzero(&jb_get_frame, sizeof(jb_get_frame));
    pj_bzero(&jb_put_frame, sizeof(jb_put_frame));

    pjmedia_jb2_create(pool, NULL, &jb_setting, &jb_cb, &jb);

    jb_put_frame.type = PJMEDIA_JB_FT_NORMAL_RAW_FRAME;
    jb_put_frame.size = jb_setting.frame_size;
    jb_put_frame.buffer = dummy;
    
    jb_get_frame.buffer = dummy;

    while ((p=fgets(line, sizeof(line), input)) != NULL) {
	int i;

	while (*p && isspace(*p))
	    ++p;

	if (!*p)
	    continue;

	/* RTrim */
	i = strlen(p);
	while (--i >= 0 && p[i] == '\r' || p[i] == '\n' || p[i] == ' ')
	    p[i] = '\0';

	if (*p == '#') {
#ifdef PRINT_COMMENT
	    printf("%s\n", p+1);
#endif
	    continue;
	}

	/* ignored line */
	if (!*p || *p == ';') {
	    if (*p && *(p+1)==';')
		break;

	    continue;
	}

	pjmedia_jb2_reset(jb);
	pj_bzero(&jb_last_state, sizeof(jb_last_state));
	pj_bzero(&jb_put_frame, sizeof(jb_put_frame));
	jb_put_frame.type = PJMEDIA_JB_FT_NORMAL_RAW_FRAME;
	jb_put_frame.size = jb_setting.frame_size;
	jb_put_frame.buffer = dummy;

	while (*p) {
	    int c;
	    unsigned seq = 0;
	    
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

	    switch (toupper(c)) {
	    case 'G': /* get */
		printf("G");
		jb_get_frame.size = jb_setting.frame_size;
		status = pjmedia_jb2_get_frame(jb, &jb_get_frame);
		break;
	    case 'P': /* put */
		printf("P");
		status = pjmedia_jb2_put_frame(jb, &jb_put_frame);
		jb_put_frame.ts += jb_setting.samples_per_frame;
		jb_put_frame.seq += 1;
		break;
	    case 'L': /* loss */
		printf("L");
		jb_put_frame.ts += jb_setting.samples_per_frame;
		jb_put_frame.seq += 1;
		break;
	    default:
		printf("Unknown character '%c'\n", c);
		break;
	    }
	    pjmedia_jb2_get_state(jb, &jb_state);
	    if (jb_state.drift != jb_last_state.drift
		|| jb_state.level != jb_last_state.level
		|| jb_state.opt_size != jb_last_state.opt_size
		) 
	    {
		printf("\n"); 
		printf("drift:%3d/%d ", jb_state.drift, jb_state.drift_span); 
		printf("level:%3d ", jb_state.level); 
		printf("cur_size:%5d ", jb_state.cur_size); 
		printf("opt_size:%5d ", jb_state.opt_size); 
		printf("\n"); 
		jb_last_state = jb_state;
	    }
	}

#ifdef REPORT
	pjmedia_jb2_get_state(jb, &jb_state);
	printf("\nStop condition:\n"); 
	printf("drift:%3d/%d ", jb_state.drift, jb_state.drift_span); 
	printf("level:%3d ", jb_state.level); 
	printf("cur_size:%5d ", jb_state.cur_size); 
	printf("opt_size:%5d ", jb_state.opt_size); 
	printf("frame_cnt:%d ", jb_state.frame_cnt); 
	printf("\n"); 

	pjmedia_jb2_get_stat(jb, &jb_stat);
	printf("lost\t\t = %d\n", jb_stat.lost); 
	printf("ooo\t\t = %d\n", jb_stat.ooo); 
	printf("full\t\t = %d\n", jb_stat.full); 
	printf("empty\t\t = %d\n", jb_stat.empty); 
	printf("out\t\t = %d\n", jb_stat.out); 
	printf("in\t\t = %d\n", jb_stat.in); 
	printf("max_size\t = %d\n", jb_stat.max_size); 
	printf("max_comp\t = %d\n", jb_stat.max_comp); 
	printf("max_drift\t = %d/%d\n", jb_stat.max_drift, 
					jb_stat.max_drift_span); 
#endif

    }

    if (input != stdin)
	fclose(input);

    pj_pool_release(pool);
    return 0;
}
