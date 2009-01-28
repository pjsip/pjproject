/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/delaybuf.h>
#include <pjmedia/sound.h>
#include <pj/errno.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/unicode.h>
#include <e32cons.h>

#define THIS_FILE		"app_main.cpp"
#define CLOCK_RATE		8000
#define CHANNEL_COUNT		1
#define PTIME			20
#define SAMPLES_PER_FRAME	(CLOCK_RATE*PTIME/1000)
#define BITS_PER_SAMPLE		16

extern CConsoleBase* console;

static pj_caching_pool cp;
static pjmedia_snd_stream *strm;
static unsigned rec_cnt, play_cnt;
static pj_time_val t_start;

pj_pool_t *pool;
pjmedia_delay_buf *delaybuf;

/* Logging callback */
static void log_writer(int level, const char *buf, unsigned len)
{
    static wchar_t buf16[PJ_LOG_MAX_SIZE];

    PJ_UNUSED_ARG(level);

    pj_ansi_to_unicode(buf, len, buf16, PJ_ARRAY_SIZE(buf16));

    TPtrC16 aBuf((const TUint16*)buf16, (TInt)len);
    console->Write(aBuf);
}

/* perror util */
static void app_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(THIS_FILE, "Error: %s: %s", title, errmsg));
}

/* Application init */
static pj_status_t app_init()
{
    unsigned i, count;
    pj_status_t status;

    /* Redirect log */
    pj_log_set_log_func((void (*)(int,const char*,int)) &log_writer);
    pj_log_set_decor(PJ_LOG_HAS_NEWLINE);
    pj_log_set_level(3);

    /* Init pjlib */
    status = pj_init();
    if (status != PJ_SUCCESS) {
    	app_perror("pj_init()", status);
    	return status;
    }

    pj_caching_pool_init(&cp, NULL, 0);

    /* Init sound subsystem */
    status = pjmedia_snd_init(&cp.factory);
    if (status != PJ_SUCCESS) {
    	app_perror("pjmedia_snd_init()", status);
        pj_caching_pool_destroy(&cp);
    	pj_shutdown();
    	return status;
    }

    count = pjmedia_snd_get_dev_count();
    PJ_LOG(3,(THIS_FILE, "Device count: %d", count));
    for (i=0; i<count; ++i) {
    	const pjmedia_snd_dev_info *info;

    	info = pjmedia_snd_get_dev_info(i);
    	PJ_LOG(3, (THIS_FILE, "%d: %s %d/%d %dHz",
    		   i, info->name, info->input_count, info->output_count,
    		   info->default_samples_per_sec));
    }

    /* Create pool */
    pool = pj_pool_create(&cp.factory, THIS_FILE, 512, 512, NULL);
    if (pool == NULL) {
    	app_perror("pj_pool_create()", status);
        pj_caching_pool_destroy(&cp);
    	pj_shutdown();
    	return status;
    }

    /* Init delay buffer */
    status = pjmedia_delay_buf_create(pool, THIS_FILE, CLOCK_RATE,
				      SAMPLES_PER_FRAME, CHANNEL_COUNT,
				      0, 0, &delaybuf);
    if (status != PJ_SUCCESS) {
    	app_perror("pjmedia_delay_buf_create()", status);
        //pj_caching_pool_destroy(&cp);
    	//pj_shutdown();
    	//return status;
    }

    return PJ_SUCCESS;
}


/* Sound capture callback */
static pj_status_t rec_cb(void *user_data,
			  pj_uint32_t timestamp,
			  void *input,
			  unsigned size)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(timestamp);
    PJ_UNUSED_ARG(size);

    pjmedia_delay_buf_put(delaybuf, (pj_int16_t*)input);

    if (size != SAMPLES_PER_FRAME*2) {
		PJ_LOG(3, (THIS_FILE, "Size captured = %u",
	 		   size));
    }

    ++rec_cnt;
    return PJ_SUCCESS;
}

/* Play cb */
static pj_status_t play_cb(void *user_data,
			   pj_uint32_t timestamp,
			   void *output,
			   unsigned size)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(timestamp);
    PJ_UNUSED_ARG(size);

    pjmedia_delay_buf_get(delaybuf, (pj_int16_t*)output);

    ++play_cnt;
    return PJ_SUCCESS;
}

