/* $Header: /pjproject-0.3/pjlib/src/pjlib-samples/except.c 2     10/14/05 12:26a Bennylp $ */
/*
 * $Log: /pjproject-0.3/pjlib/src/pjlib-samples/except.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     10/10/05 3:16p Bennylp
 * Created.
 *
 */
#include <pj/except.h>
#include <pj/rand.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * \page page_pjlib_samples_except_c Example: Exception Handling
 *
 * Below is sample program to demonstrate how to use exception handling.
 *
 * \includelineno pjlib-samples/except.c
 */

static pj_exception_id_t NO_MEMORY, OTHER_EXCEPTION;

static void randomly_throw_exception()
{
    if (pj_rand() % 2)
        PJ_THROW(OTHER_EXCEPTION);
}

static void *my_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
        PJ_THROW(NO_MEMORY);
    return ptr;
}

static int test_exception()
{
    PJ_USE_EXCEPTION;
    
    PJ_TRY {
        void *data = my_malloc(200);
        free(data);
        randomly_throw_exception();
    }
    PJ_CATCH( NO_MEMORY ) {
        puts("Can't allocate memory");
        return 0;
    }
    PJ_DEFAULT {
        pj_exception_id_t x_id;
        
        x_id = PJ_GET_EXCEPTION();
        printf("Caught exception %d (%s)\n", 
            x_id, pj_exception_id_name(x_id));
    }
    PJ_END
        return 1;
}

int main()
{
    pj_status_t rc;
    
    // Error handling is omited for clarity.
    
    rc = pj_init();

    rc = pj_exception_id_alloc("No Memory", &NO_MEMORY);
    rc = pj_exception_id_alloc("Other Exception", &OTHER_EXCEPTION);
    
    return test_exception();
}

