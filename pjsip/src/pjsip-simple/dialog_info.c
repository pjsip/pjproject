/*
 * Copyright (C) 2024 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2013 Maxim Kondratenko <max.kondr@gmail.com>
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
#include <pjsip-simple/dialog_info.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>

static pj_str_t DIALOG_INFO = { "dialog-info", 11 };
static pj_str_t VERSION = { "version", 7 };
static pj_str_t DIALOG = { "dialog", 6};
static pj_str_t STATE = { "state", 5 };
static pj_str_t DURATION = { "duration", 8 };
static pj_str_t ID = { "id", 2 };
static pj_str_t LOCAL = { "local", 5 };
static pj_str_t IDENTITY = { "identity", 8 };
static pj_str_t DISPLAY = { "display", 7 };
static pj_str_t TARGET = { "target", 6 };
static pj_str_t URI = { "uri", 3 };
static pj_str_t REMOTE = { "remote", 6 };
static pj_str_t CALL_ID = { "call-id", 7 };
static pj_str_t ENTITY = { "entity", 6 };
static pj_str_t DIRECTION = { "direction", 9 };
static pj_str_t REMOTE_TAG = { "remote-tag", 10 };
static pj_str_t LOCAL_TAG = { "local-tag", 9 };
static pj_str_t EMPTY_STRING = { NULL, 0 };

/*
static pj_str_t XMLNS = { "xmlns", 5 };
static pj_str_t DIALOG_INFO_XMLNS = {"urn:ietf:params:xml:ns:dialog-info", 34};
*/

static void xml_init_node(pj_pool_t *pool, pj_xml_node *node,
                          pj_str_t *name, const pj_str_t *value)
{
    pj_list_init(&node->attr_head);
    pj_list_init(&node->node_head);
    node->name = *name;
    if (value) pj_strdup(pool, &node->content, value);
    else node->content.ptr=NULL, node->content.slen=0;
}

static pj_xml_attr* xml_create_attr(pj_pool_t *pool, pj_str_t *name,
                                    const pj_str_t *value)
{
    pj_xml_attr *attr = PJ_POOL_ALLOC_T(pool, pj_xml_attr);
    attr->name = *name;
    pj_strdup(pool, &attr->value, value);
    return attr;
}

/* Remote */
PJ_DEF(void) pjsip_dlg_info_remote_construct(pj_pool_t *pool,
                                             pjsip_dlg_info_remote *remote)
{
    pj_xml_node *node;

    xml_init_node(pool, remote, &REMOTE, NULL);
    node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
    xml_init_node(pool, node, NULL, NULL);
    pj_xml_add_node(remote, node);
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_remote_get_identity(const pjsip_dlg_info_remote *remote)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)remote, &IDENTITY);
    if (!node)
        return &EMPTY_STRING;
    return &node->content;
}

PJ_DEF(void) pjsip_dlg_info_remote_add_identity(pj_pool_t *pool,
                                                pjsip_dlg_info_remote *remote,
                                                const pj_str_t *identity)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)remote, &IDENTITY);
    if (!node) {
        node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
        xml_init_node(pool, node, &IDENTITY, identity);
        pj_xml_add_node(remote, node);
    } else {
        pj_strdup(pool, &node->content, identity);
    }
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_remote_get_identity_display(const pjsip_dlg_info_remote *remote)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)remote, &IDENTITY);
    pj_xml_attr *attr;

    if (!node)
        return &EMPTY_STRING;
    attr = pj_xml_find_attr(node, &DISPLAY, NULL);
    if (!attr)
        return &EMPTY_STRING;
    return &attr->value;
}

PJ_DEF(void) 
pjsip_dlg_info_remote_set_identity_display(pj_pool_t *pool,
                                           pjsip_dlg_info_remote *remote,
                                           const pj_str_t *identity_display)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)remote, &IDENTITY);
    pj_xml_attr *attr;

    if (!node) {
        node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
        xml_init_node(pool, node, &IDENTITY, NULL);
        pj_xml_add_node(remote, node);
    }
    attr = pj_xml_find_attr(node, &DISPLAY, NULL);
    if (!attr) {
        attr = xml_create_attr(pool, &DISPLAY, identity_display);
        pj_xml_add_attr(node, attr);
    } else {
        pj_strdup(pool, &attr->value, identity_display);
    }
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_remote_get_target_uri(const pjsip_dlg_info_remote *remote)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)remote, &TARGET);
    pj_xml_attr *attr;

    if (!node)
        return &EMPTY_STRING;
    attr = pj_xml_find_attr(node, &URI, NULL);
    if (!attr)
        return &EMPTY_STRING;
    return &attr->value;
}

