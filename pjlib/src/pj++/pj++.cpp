/* $Id$
 *
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
