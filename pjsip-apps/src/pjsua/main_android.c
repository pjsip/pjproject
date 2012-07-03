/* $Id$ */
/* 
 * Copyright (C) 2012-2012 Teluu Inc. (http://www.teluu.com)
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

#include <pjsua-lib/pjsua.h>

#if defined(PJ_ANDROID) && PJ_ANDROID != 0

#define THIS_FILE	"main_android.c"

#include <android/log.h>

#define SHOW_LOG 1

struct {
    pj_caching_pool  caching_pool;
    pj_pool_t       *pool;
    pj_sem_t        *input_sem;
    pj_sem_t        *output_sem;
    pj_sem_t        *out_print_sem;
    char             line[200];
    char             out_buf[1000];
} app_var;

int main(int argc, char *argv[]);
extern pj_log_func *log_cb;

char argv_buf[200];
char *_argv[] = {"", "--config-file", argv_buf};

void setInput(char *s)
{
    app_var.line[0] = 0;
    if (strlen(s) < sizeof(app_var.line))
        strncpy(app_var.line, s, strlen(s));
    pj_sem_post(app_var.input_sem);
}

char * getMessage()
{
    pj_sem_wait(app_var.output_sem);
    return app_var.out_buf;
}

void finishDisplayMsg()
{
    pj_sem_post(app_var.out_print_sem);
}

void showMsg(const char *format, ...)
{
    va_list arg;
    
    va_start(arg, format);
#if SHOW_LOG
    __android_log_vprint(ANDROID_LOG_INFO, "apjsua", format, arg);
#endif
    vsnprintf(app_var.out_buf, sizeof(app_var.out_buf), format, arg);
    va_end(arg);
    
    pj_sem_post(app_var.output_sem);
    pj_sem_wait(app_var.out_print_sem);
}

char * getInput(char *s, int n, FILE *stream)
{
    if (stream != stdin)
        return fgets(s, n, stream);
    
    pj_sem_wait(app_var.input_sem);
    strncpy(s, app_var.line, n);
    return app_var.line;
}

pj_bool_t showNotification(pjsua_call_id call_id)
{
    return PJ_TRUE;
}

void showLog(int level, const char *data, int len)
{
    showMsg("%s", data);
}

void deinitApp()
{
    showMsg("Quitting...");
    
    if (app_var.input_sem) {
        pj_sem_destroy(app_var.input_sem);
        app_var.input_sem = NULL;
    }
    if (app_var.output_sem) {
        pj_sem_destroy(app_var.output_sem);
        app_var.output_sem = NULL;
    }
    if (app_var.out_print_sem) {
        pj_sem_destroy(app_var.out_print_sem);
        app_var.out_print_sem = NULL;
    }
    
    pj_pool_release(app_var.pool);
    pj_caching_pool_destroy(&app_var.caching_pool);
    pj_shutdown();
}

int initApp()
{
    pj_status_t status = PJ_SUCCESS;
    
    pj_init();
    pj_caching_pool_init(&app_var.caching_pool,
                         &pj_pool_factory_default_policy, 0);
    app_var.pool = pj_pool_create(&app_var.caching_pool.factory, "apjsua",
                                  256, 256, 0);

    app_var.input_sem = app_var.output_sem = app_var.out_print_sem = NULL;
    status = pj_sem_create(app_var.pool, NULL, 0, 1, &app_var.input_sem);
    if (status != PJ_SUCCESS)
        goto on_return;
    status = pj_sem_create(app_var.pool, NULL, 0, 1, &app_var.output_sem);
    if (status != PJ_SUCCESS)
        goto on_return;
    status = pj_sem_create(app_var.pool, NULL, 0, 1, &app_var.out_print_sem);
    if (status != PJ_SUCCESS)
        goto on_return;
    
    pj_log_set_log_func(&showLog);
    log_cb = &showLog;
    
    return status;
    
on_return:
    deinitApp();
    return status;
}

int startPjsua(char *cfgFile)
{
    strncpy(argv_buf, cfgFile, sizeof(argv_buf));
    return main(3, _argv);
}

#endif /* PJ_ANDROID */
