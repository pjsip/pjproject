/* 
 * Copyright (C) 2021-2021 Teluu Inc. (http://www.teluu.com)
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
#include <pj/guid.h>
#include <pj/assert.h>
#include <pj/os.h>
#include <pj/string.h>
#include <uuid.h>

PJ_DEF_DATA(const unsigned) PJ_GUID_STRING_LENGTH = 36;

PJ_DEF(unsigned) pj_GUID_STRING_LENGTH()
{
    return PJ_GUID_STRING_LENGTH;
}

PJ_DEF(pj_str_t *) pj_generate_unique_string(pj_str_t *str)
{
    uint32_t status;
    uuid_t u;
    char *s = NULL;
    pj_str_t sguid;

    PJ_ASSERT_RETURN(str && str->ptr, NULL);
    PJ_CHECK_STACK();

    uuid_create(&u, &status);
    PJ_ASSERT_RETURN(status == uuid_s_ok, NULL);
    uuid_to_string(&u, &s, &status);
    PJ_ASSERT_RETURN(status == uuid_s_ok, NULL);
    sguid = pj_str(s);
    pj_strncpy(str, &sguid, PJ_GUID_STRING_LENGTH);
    free(s);

    return str;
}
