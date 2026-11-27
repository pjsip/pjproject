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
#ifndef __PJLIB_UTIL_RESOLVER_INTERNAL_H__
#define __PJLIB_UTIL_RESOLVER_INTERNAL_H__

/* Private API shared by the DNS resolver modules, not for application use. */

#include <pjlib-util/resolver.h>
#include <pj/lock.h>

PJ_BEGIN_DECL

/* Parameters of pj_dns_resolver_cancel_query2(), to be zeroed before use. */
typedef struct pj_dns_cancel_query_param
{
    /* Input: if non-zero, the callback is called with a failure status,
     * unless it has been called or taken to be called already.
     */
    pj_bool_t   notify;

    /* Output: PJ_TRUE if the callback has been taken to be called, e.g.
     * another thread is processing the answer or the timeout of the query.
     * The callback may be running, also in this thread, or may still be
     * called once. PJ_FALSE means that the callback will not be called
     * after the function returns.
     */
    pj_bool_t   cb_busy;

} pj_dns_cancel_query_param;

/* Cancel a pending query like pj_dns_resolver_cancel_query(), and tell
 * whether the callback of the query can still be called afterwards.
 */
PJ_DECL(pj_status_t) pj_dns_resolver_cancel_query2(
                                        pj_dns_async_query *query,
                                        pj_dns_cancel_query_param *prm);

/* Get the group lock of the resolver. The memory of the resolver, including
 * its pending queries, is kept until the last reference of this lock is
 * released, also after pj_dns_resolver_destroy().
 */
PJ_DECL(pj_grp_lock_t*) pj_dns_resolver_get_grp_lock(
                                                pj_dns_resolver *resolver);

PJ_END_DECL

#endif  /* __PJLIB_UTIL_RESOLVER_INTERNAL_H__ */