PJ_DEF(void)
pjsip_dlg_info_remote_set_target_uri(pj_pool_t *pool,
                                     pjsip_dlg_info_remote *remote,
                                     const pj_str_t * target_uri)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)remote, &TARGET);
    pj_xml_attr *attr;

    if (!node) {
        node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
        xml_init_node(pool, node, &TARGET, NULL);
        pj_xml_add_node(remote, node);
    }
    attr = pj_xml_find_attr(node, &URI, NULL);
    if (!attr) {
        attr = xml_create_attr(pool, &URI, target_uri);
        pj_xml_add_attr(node, attr);
    } else {
        pj_strdup(pool, &attr->value, target_uri);
    }
}


/* Local */
PJ_DEF(void) pjsip_dlg_info_local_construct(pj_pool_t *pool,
                                            pjsip_dlg_info_local *local)
{
    pj_xml_node *node;

    xml_init_node(pool, local, &LOCAL, NULL);
    node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
    xml_init_node(pool, node, NULL, NULL);
    pj_xml_add_node(local, node);
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_local_get_identity(const pjsip_dlg_info_local *local)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)local, &IDENTITY);
    if (!node)
        return &EMPTY_STRING;
    return &node->content;
}

PJ_DEF(void) pjsip_dlg_info_local_add_identity(pj_pool_t *pool,
                                               pjsip_dlg_info_local *local,
                                               const pj_str_t *identity)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)local, &IDENTITY);
    if (!node) {
        node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
        xml_init_node(pool, node, &IDENTITY, identity);
        pj_xml_add_node(local, node);
    } else {
        pj_strdup(pool, &node->content, identity);
    }
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_local_get_identity_display(const pjsip_dlg_info_local *local)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)local, &IDENTITY);
    pj_xml_attr *attr;

    if (!node)
        return &EMPTY_STRING;
    attr = pj_xml_find_attr(node, &DISPLAY, NULL);
    if (!attr)
        return &EMPTY_STRING;
    return &attr->value;
}

PJ_DEF(void)
pjsip_dlg_info_local_set_identity_display(pj_pool_t *pool,
                                          pjsip_dlg_info_local *local,
                                          const pj_str_t *identity_display)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)local, &IDENTITY);
    pj_xml_attr *attr;

    if (!node) {
        node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
        xml_init_node(pool, node, &IDENTITY, NULL);
        pj_xml_add_node(local, node);
    }
    attr = pj_xml_find_attr(node, &DISPLAY, NULL);
    if (!attr) {
        attr = xml_create_attr(pool, &DISPLAY, identity_display);
        pj_xml_add_attr(node, attr);
    } else {
        pj_strdup(pool, &attr->value, identity_display);
    }
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_local_get_target_uri(const pjsip_dlg_info_local *local)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)local, &TARGET);
    pj_xml_attr *attr;

    if (!node)
        return &EMPTY_STRING;
    attr = pj_xml_find_attr(node, &URI, NULL);
    if (!attr)
        return &EMPTY_STRING;
    return &attr->value;
}

PJ_DEF(void)
pjsip_dlg_info_local_set_target_uri(pj_pool_t *pool,
                                    pjsip_dlg_info_local *local,
                                    const pj_str_t * target_uri)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)local, &TARGET);
    pj_xml_attr *attr;

    if (!node) {
        node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
        xml_init_node(pool, node, &TARGET, NULL);
        pj_xml_add_node(local, node);
    }
    attr = pj_xml_find_attr(node, &URI, NULL);
    if (!attr) {
        attr = xml_create_attr(pool, &URI, target_uri);
        pj_xml_add_attr(node, attr);
    } else {
        pj_strdup(pool, &attr->value, target_uri);
    }
}