/* Start sound */
static pj_status_t snd_start(unsigned flag)
{
    pj_status_t status;

    if (strm != NULL) {
    	app_perror("snd already open", PJ_EINVALIDOP);
    	return PJ_EINVALIDOP;
    }

    if (flag==PJMEDIA_DIR_CAPTURE_PLAYBACK)
    	status = pjmedia_snd_open(-1, -1, CLOCK_RATE, CHANNEL_COUNT,
    				  SAMPLES_PER_FRAME, BITS_PER_SAMPLE,
    				  &rec_cb, &play_cb, NULL, &strm);
    else if (flag==PJMEDIA_DIR_CAPTURE)
    	status = pjmedia_snd_open_rec(-1, CLOCK_RATE, CHANNEL_COUNT,
    				      SAMPLES_PER_FRAME, BITS_PER_SAMPLE,
    				      &rec_cb, NULL, &strm);
    else
    	status = pjmedia_snd_open_player(-1, CLOCK_RATE, CHANNEL_COUNT,
    					 SAMPLES_PER_FRAME, BITS_PER_SAMPLE,
    					 &play_cb, NULL, &strm);

    if (status != PJ_SUCCESS) {
    	app_perror("snd open", status);
    	return status;
    }

    rec_cnt = play_cnt = 0;
    pj_gettimeofday(&t_start);

    pjmedia_delay_buf_reset(delaybuf);

    status = pjmedia_snd_stream_start(strm);
    if (status != PJ_SUCCESS) {
    	app_perror("snd start", status);
    	pjmedia_snd_stream_close(strm);
    	strm = NULL;
    	return status;
    }

    return PJ_SUCCESS;
}

/* Stop sound */
static pj_status_t snd_stop()
{
    pj_time_val now;
    pj_status_t status;

    if (strm == NULL) {
    	app_perror("snd not open", PJ_EINVALIDOP);
    	return PJ_EINVALIDOP;
    }

    status = pjmedia_snd_stream_stop(strm);
    if (status != PJ_SUCCESS) {
    	app_perror("snd failed to stop", status);
    }
    status = pjmedia_snd_stream_close(strm);
    strm = NULL;

    pj_gettimeofday(&now);
    PJ_TIME_VAL_SUB(now, t_start);

    PJ_LOG(3,(THIS_FILE, "Duration: %d.%03d", now.sec, now.msec));
    PJ_LOG(3,(THIS_FILE, "Captured: %d", rec_cnt));
    PJ_LOG(3,(THIS_FILE, "Played: %d", play_cnt));

    return status;
}

/* Shutdown application */
static void app_fini()
{
    if (strm)
    	snd_stop();

    pjmedia_snd_deinit();
    pjmedia_delay_buf_destroy(delaybuf);
    pj_pool_release(pool);
    pj_caching_pool_destroy(&cp);
    pj_shutdown();
}


////////////////////////////////////////////////////////////////////////////
/*
 * The interractive console UI
 */
#include <e32base.h>

class ConsoleUI : public CActive
{
public:
    ConsoleUI(CConsoleBase *con);

    // Run console UI
    void Run();

    // Stop
    void Stop();

protected:
    // Cancel asynchronous read.
    void DoCancel();

    // Implementation: called when read has completed.
    void RunL();

private:
    CConsoleBase *con_;
};


ConsoleUI::ConsoleUI(CConsoleBase *con)
: CActive(EPriorityUserInput), con_(con)
{
    CActiveScheduler::Add(this);
}

// Run console UI
void ConsoleUI::Run()
{
    con_->Read(iStatus);
    SetActive();
}

// Stop console UI
void ConsoleUI::Stop()
{
    DoCancel();
}

// Cancel asynchronous read.
void ConsoleUI::DoCancel()
{
    con_->ReadCancel();
}

static void PrintMenu()
{
    PJ_LOG(3, (THIS_FILE, "\n\n"
	    "Menu:\n"
	    "  a    Start bidir sound\n"
	    "  t    Start recorder\n"
	    "  p    Start player\n"
	    "  d    Stop & close sound\n"
	    "  w    Quit\n"));
}

// Implementation: called when read has completed.
void ConsoleUI::RunL()
{
    TKeyCode kc = con_->KeyCode();
    pj_bool_t reschedule = PJ_TRUE;

    switch (kc) {
    case 'w':
	    snd_stop();
	    CActiveScheduler::Stop();
	    reschedule = PJ_FALSE;
	    break;
    case 'a':
    	snd_start(PJMEDIA_DIR_CAPTURE_PLAYBACK);
	break;
    case 't':
    	snd_start(PJMEDIA_DIR_CAPTURE);
	break;
    case 'p':
    	snd_start(PJMEDIA_DIR_PLAYBACK);
    break;
    case 'd':
    	snd_stop();
	break;
    default:
	    PJ_LOG(3,(THIS_FILE, "Keycode '%c' (%d) is pressed",
		      kc, kc));
	    break;
    }

    PrintMenu();

    if (reschedule)
	Run();
}


////////////////////////////////////////////////////////////////////////////
int app_main()
{
    if (app_init() != PJ_SUCCESS)
        return -1;

    // Run the UI
    ConsoleUI *con = new ConsoleUI(console);

    con->Run();

    PrintMenu();
    CActiveScheduler::Start();

    delete con;

    app_fini();
    return 0;
}

