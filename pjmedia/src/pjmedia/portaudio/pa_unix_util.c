/*
 * $Id: pa_unix_util.c,v 1.1.2.7 2005/03/31 15:02:48 aknudsen Exp $
 * Portable Audio I/O Library
 * UNIX platform-specific support functions
 *
 * Based on the Open Source API proposed by Ross Bencina
 * Copyright (c) 1999-2000 Ross Bencina
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

 
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h> /* For memset */

#include "pa_util.h"
#include "pa_unix_util.h"

/*
   Track memory allocations to avoid leaks.
 */

#if PA_TRACK_MEMORY
static int numAllocations_ = 0;
#endif


void *PaUtil_AllocateMemory( long size )
{
    void *result = malloc( size );

#if PA_TRACK_MEMORY
    if( result != NULL ) numAllocations_ += 1;
#endif
    return result;
}


void PaUtil_FreeMemory( void *block )
{
    if( block != NULL )
    {
        free( block );
#if PA_TRACK_MEMORY
        numAllocations_ -= 1;
#endif

    }
}


int PaUtil_CountCurrentlyAllocatedBlocks( void )
{
#if PA_TRACK_MEMORY
    return numAllocations_;
#else
    return 0;
#endif
}


void Pa_Sleep( long msec )
{
    while( msec > 999 )     /* For OpenBSD and IRIX, argument */
        {                   /* to usleep must be < 1000000.   */
        usleep( 999000 );
        msec -= 999;
        }
    usleep( msec * 1000 );
}

/*            *** NOT USED YET: ***
static int usePerformanceCounter_;
static double microsecondsPerTick_;
*/

void PaUtil_InitializeClock( void )
{
    /* TODO */
}


PaTime PaUtil_GetTime( void )
{
    struct timeval tv;
    gettimeofday( &tv, NULL );
    return (PaTime) tv.tv_usec / 1000000. + tv.tv_sec;
}

PaError PaUtil_InitializeThreading( PaUtilThreading *threading )
{
    (void) paUtilErr_;
    return paNoError;
}

void PaUtil_TerminateThreading( PaUtilThreading *threading )
{
}

PaError PaUtil_StartThreading( PaUtilThreading *threading, void *(*threadRoutine)(void *), void *data )
{
    pthread_create( &threading->callbackThread, NULL, threadRoutine, data );
    return paNoError;
}

PaError PaUtil_CancelThreading( PaUtilThreading *threading, int wait, PaError *exitResult )
{
    PaError result = paNoError;
    void *pret;

    if( exitResult )
        *exitResult = paNoError;

    /* Only kill the thread if it isn't in the process of stopping (flushing adaptation buffers) */
    if( !wait )
        pthread_cancel( threading->callbackThread );   /* XXX: Safe to call this if the thread has exited on its own? */
    pthread_join( threading->callbackThread, &pret );

#ifdef PTHREAD_CANCELED
    if( pret && PTHREAD_CANCELED != pret )
#else
    /* !wait means the thread may have been canceled */
    if( pret && wait )
#endif
    {
        if( exitResult )
            *exitResult = *(PaError *) pret;
        free( pret );
    }

    return result;
}

/*
static void *CanaryFunc( void *userData )
{
    const unsigned intervalMsec = 1000;
    PaUtilThreading *th = (PaUtilThreading *) userData;

    while( 1 )
    {
        th->canaryTime = PaUtil_GetTime();

        pthread_testcancel();
        Pa_Sleep( intervalMsec );
    }

    pthread_exit( NULL );
}
*/
