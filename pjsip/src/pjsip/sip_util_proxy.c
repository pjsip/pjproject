/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjsip/sip_util.h>
#include <pjsip/sip_errno.h>
#include <pj/assert.h>

PJ_DEF(pj_status_t) pjsip_endpt_create_request_fwd(  pjsip_endpoint *endpt,
						     pjsip_rx_data *rdata, 
						     const pjsip_uri *uri,
						     const pj_str_t *branch,
						     unsigned options,
						     pjsip_tx_data **tdata)
{
    PJ_UNUSED_ARG(endpt);
    PJ_UNUSED_ARG(rdata);
    PJ_UNUSED_ARG(uri);
    PJ_UNUSED_ARG(branch);
    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(tdata);

    pj_assert(!"Not implemented yet");
    return PJ_EBUG;
}


PJ_DEF(pj_status_t) pjsip_endpt_create_response_fwd( pjsip_endpoint *endpt,
						     pjsip_rx_data *rdata, 
						     unsigned options,
						     pjsip_tx_data **tdata)
{
    PJ_UNUSED_ARG(endpt);
    PJ_UNUSED_ARG(rdata);
    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(tdata);

    pj_assert(!"Not implemented yet");
    return PJ_EBUG;
}


PJ_DEF(pj_str_t) pjsip_calculate_branch_id( pjsip_rx_data *rdata )
{
    pj_str_t empty_str = { NULL, 0 };

    PJ_UNUSED_ARG(rdata);
    pj_assert(!"Not implemented yet");
    return empty_str;
}


