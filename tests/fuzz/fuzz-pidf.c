/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjsip.h>
#include <pjsip-simple/pidf.h>
#include <pjsip-simple/xpidf.h>
#include <pjsip-simple/dialog_info.h>
#include <pjsip-simple/iscomposing.h>

#define POOL_SIZE 32000
#define PRINT_BUF_SIZE 8192

/* Global state for one-time initialization */
static pj_caching_pool caching_pool;
static pj_pool_factory *mem;

/* Test PIDF parse and print round-trip */
static void test_pidf(pj_pool_t *pool, char *buf, int len)
{
    pjpidf_pres *pres;
    char print_buf[PRINT_BUF_SIZE];
    int print_len;

    pres = pjpidf_parse(pool, buf, len);
    if (!pres)
        return;

    /* Print and re-parse */
    print_len = pjpidf_print(pres, print_buf, sizeof(print_buf) - 1);
    if (print_len > 0) {
        print_buf[print_len] = '\0';
        pjpidf_parse(pool, print_buf, print_len);
    }
}

/* Test xPIDF parse and print round-trip */
static void test_xpidf(pj_pool_t *pool, char *buf, pj_size_t len)
{
    pjxpidf_pres *pres;
    char print_buf[PRINT_BUF_SIZE];
    int print_len;

    pres = pjxpidf_parse(pool, buf, len);
    if (!pres)
        return;

    /* Print and re-parse */
    print_len = pjxpidf_print(pres, print_buf, sizeof(print_buf) - 1);
    if (print_len > 0) {
        print_buf[print_len] = '\0';
        pjxpidf_parse(pool, print_buf, (pj_size_t)print_len);
    }
}

/* Test dialog-info parse and print round-trip */
static void test_dialog_info(pj_pool_t *pool, char *buf, int len)
{
    pj_xml_node *dlg;
    char print_buf[PRINT_BUF_SIZE];
    int print_len;

    dlg = pjsip_dlg_info_parse(pool, buf, len);
    if (!dlg)
        return;

    /* Print and re-parse */
    print_len = pjsip_dlg_info_print(dlg, print_buf, sizeof(print_buf) - 1);
    if (print_len > 0) {
        print_buf[print_len] = '\0';
        pjsip_dlg_info_parse(pool, print_buf, print_len);
    }
}

/* Test iscomposing parse and body creation */
static void test_iscomposing(pj_pool_t *pool, char *buf, pj_size_t len)
{
    pj_bool_t is_composing;
    pj_str_t *content_type;
    int refresh = 0;
    pj_status_t status;

    status = pjsip_iscomposing_parse(pool, buf, len,
                                     &is_composing,
                                     NULL,
                                     &content_type,
                                     &refresh);
    if (status != PJ_SUCCESS)
        return;

    /* Create body from parsed values; lst_actv is pj_time_val so pass NULL */
    pjsip_iscomposing_create_body(pool, is_composing,
                                  NULL, content_type, refresh);
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    static int initialized = 0;
    pj_pool_t *pool;
    char *buf;

    /* One-time initialization */
    if (!initialized) {
        pj_init();
        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
        pj_log_set_level(0);
        mem = &caching_pool.factory;
        initialized = 1;
    }

    /* Skip tiny inputs */
    if (Size < 4)
        return 0;

    /* Create pool for this iteration */
    pool = pj_pool_create(mem, "pidf", POOL_SIZE, POOL_SIZE, NULL);
    if (!pool)
        return 0;

    /* Copy input to mutable pool buffer for APIs requiring char * */
    buf = (char *)pj_pool_alloc(pool, Size + 1);
    pj_memcpy(buf, Data, Size);
    buf[Size] = '\0';

    /* Run all parsers on every iteration */
    test_pidf(pool, buf, (int)Size);
    test_xpidf(pool, buf, (pj_size_t)Size);
    test_dialog_info(pool, buf, (int)Size);
    test_iscomposing(pool, buf, (pj_size_t)Size);

    /* Release pool */
    pj_pool_release(pool);

    return 0;
}
