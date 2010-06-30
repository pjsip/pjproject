/* $Id$ */
/*
 * Copyright (C) 2010 Teluu Inc. (http://www.teluu.com)
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

#include <pjlib-util/cli_imp.h>
#include <pjlib-util/cli_console.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pjlib-util/errno.h>

struct cli_console_fe
{
    pj_cli_front_end    base;
    pj_pool_t          *pool;
    pj_cli_sess        *sess;
    pj_thread_t        *input_thread;
    pj_bool_t           thread_quit;
    pj_sem_t           *thread_sem;

    struct async_input_t
    {
        char       *buf;
        unsigned    maxlen;
        pj_sem_t   *sem;
    } input;
};

static void cli_console_write_log(pj_cli_front_end *fe, int level,
		                  const char *data, int len)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    if (cfe->sess->log_level > level)
        printf("%.*s", len, data);
}


static void cli_console_quit(pj_cli_front_end *fe, pj_cli_sess *req)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    PJ_UNUSED_ARG(req);

    pj_assert(cfe);
    if (cfe->input_thread) {
        cfe->thread_quit = PJ_TRUE;
        pj_sem_post(cfe->input.sem);
        pj_sem_post(cfe->thread_sem);
    }
}

static void cli_console_destroy(pj_cli_front_end *fe)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    pj_assert(cfe);
    cli_console_quit(fe, NULL);

    if (cfe->input_thread)
        pj_thread_destroy(cfe->input_thread);
    pj_sem_destroy(cfe->thread_sem);
    pj_sem_destroy(cfe->input.sem);
    pj_pool_release(cfe->pool);
}

PJ_DEF(void) pj_cli_console_cfg_default(pj_cli_console_cfg *param)
{
    pj_assert(param);

    param->log_level = PJ_CLI_CONSOLE_LOG_LEVEL;
}

PJ_DEF(pj_status_t) pj_cli_console_create(pj_cli_t *cli,
					  const pj_cli_console_cfg *param,
					  pj_cli_sess **p_sess,
					  pj_cli_front_end **p_fe)
{
    pj_cli_sess *sess;
    struct cli_console_fe *fe;
    pj_cli_console_cfg cfg;
    pj_pool_t *pool;

    PJ_ASSERT_RETURN(cli && p_sess, PJ_EINVAL);

    pool = pj_pool_create(pj_cli_get_param(cli)->pf, "console_fe",
                          PJ_CLI_CONSOLE_POOL_SIZE, PJ_CLI_CONSOLE_POOL_INC,
                          NULL);
    if (!pool)
        return PJ_ENOMEM;
    sess = PJ_POOL_ZALLOC_T(pool, pj_cli_sess);
    fe = PJ_POOL_ZALLOC_T(pool, struct cli_console_fe);

    if (!param) {
        pj_cli_console_cfg_default(&cfg);
        param = &cfg;
    }
    sess->fe = &fe->base;
    sess->log_level = param->log_level;
    sess->op = PJ_POOL_ZALLOC_T(pool, struct pj_cli_sess_op);
    fe->base.op = PJ_POOL_ZALLOC_T(pool, struct pj_cli_front_end_op);
    fe->base.cli = cli;
    fe->base.type = PJ_CLI_CONSOLE_FRONT_END;
    fe->base.op->on_write_log = &cli_console_write_log;
    fe->base.op->on_quit = &cli_console_quit;
    fe->base.op->on_destroy = &cli_console_destroy;
    fe->pool = pool;
    fe->sess = sess;
    pj_sem_create(pool, "console_fe", 0, 1, &fe->thread_sem);
    pj_sem_create(pool, "console_fe", 0, 1, &fe->input.sem);
    pj_cli_register_front_end(cli, &fe->base);

    *p_sess = sess;
    if (p_fe)
        *p_fe = &fe->base;

    return PJ_SUCCESS;
}

static int readline_thread(void * p)
{
    struct cli_console_fe * fe = (struct cli_console_fe *)p;
    int i;

    while (!fe->thread_quit) {
        fgets(fe->input.buf, fe->input.maxlen, stdin);
        for (i = pj_ansi_strlen(fe->input.buf) - 1; i >= 0; i--) {
            if (fe->input.buf[i] == '\n' || fe->input.buf[i] == '\r')
                fe->input.buf[i] = 0;
            else
                break;
        }
        pj_sem_post(fe->input.sem);
        /* Sleep until the next call of pj_cli_console_readline() */
        pj_sem_wait(fe->thread_sem);
    }
    fe->input_thread = NULL;

    return 0;
}

PJ_DEF(pj_status_t) pj_cli_console_readline(pj_cli_sess *sess,
					    char *buf,
					    unsigned maxlen)
{
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;

    PJ_ASSERT_RETURN(sess && buf, PJ_EINVAL);

    fe->input.buf = buf;
    fe->input.maxlen = maxlen;

    if (!fe->input_thread) {
        pj_status_t status;

        status = pj_thread_create(fe->pool, NULL, &readline_thread, fe,
                                  0, 0, &fe->input_thread);
        if (status != PJ_SUCCESS)
            return status;
    } else {
        /* Wake up readline thread */
        pj_sem_post(fe->thread_sem);
    }

    pj_sem_wait(fe->input.sem);

    return (pj_cli_is_quitting(fe->base.cli)? PJ_CLI_EEXIT : PJ_SUCCESS);
}
