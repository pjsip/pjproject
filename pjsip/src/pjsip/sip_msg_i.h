/* $Header: /pjproject/pjsip/src/pjsip/sip_msg_i.h 2     2/24/05 10:46a Bennylp $ */

PJ_IDEF(void) pjsip_msg_add_hdr( pjsip_msg *msg, pjsip_hdr *hdr )
{
    pj_list_insert_before(&msg->hdr, hdr);
}

PJ_IDEF(void) pjsip_msg_insert_first_hdr( pjsip_msg *msg, pjsip_hdr *hdr )
{
    pj_list_insert_after(&msg->hdr, hdr);
}

