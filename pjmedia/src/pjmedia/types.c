/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/types.h>
#include <pj/assert.h>
#include <pj/string.h>


/* Map structure for pjmedia type names */
typedef struct pjmedia_type_map {
    pjmedia_type type;
    const char* name;
} pjmedia_type_map;

/* Internal mapping for pjmedia type names */
static pjmedia_type_map media_type_names[] = {
    {PJMEDIA_TYPE_NONE,         "none"},
    {PJMEDIA_TYPE_AUDIO,        "audio"},
    {PJMEDIA_TYPE_VIDEO,        "video"},
    {PJMEDIA_TYPE_APPLICATION,  "application"},
    {PJMEDIA_TYPE_UNKNOWN,      "unknown"}
};

/*
 * Utility function to return the string name for a pjmedia_type.
 */
PJ_DEF(const char*) pjmedia_type_name(pjmedia_type t)
{
    pj_assert(t < (int)PJ_ARRAY_SIZE(media_type_names));
    pj_assert(PJMEDIA_TYPE_UNKNOWN == 4);

    if (t < (int)PJ_ARRAY_SIZE(media_type_names))
        return media_type_names[t].name;
    else
        return "??";
}

/*
 * Utility function to return the media type for a media name string.
 */
PJ_DEF(pjmedia_type) pjmedia_get_type(const pj_str_t *name)
{
    int i;
    for (i = 0; i < (int)PJ_ARRAY_SIZE(media_type_names); ++i) {
        if (pj_stricmp2(name, media_type_names[i].name)==0)
            return media_type_names[i].type;
    }
    return PJMEDIA_TYPE_UNKNOWN;
}