/* Dialog */
PJ_DEF(void)
pjsip_dlg_info_dialog_construct(pj_pool_t *pool,
                                pjsip_dlg_info_dialog *dialog,
                                const pj_str_t *id)
{
    pj_xml_attr *attr;
    pjsip_dlg_info_local *local;
    pjsip_dlg_info_remote *remote;

    xml_init_node(pool, dialog, &DIALOG, NULL);
    attr = xml_create_attr(pool, &ID, id);
    pj_xml_add_attr(dialog, attr);
    local = PJ_POOL_ALLOC_T(pool, pjsip_dlg_info_local);
    pjsip_dlg_info_local_construct(pool, local);
    pj_xml_add_node(dialog, local);

    remote = PJ_POOL_ALLOC_T(pool, pjsip_dlg_info_remote);
    pjsip_dlg_info_remote_construct(pool, remote);
    pj_xml_add_node(dialog, remote);
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_dialog_get_id(const pjsip_dlg_info_dialog *dialog)
{
    const pj_xml_attr *attr = pj_xml_find_attr((pj_xml_node*)dialog,
                                               &ID, NULL);
    if (!attr)
        return &EMPTY_STRING;
    return &attr->value;
}

PJ_DEF(void)
pjsip_dlg_info_dialog_set_id(pj_pool_t *pool,
                             pjsip_dlg_info_dialog *dialog,
                             const pj_str_t *id)
{
    pj_xml_attr *attr = pj_xml_find_attr(dialog, &ID, NULL);
    pj_assert(attr);
    pj_strdup(pool, &attr->value, id);
}

PJ_DEF(const pj_str_t*)
pjsip_dlg_info_dialog_get_call_id(const pjsip_dlg_info_dialog *dialog)
{
    const pj_xml_attr *attr = pj_xml_find_attr((pj_xml_node*)dialog, &CALL_ID,
                                               NULL);
    if (attr)
        return &attr->value;
    return &EMPTY_STRING;
}

PJ_DEF(void)
pjsip_dlg_info_dialog_set_call_id(pj_pool_t *pool,
                                  pjsip_dlg_info_dialog *dialog,
                                  const pj_str_t *call_id)
{
    pj_xml_attr *attr = pj_xml_find_attr(dialog, &CALL_ID, NULL);
    if (!attr) {
        attr = xml_create_attr(pool, &CALL_ID, call_id);
        pj_xml_add_attr(dialog, attr);
    } else {
        pj_strdup(pool, &attr->value, call_id);
    }
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_dialog_get_remote_tag(const pjsip_dlg_info_dialog *dialog)
{
    const pj_xml_attr *attr = pj_xml_find_attr((pj_xml_node*)dialog,
                                               &REMOTE_TAG, NULL);
    if (attr)
        return &attr->value;
    return &EMPTY_STRING;
}

PJ_DEF(void)
pjsip_dlg_info_dialog_set_remote_tag(pj_pool_t *pool,
                                     pjsip_dlg_info_dialog *dialog,
                                     const pj_str_t *remote_tag)
{
    pj_xml_attr *attr = pj_xml_find_attr(dialog, &REMOTE_TAG, NULL);
    if (!attr) {
        attr = xml_create_attr(pool, &REMOTE_TAG, remote_tag);
        pj_xml_add_attr(dialog, attr);
    } else {
        pj_strdup(pool, &attr->value, remote_tag);
    }
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_dialog_get_local_tag(const pjsip_dlg_info_dialog *dialog)
{
    const pj_xml_attr *attr = pj_xml_find_attr((pj_xml_node*)dialog,
                                               &LOCAL_TAG, NULL);
    if (attr)
        return &attr->value;
    return &EMPTY_STRING;
}

PJ_DEF(void) pjsip_dlg_info_dialog_set_local_tag(pj_pool_t *pool,
                            pjsip_dlg_info_dialog *dialog,
                            const pj_str_t *local_tag)
{
    pj_xml_attr *attr = pj_xml_find_attr(dialog, &LOCAL_TAG, NULL);
    if (!attr) {
        attr = xml_create_attr(pool, &LOCAL_TAG, local_tag);
        pj_xml_add_attr(dialog, attr);
    } else {
        pj_strdup(pool, &attr->value, local_tag);
    }
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_dialog_get_direction(const pjsip_dlg_info_dialog *dialog)
{
    const pj_xml_attr *attr = pj_xml_find_attr((pj_xml_node*)dialog,
                                               &DIRECTION, NULL);
    if (attr)
        return &attr->value;
    return &EMPTY_STRING;
}

PJ_DEF(void)
pjsip_dlg_info_dialog_set_direction(pj_pool_t *pool,
                                    pjsip_dlg_info_dialog *dialog,
                                    const pj_str_t *direction)
{
    pj_xml_attr *attr = pj_xml_find_attr(dialog, &DIRECTION, NULL);
    if (!attr) {
        attr = xml_create_attr(pool, &DIRECTION, direction);
        pj_xml_add_attr(dialog, attr);
    } else {
        pj_strdup(pool, &attr->value, direction);
    }
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_dialog_get_state(pjsip_dlg_info_dialog *dialog)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)dialog, &STATE);
    if (!node)
        return &EMPTY_STRING;
    return &node->content;
}

PJ_DEF(void)
pjsip_dlg_info_dialog_set_state(pj_pool_t *pool,
                                pjsip_dlg_info_dialog *dialog,
                                const pj_str_t *state)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)dialog, &STATE);
    if (!node) {
        node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
        xml_init_node(pool, node, &STATE, state);
        pj_xml_add_node(dialog, node);
    } else {
        pj_strdup(pool, &node->content, state);
    }
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_dialog_get_duration(pjsip_dlg_info_dialog *dialog)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)dialog, &DURATION);
    if (!node)
        return &EMPTY_STRING;
    return &node->content;
}

