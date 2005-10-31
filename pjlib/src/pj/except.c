/* $Header: /pjproject-0.3/pjlib/src/pj/except.c 6     10/14/05 12:26a Bennylp $ */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pj/except.c $
 * 
 * 6     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 5     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 4     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#include <pj/except.h>
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/errno.h>

static long thread_local_id = -1;

#if defined(PJ_HAS_EXCEPTION_NAMES) && PJ_HAS_EXCEPTION_NAMES != 0
    static const char *exception_id_names[PJ_MAX_EXCEPTION_ID];
#else
    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    static int last_exception_id = 1;
#endif  /* PJ_HAS_EXCEPTION_NAMES */


PJ_DEF(void) pj_throw_exception_(int exception_id)
{
    struct pj_exception_state_t *handler;

    handler = pj_thread_local_get(thread_local_id);
    if (handler == NULL) {
        PJ_LOG(1,("except.c", "!!!FATAL: unhandled exception %d!\n", exception_id));
        pj_assert(handler != NULL);
        /* This will crash the system! */
    }
    pj_longjmp(handler->state, exception_id);
}

PJ_DEF(void) pj_push_exception_handler_(struct pj_exception_state_t *rec)
{
    struct pj_exception_state_t *parent_handler = NULL;

    if (thread_local_id == -1) {
	pj_thread_local_alloc(&thread_local_id);
	pj_assert(thread_local_id != -1);
    }
    parent_handler = pj_thread_local_get(thread_local_id);
    rec->prev = parent_handler;
    pj_thread_local_set(thread_local_id, rec);
}

PJ_DEF(void) pj_pop_exception_handler_(void)
{
    struct pj_exception_state_t *handler;

    handler = pj_thread_local_get(thread_local_id);
    pj_assert(handler != NULL);
    pj_thread_local_set(thread_local_id, handler->prev);
}

#if defined(PJ_HAS_EXCEPTION_NAMES) && PJ_HAS_EXCEPTION_NAMES != 0
PJ_DEF(pj_status_t) pj_exception_id_alloc( const char *name,
                                           pj_exception_id_t *id)
{
    unsigned i;

    pj_enter_critical_section();

    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    for (i=1; i<PJ_MAX_EXCEPTION_ID; ++i) {
        if (exception_id_names[i] == NULL) {
            exception_id_names[i] = name;
            *id = i;
            pj_leave_critical_section();
            return PJ_SUCCESS;
        }
    }

    pj_leave_critical_section();
    return PJ_ETOOMANY;
}

PJ_DEF(pj_status_t) pj_exception_id_free( pj_exception_id_t id )
{
    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    PJ_ASSERT_RETURN(id>0 && id<PJ_MAX_EXCEPTION_ID, PJ_EINVAL);
    
    pj_enter_critical_section();
    exception_id_names[id] = NULL;
    pj_leave_critical_section();

    return PJ_SUCCESS;

}

PJ_DEF(const char*) pj_exception_id_name(pj_exception_id_t id)
{
    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    PJ_ASSERT_RETURN(id>0 && id<PJ_MAX_EXCEPTION_ID, "<Invalid ID>");

    if (exception_id_names[id] == NULL)
        return "<Unallocated ID>";

    return exception_id_names[id];
}

#else   /* PJ_HAS_EXCEPTION_NAMES */
PJ_DEF(pj_status_t) pj_exception_id_alloc( const char *name,
                                           pj_exception_id_t *id)
{
    PJ_ASSERT_RETURN(last_exception_id < PJ_MAX_EXCEPTION_ID-1, PJ_ETOOMANY);

    *id = last_exception_id++
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_exception_id_free( pj_exception_id_t id )
{
    return PJ_SUCCESS;
}

PJ_DEF(const char*) pj_exception_id_name(pj_exception_id_t id)
{
    return "";
}

#endif  /* PJ_HAS_EXCEPTION_NAMES */



