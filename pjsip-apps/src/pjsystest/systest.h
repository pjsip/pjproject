/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __SYSTEST_H__
#define __SYSTEST_H__

#include <pjlib.h>

/*
 * Overrideable parameters
 */
#define OVERRIDE_AUDDEV_REC_LAT		100
#define OVERRIDE_AUDDEV_PLAY_LAT	200
#define OVERRIDE_AUD_FRAME_PTIME	0
#define CLOCK_RATE			8000
#define CHANNEL_COUNT			1

#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE
    #define LOG_OUT_PATH		"\\PJSYSTEST.LOG"
    #define RESULT_OUT_PATH		"\\PJSYSTEST_RESULT.TXT"
    #define WAV_PLAYBACK_PATH		"\\Program Files\\pjsystest\\input.8.wav"
    #define WAV_REC_OUT_PATH		"\\PJSYSTEST_TESTREC.WAV"
    #define WAV_TOCK8_PATH		"\\Program Files\\pjsystest\\tock8.WAV"
    #define WAV_LATENCY_OUT_PATH	"\\PJSYSTEST_LATREC.WAV"
#else
    #define LOG_OUT_PATH		"PJSYSTEST.LOG"
    #define RESULT_OUT_PATH		"PJSYSTEST.TXT"
    #define WAV_PLAYBACK_PATH		"pjsip8.wav"
    #define WAV_REC_OUT_PATH		"TESTREC.WAV"
    #define WAV_TOCK8_PATH		"TOCK8.WAV"
    #define WAV_LATENCY_OUT_PATH	"LATENCY.WAV"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* API, to be called by main() */
int	    systest_init(void);
int	    systest_run(void);
void	    systest_save_result(const char *filename);
void	    systest_deinit(void);

/* Test item is used to record the test result */
typedef struct test_item_t
{
    char	title[80];
    pj_bool_t	skipped;
    pj_bool_t	success;
    char	reason[1024];
} test_item_t;

#define SYSTEST_MAX_TEST    32
extern unsigned	    test_item_count;
extern test_item_t  test_items[SYSTEST_MAX_TEST];

test_item_t *systest_alloc_test_item(const char *title);

#ifdef __cplusplus
}
#endif

#endif	/* __SYSTEST_H__ */