PJ_DEF(void)
pjsip_dlg_info_dialog_set_duration(pj_pool_t *pool,
                                   pjsip_dlg_info_dialog *dialog,
                                   const pj_str_t *duration)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)dialog, &DURATION);
    if (!node) {
        node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
        xml_init_node(pool, node, &DURATION, duration);
        pj_xml_add_node(dialog, node);
    } else {
        pj_strdup(pool, &node->content, duration);
    }
}

PJ_DEF(pjsip_dlg_info_local *)
pjsip_dlg_info_dialog_get_local(pjsip_dlg_info_dialog *dialog)
{
    pjsip_dlg_info_local *local = (pjsip_dlg_info_local*)
                                  pj_xml_find_node(dialog, &LOCAL);
    if (local)
        return local;
    return NULL;
}

PJ_DEF(pjsip_dlg_info_local *)
pjsip_dlg_info_dialog_add_local(pj_pool_t *pool,
                                pjsip_dlg_info_dialog *dialog)
{
    pjsip_dlg_info_local *local = PJ_POOL_ALLOC_T(pool, pjsip_dlg_info_local);
    xml_init_node(pool, local, &LOCAL, NULL);
    pj_xml_add_node(dialog, local);
    return local;
}

PJ_DEF(pjsip_dlg_info_remote *)
pjsip_dlg_info_dialog_get_remote(pjsip_dlg_info_dialog *dialog)
{
    pjsip_dlg_info_remote *remote = (pjsip_dlg_info_remote*)
                                    pj_xml_find_node(dialog, &REMOTE);
    if (remote)
        return remote;
    return NULL;
}

PJ_DEF(pjsip_dlg_info_remote *)
pjsip_dlg_info_dialog_add_remote(pj_pool_t *pool,
                                 pjsip_dlg_info_dialog *dialog)
{
    pjsip_dlg_info_remote *remote = PJ_POOL_ALLOC_T(pool,
                                                    pjsip_dlg_info_remote);
    xml_init_node(pool, remote, &REMOTE, NULL);
    pj_xml_add_node(dialog, remote);
    return remote;
}


