/* 
 * Copyright (C) 2023 Teluu Inc. (http://www.teluu.com)
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
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>

#include <pjmedia/rtcp.h>

#define kMinInputLength 10
#define kMaxInputLength 5120

int rtcp_parser(char *data, size_t size)
{

    int ret = 0;
    pjmedia_rtcp_session_setting setting;
    pjmedia_rtcp_session session;

    pjmedia_rtcp_session_setting_default(&setting);
    pjmedia_rtcp_init2(&session, &setting);

    pjmedia_rtcp_rx_rtcp(&session, data, size);

    return ret;
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    char *data;
    int ret = 0;

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    /* Add null termination for the data */
    data = (char *)calloc((Size+1), sizeof(char));
    memcpy((void *)data, (void *)data, Size);

    /* Init */
    pj_init();
    pj_log_set_level(0);

    /* Fuzz */
    ret = rtcp_parser(data, Size);

    free(data);

    return ret;
}
