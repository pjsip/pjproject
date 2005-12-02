/* $Header: /pjproject/pjlib/src/pj/except.c 2     2/24/05 10:34a Bennylp $ */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pj/except.h>
#include <pj/os.h>
#include <assert.h>

static int thread_local_id = -1;

PJ_DEF(void) pj_throw_exception_(int exception_id)
{
    struct pj_exception_state_t *handler;

    handler = pj_thread_local_get(thread_local_id);
    assert(handler != NULL);
    longjmp(handler->state, exception_id);
}

PJ_DEF(void) pj_push_exception_handler_(struct pj_exception_state_t *rec)
{
    struct pj_exception_state_t *parent_handler = NULL;

    if (thread_local_id == -1) {
	thread_local_id = pj_thread_local_alloc();
	assert(thread_local_id != -1);
    }
    parent_handler = pj_thread_local_get(thread_local_id);
    rec->prev = parent_handler;
    pj_thread_local_set(thread_local_id, rec);
}

PJ_DEF(void) pj_pop_exception_handler_()
{
    struct pj_exception_state_t *handler;

    handler = pj_thread_local_get(thread_local_id);
    assert(handler != NULL);
    pj_thread_local_set(thread_local_id, handler->prev);
}

