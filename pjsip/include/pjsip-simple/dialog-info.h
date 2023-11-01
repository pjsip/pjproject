/*
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
#ifndef __PJSIP_SIMPLE_DIALOG_INFO_H__
#define __PJSIP_SIMPLE_DIALOG_INFO_H__

/**
 * @file dialog-info.h
 * @brief Dialog-Info Information Data Format (RFC 4235)
 */
#include <pjsip-simple/types.h>
#include <pjlib-util/xml.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_SIMPLE_DIALOG_INFO Dialog-Info/Blf Information Data Format (RFC 4235)
 * @ingroup PJSIP_SIMPLE
 * @brief Support for Dialog-Info/Blf Information Data Format (RFC 4235)
 * @{
 *
 * This file provides tools for manipulating Dialog-Info Information Data
 * Format (BLF) as described in RFC 4235.
 */
typedef struct pj_xml_node pjdialog_info_dialog_info;
typedef struct pj_xml_node pjdialog_info_dialog;
typedef struct pj_xml_node pjdialog_info_local;
typedef struct pj_xml_node pjdialog_info_remote;


typedef struct pjdialog_info_local_op
{
    void (*construct)(pj_pool_t*, pjdialog_info_local*);

    const pj_str_t* (*get_identity)(const pjdialog_info_local*);
    void (*set_identity)(pj_pool_t *pool, const pjdialog_info_local*, const pj_str_t*);

    const pj_str_t* (*get_identity_display)(const pjdialog_info_local*);
    void (*set_identity_display)(pj_pool_t *pool, const pjdialog_info_local*, const pj_str_t*);

    const pj_str_t* (*get_target_uri)(const pjdialog_info_local*);
    void (*set_target_uri)(pj_pool_t *pool, const pjdialog_info_local*, const pj_str_t*);
} pjdialog_info_local_op;

typedef struct pjdialog_info_remote_op
{
    void (*construct)(pj_pool_t*, pjdialog_info_remote*);

    const pj_str_t* (*get_identity)(const pjdialog_info_remote*);
    void (*set_identity)(pj_pool_t *pool, const pjdialog_info_remote*, const pj_str_t*);

    const pj_str_t* (*get_identity_display)(const pjdialog_info_remote*);
    void (*set_identity_display)(pj_pool_t *pool, const pjdialog_info_remote*, const pj_str_t*);

    const pj_str_t* (*get_target_uri)(const pjdialog_info_remote*);
    void (*set_target_uri)(pj_pool_t *pool, const pjdialog_info_remote*, const pj_str_t*);
} pjdialog_info_remote_op;

typedef struct pjdialog_info_dialog_op
{
    void (*construct)(pj_pool_t*, pjdialog_info_dialog*, const pj_str_t*);

    const pj_str_t* (*get_id)(const pjdialog_info_dialog*);
    void (*set_id)(pj_pool_t *pool, const pjdialog_info_dialog*, const pj_str_t*);

    const pj_str_t* (*get_call_id)(const pjdialog_info_dialog*);
    void (*set_call_id)(pj_pool_t *pool, const pjdialog_info_dialog*, const pj_str_t*);

    const pj_str_t* (*get_remote_tag)(const pjdialog_info_dialog*);
    void (*set_remote_tag)(pj_pool_t *pool, const pjdialog_info_dialog*, const pj_str_t*);

    const pj_str_t* (*get_local_tag)(const pjdialog_info_dialog*);
    void (*set_local_tag)(pj_pool_t *pool, const pjdialog_info_dialog*, const pj_str_t*);

    const pj_str_t* (*get_direction)(const pjdialog_info_dialog*);
    void (*set_direction)(pj_pool_t *pool, const pjdialog_info_dialog*, const pj_str_t*);

    const pj_str_t* (*get_state)(const pjdialog_info_dialog*);
    void (*set_state)(pj_pool_t *pool, const pjdialog_info_dialog*, const pj_str_t*);

    const pj_str_t* (*get_duration)(const pjdialog_info_dialog*);
    void (*set_duration)(pj_pool_t *pool, const pjdialog_info_dialog*, const pj_str_t*);

    pjdialog_info_local* (*get_local)(const pjdialog_info_dialog*);
    pjdialog_info_local* (*set_local)(pj_pool_t *pool, const pjdialog_info_dialog*, const pjdialog_info_local*);

    pjdialog_info_remote* (*get_remote)(const pjdialog_info_dialog*);
    pjdialog_info_remote* (*set_remote)(pj_pool_t *pool, const pjdialog_info_dialog*, const pjdialog_info_remote*);
} pjdialog_info_dialog_op;

typedef struct pjdialog_info_dialog_info_op
{
    void (*construct)(pj_pool_t*, pjdialog_info_dialog_info*, const pj_str_t*, const pj_str_t*, const pj_str_t*);

    const pj_str_t* (*get_state)(const pjdialog_info_dialog_info*);
    void (*set_state)(pj_pool_t *pool, const pjdialog_info_dialog_info*, const pj_str_t*);

    const pj_str_t* (*get_version)(const pjdialog_info_dialog_info*);
    void (*set_version)(pj_pool_t *pool, const pjdialog_info_dialog_info*, const pj_str_t*);

    const pj_str_t* (*get_entity)(const pjdialog_info_dialog_info*);
    void (*set_entity)(pj_pool_t *pool, const pjdialog_info_dialog_info*, const pj_str_t*);

    pjdialog_info_dialog* (*get_dialog)(const pjdialog_info_dialog_info*);
    void (*set_dialog)(pj_pool_t *pool, const pjdialog_info_dialog_info*, const pjdialog_info_dialog*);
} pjdialog_info_dialog_info_op;


extern struct pjdialog_info_op_desc
{
    pjdialog_info_dialog_info  dialog_info;
    pjdialog_info_dialog dialog;
    pjdialog_info_local  local;
    pjdialog_info_remote remote;
} pjdialog_info_op;

/******************************************************************************
 * Top level API for managing dialog-info document.
 *****************************************************************************/
PJ_DECL(pjdialog_info_dialog_info*)    pjdialog_info_create(pj_pool_t *pool, const pj_str_t *version, const pj_str_t *state, const pj_str_t* entity);
PJ_DECL(pjdialog_info_dialog_info*)    pjdialog_info_parse(pj_pool_t *pool, char *text, int len);
PJ_DECL(int) pjdialog_info_print(const pjdialog_info_dialog_info* dialog_info, char *buf, int len);

/******************************************************************************
 * API for managing Dialog-info node.
 *****************************************************************************/
PJ_DECL(void)   pjdialog_info_dialog_info_construct(pj_pool_t *pool,
                            pjdialog_info_dialog_info *dialog_info,
                            const pj_str_t *version,
                            const pj_str_t *state,
                            const pj_str_t* entity);

PJ_DECL(const pj_str_t*)   pjdialog_info_dialog_info_get_state(pjdialog_info_dialog_info *dialog_info);
PJ_DECL(void)   pjdialog_info_dialog_info_set_state(pj_pool_t *pool,
                            pjdialog_info_dialog_info *dialog_info,
                            const pj_str_t *state);

PJ_DECL(const pj_str_t*)   pjdialog_info_dialog_info_get_version(pjdialog_info_dialog_info *dialog_info);
PJ_DECL(void)   pjdialog_info_dialog_info_set_version(pj_pool_t *pool,
                            pjdialog_info_dialog_info *dialog_info,
                            const pj_str_t *version);

PJ_DECL(const pj_str_t*)   pjdialog_info_dialog_info_get_entity(pjdialog_info_dialog_info *dialog_info);
PJ_DECL(void)   pjdialog_info_dialog_info_set_entity(pj_pool_t *pool,
                            pjdialog_info_dialog_info *dialog_info,
                            const pj_str_t *entity);


PJ_DECL(pjdialog_info_dialog*) pjdialog_info_dialog_info_add_dialog(pj_pool_t *pool,
                            pjdialog_info_dialog_info *dialog_info,
                            const pj_str_t *id);
PJ_DECL(pjdialog_info_dialog*) pjdialog_info_dialog_info_get_dialog(pjdialog_info_dialog_info *dialog_info);

/******************************************************************************
 * API for managing Dialog node.
 *****************************************************************************/
PJ_DECL(void)        pjdialog_info_dialog_construct(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog,
                            const pj_str_t *id);

PJ_DECL(const pj_str_t*) pjdialog_info_dialog_get_id(const pjdialog_info_dialog *dialog);
PJ_DECL(void)        pjdialog_info_dialog_set_id(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog,
                            const pj_str_t *id);

PJ_DECL(const pj_str_t*) pjdialog_info_dialog_get_call_id(const pjdialog_info_dialog *dialog);
PJ_DECL(void)        pjdialog_info_dialog_set_call_id(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog,
                            const pj_str_t *call_id);

PJ_DECL(const pj_str_t*) pjdialog_info_dialog_get_remote_tag(const pjdialog_info_dialog *dialog);
PJ_DECL(void)        pjdialog_info_dialog_set_remote_tag(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog,
                            const pj_str_t *remote_tag);

PJ_DECL(const pj_str_t*) pjdialog_info_dialog_get_local_tag(const pjdialog_info_dialog *dialog);
PJ_DECL(void)        pjdialog_info_dialog_set_local_tag(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog,
                            const pj_str_t *local_tag);

PJ_DECL(const pj_str_t*) pjdialog_info_dialog_get_direction(const pjdialog_info_dialog *dialog);
PJ_DECL(void)        pjdialog_info_dialog_set_direction(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog,
                            const pj_str_t *direction);

PJ_DECL(const pj_str_t*)  pjdialog_info_dialog_get_state(pjdialog_info_dialog *dialog);
PJ_DECL(void)        pjdialog_info_dialog_set_state(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog,
                            const pj_str_t *state);

PJ_DECL(const pj_str_t*)  pjdialog_info_dialog_get_duration(pjdialog_info_dialog *dialog);
PJ_DECL(void)        pjdialog_info_dialog_set_duration(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog,
                            const pj_str_t *duration);

PJ_DECL(pjdialog_info_local*)  pjdialog_info_dialog_get_local(pjdialog_info_dialog *dialog);
PJ_DECL(pjdialog_info_local*)  pjdialog_info_dialog_add_local(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog);

PJ_DECL(pjdialog_info_remote*)  pjdialog_info_dialog_get_remote(pjdialog_info_dialog *dialog);
PJ_DECL(pjdialog_info_remote*)  pjdialog_info_dialog_add_remote(pj_pool_t *pool,
                            pjdialog_info_dialog *dialog);

/******************************************************************************
 * API for managing Local node.
 *****************************************************************************/
PJ_DECL(void)        pjdialog_info_local_construct(pj_pool_t *pool,
                                pjdialog_info_local *local);

PJ_DECL(const pj_str_t *) pjdialog_info_local_get_identity(const pjdialog_info_local *local);
PJ_DECL(void)        pjdialog_info_local_add_identity(pj_pool_t *pool,
                            pjdialog_info_local *local,
                            const pj_str_t *identity);

PJ_DECL(const pj_str_t *) pjdialog_info_local_get_identity_display(const pjdialog_info_local *local);
PJ_DECL(void)        pjdialog_info_local_set_identity_display(pj_pool_t *pool,
                            pjdialog_info_local *local,
                            const pj_str_t *identity_display);

PJ_DECL(const pj_str_t *) pjdialog_info_local_get_target_uri(const pjdialog_info_local *local);
PJ_DECL(void)        pjdialog_info_local_set_target_uri(pj_pool_t *pool,
                            pjdialog_info_local *local,
                            const pj_str_t * target_uri);

/******************************************************************************
 * API for managing Remote node.
 *****************************************************************************/
PJ_DECL(void)        pjdialog_info_remote_construct(pj_pool_t *pool,
                                pjdialog_info_remote *remote);

PJ_DECL(const pj_str_t *) pjdialog_info_remote_get_identity(const pjdialog_info_remote *remote);
PJ_DECL(void)        pjdialog_info_remote_add_identity(pj_pool_t *pool,
                            pjdialog_info_remote *remote,
                            const pj_str_t *identity);

PJ_DECL(const pj_str_t *) pjdialog_info_remote_get_identity_display(const pjdialog_info_remote *remote);
PJ_DECL(void)        pjdialog_info_remote_set_identity_display(pj_pool_t *pool,
                            pjdialog_info_remote *remote,
                            const pj_str_t *identity_display);

PJ_DECL(const pj_str_t *) pjdialog_info_remote_get_target_uri(const pjdialog_info_remote *remote);
PJ_DECL(void)        pjdialog_info_remote_set_target_uri(pj_pool_t *pool,
                            pjdialog_info_remote *remote,
                            const pj_str_t * target_uri);

PJ_END_DECL


#endif  /* __PJSIP_SIMPLE_DIALOG_INFO_H__ */
