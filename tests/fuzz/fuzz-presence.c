#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pjsip.h>
#include <pjlib.h>
#include <pjsip-simple/pidf.h>
#include <pjsip-simple/xpidf.h>
#include <pjsip-simple/rpid.h>
#include <pjsip-simple/dialog_info.h>
#include <pjsip-simple/iscomposing.h>

static pj_caching_pool caching_pool;
static pj_pool_t *pool = NULL;
static int initialized = 0;

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    char print_buf[2048];

    if (!initialized) {
        pj_init();
        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
        initialized = 1;
    }

    if (Size == 0) return 0;

    pool = pj_pool_create(&caching_pool.factory, "fuzz-presence", 4000, 4000, NULL);
    if (!pool) return 0;

    char *doc = pj_pool_alloc(pool, Size + 1);
    if (!doc) {
        pj_pool_release(pool);
        return 0;
    }
    memcpy(doc, Data, Size);
    doc[Size] = '\0';

    /* Test PIDF parser */
    pjpidf_pres *pidf_pres = pjpidf_parse(pool, doc, (int)Size);
    if (pidf_pres) {
        pjpidf_print(pidf_pres, print_buf, sizeof(print_buf));

        pjpidf_tuple *tuple = pjpidf_pres_get_first_tuple(pidf_pres);
        if (tuple) {
            /* Call only safe accessor functions without assertions */
            pjpidf_tuple_get_contact(tuple);
            pjpidf_tuple_get_timestamp(tuple);

            pjpidf_note *note = pjpidf_tuple_get_first_note(tuple);
            while (note) {
                note = pjpidf_tuple_get_next_note(tuple, note);
            }
        }

        pjpidf_pres_get_first_note(pidf_pres);

        /* Test RPID within PIDF */
        pjrpid_element rpid_elem;
        if (pjrpid_get_element(pidf_pres, pool, &rpid_elem) == PJ_SUCCESS) {
            (void)rpid_elem.type;
            (void)rpid_elem.activity;
            if (rpid_elem.note.slen > 0) {
                (void)rpid_elem.note.ptr;
            }
        }
    }

    /* Test XPIDF parser */
    pjxpidf_pres *xpidf_pres = pjxpidf_parse(pool, doc, (pj_size_t)Size);
    if (xpidf_pres) {
        pjxpidf_print(xpidf_pres, print_buf, sizeof(print_buf));
        pjxpidf_get_uri(xpidf_pres);
        pjxpidf_get_status(xpidf_pres);
    }

    /* Test Dialog-Info parser */
    pjsip_dlg_info_dialog *dlg_info = pjsip_dlg_info_parse(pool, doc, (int)Size);
    if (dlg_info) {
        pj_xml_attr *attr = dlg_info->attr_head.next;
        while (attr != &dlg_info->attr_head) {
            if (attr->name.slen > 0) (void)attr->name.ptr;
            if (attr->value.slen > 0) (void)attr->value.ptr;
            attr = attr->next;
        }

        pj_xml_node *child = dlg_info->node_head.next;
        while (child != (pj_xml_node*)&dlg_info->node_head) {
            if (child->name.slen > 0) (void)child->name.ptr;
            if (child->content.slen > 0) (void)child->content.ptr;
            child = child->next;
        }
    }

    /* Test Iscomposing parser */
    pj_bool_t is_composing = PJ_FALSE;
    pj_str_t *last_active = NULL;
    pj_str_t *content_type = NULL;
    int refresh = 0;

    if (pjsip_iscomposing_parse(pool, doc, (pj_size_t)Size,
                                &is_composing, &last_active,
                                &content_type, &refresh) == PJ_SUCCESS) {
        (void)is_composing;
        (void)refresh;
        if (last_active && last_active->slen > 0) (void)last_active->ptr;
        if (content_type && content_type->slen > 0) (void)content_type->ptr;
    }

    pj_pool_release(pool);
    return 0;
}

