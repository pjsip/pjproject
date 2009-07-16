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

int	    systest_init(void);
int	    systest_run(void);
void	    systest_save_result(const char *filename);
void	    systest_deinit(void);

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
