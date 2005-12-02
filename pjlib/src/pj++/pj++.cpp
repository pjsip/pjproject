/* $Header: /pjproject/pjlib/src/pj++/pj++.cpp 4     4/17/05 11:59a Bennylp $ */
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
#include <pj++/scanner.hpp>
#include <pj++/timer.hpp>
#include <pj/except.h>

void PJ_Scanner::syntax_error_handler_throw_pj(pj_scanner *)
{
    PJ_THROW( PJ_Scanner::SYNTAX_ERROR );
}

void PJ_Timer_Entry::timer_heap_callback(pj_timer_heap_t *, pj_timer_entry *e)
{
    PJ_Timer_Entry *entry = static_cast<PJ_Timer_Entry*>(e);
    entry->on_timeout();
}