/* Dialog-Info */
PJ_DEF(void)
pjsip_dlg_info_dialog_info_construct(pj_pool_t *pool,
                                     pjsip_dlg_info_dialog_info *dialog_info,
                                     const pj_str_t *version,
                                     const pj_str_t *state,
                                     const pj_str_t* entity)
{
    pj_xml_attr *attr;
    pjsip_dlg_info_dialog *dialog;

    xml_init_node(pool, dialog_info, &DIALOG_INFO, NULL);
    attr = xml_create_attr(pool, &VERSION, version);
    pj_xml_add_attr(dialog_info, attr);

    attr = xml_create_attr(pool, &STATE, state);
    pj_xml_add_attr(dialog_info, attr);

    attr = xml_create_attr(pool, &ENTITY, entity);
    pj_xml_add_attr(dialog_info, attr);

    dialog = PJ_POOL_ALLOC_T(pool, pjsip_dlg_info_dialog);
    pjsip_dlg_info_local_construct(pool, dialog);
    pj_xml_add_node(dialog_info, dialog);
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_dialog_info_get_state(pjsip_dlg_info_dialog_info *dialog_info)
{
    const pj_xml_attr *attr = pj_xml_find_attr((pj_xml_node*)dialog_info,
                                               &STATE, NULL);
    if (!attr)
        return &EMPTY_STRING;
    return &attr->value;
}

PJ_DEF(void)
pjsip_dlg_info_dialog_info_set_state(pj_pool_t *pool,
                                     pjsip_dlg_info_dialog_info *dialog_info,
                                     const pj_str_t *state)
{
    pj_xml_attr *attr = pj_xml_find_attr(dialog_info, &STATE, NULL);
    pj_assert(attr);
    pj_strdup(pool, &attr->value, state);
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_dialog_info_get_version(pjsip_dlg_info_dialog_info *dialog_info)
{
    const pj_xml_attr *attr = pj_xml_find_attr((pj_xml_node*)dialog_info,
                                               &VERSION, NULL);
    if (!attr)
        return &EMPTY_STRING;
    return &attr->value;
}


PJ_DEF(void)
pjsip_dlg_info_dialog_info_set_version(pj_pool_t *pool,
                                       pjsip_dlg_info_dialog_info *dialog_info,
                                       const pj_str_t *version)
{
    pj_xml_attr *attr = pj_xml_find_attr(dialog_info, &VERSION, NULL);
    pj_assert(attr);
    pj_strdup(pool, &attr->value, version);
}

PJ_DEF(const pj_str_t *)
pjsip_dlg_info_dialog_info_get_entity(pjsip_dlg_info_dialog_info *dialog_info)
{
    const pj_xml_attr *attr = pj_xml_find_attr((pj_xml_node*)dialog_info,
                                               &ENTITY, NULL);
    if (!attr)
        return &EMPTY_STRING;
    return &attr->value;
}

PJ_DEF(void)
pjsip_dlg_info_dialog_info_set_entity(pj_pool_t *pool,
                                      pjsip_dlg_info_dialog_info *dialog_info,
                                      const pj_str_t *entity)
{
    pj_xml_attr *attr = pj_xml_find_attr(dialog_info, &ENTITY, NULL);
    pj_assert(attr);
    pj_strdup(pool, &attr->value, entity);
}

PJ_DEF(pjsip_dlg_info_dialog *)
pjsip_dlg_info_dialog_info_get_dialog(pjsip_dlg_info_dialog_info *dialog_info)
{
    pjsip_dlg_info_dialog *dialog = (pjsip_dlg_info_dialog*)
                                    pj_xml_find_node(dialog_info, &DIALOG);
    return dialog;
}

PJ_DEF(pjsip_dlg_info_dialog *)
pjsip_dlg_info_dialog_info_add_dialog(pj_pool_t *pool,
                                      pjsip_dlg_info_dialog_info *dialog_info,
                                      const pj_str_t *id)
{
    pjsip_dlg_info_dialog *dialog = PJ_POOL_ALLOC_T(pool,
                                                    pjsip_dlg_info_dialog);
    pjsip_dlg_info_dialog_construct(pool, dialog, id);
    pj_xml_add_node(dialog_info, dialog);
    return dialog;
}

/* Dialog-Info document */

PJ_DEF(pjsip_dlg_info_dialog *)
pjsip_dlg_info_create(pj_pool_t *pool, const pj_str_t *version,
                      const pj_str_t *state, const pj_str_t *entity)
{
    pjsip_dlg_info_dialog *dialog_info;

    dialog_info = PJ_POOL_ALLOC_T(pool, pjsip_dlg_info_dialog);
    pjsip_dlg_info_dialog_info_construct(pool, dialog_info, version,
                                         state, entity);
    return dialog_info;
}

PJ_DEF(pjsip_dlg_info_dialog *) pjsip_dlg_info_parse(pj_pool_t *pool,
                                                     char *text,
                                                     int len)
{
    pjsip_dlg_info_dialog *dialog_info = pj_xml_parse(pool, text, len);
    if (dialog_info) {
        if (pj_stricmp(&dialog_info->name, &DIALOG_INFO) == 0)
            return dialog_info;
    }
    return NULL;
}

PJ_DEF(int) pjsip_dlg_info_print(const pjsip_dlg_info_dialog *dialog_info,
                                 char *buf, int len)
{
    return pj_xml_print(dialog_info, buf, len, PJ_TRUE);
}
