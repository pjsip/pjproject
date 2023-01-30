/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
#include "test.h"

#define THIS_FILE       "json_test.c"

#if INCLUDE_JSON_TEST

#include <pjlib-util/json.h>
#include <pj/log.h>
#include <pj/string.h>

static char json_doc1[] =
"{\
    \"Object\": {\
       \"Integer\":  800,\
       \"Negative\":  -12,\
       \"Float\": -7.2,\
       \"String\":  \"A\\tString with tab\",\
       \"Object2\": {\
           \"True\": true,\
           \"False\": false,\
           \"Null\": null\
       },\
       \"Array1\": [116, false, \"string\", {}],\
       \"Array2\": [\
            {\
                   \"Float\": 123.,\
            },\
            {\
                   \"Float\": 123.,\
            }\
       ]\
     },\
   \"Integer\":  800,\
   \"Array1\": [116, false, \"string\"]\
}\
";

static int json_verify_1()
{
    pj_pool_t *pool;
    pj_json_elem *elem;
    char *out_buf;
    unsigned size;
    pj_json_err_info err;

    pool = pj_pool_create(mem, "json", 1000, 1000, NULL);

    size = (unsigned)strlen(json_doc1);
    elem = pj_json_parse(pool, json_doc1, &size, &err);
    if (!elem) {
        PJ_LOG(1, (THIS_FILE, "  Error: json_verify_1() parse error"));
        goto on_error;
    }

    size = (unsigned)strlen(json_doc1) * 2;
    out_buf = pj_pool_alloc(pool, size);

    if (pj_json_write(elem, out_buf, &size)) {
        PJ_LOG(1, (THIS_FILE, "  Error: json_verify_1() write error"));
        goto on_error;
    }

    PJ_LOG(3,(THIS_FILE, "Json document:\n%s", out_buf));
    pj_pool_release(pool);
    return 0;

on_error:
    pj_pool_release(pool);
    return 10;
}


int json_test(void)
{
    int rc;

    rc = json_verify_1();
    if (rc)
        return rc;

    return 0;
}



#else
int json_dummy;
#endif
