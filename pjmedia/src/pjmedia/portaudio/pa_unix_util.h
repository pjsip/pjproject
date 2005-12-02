/* 
 * PJMEDIA - Multimedia over IP Stack 
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
#ifndef PA_UNIX_UTIL_H
#define PA_UNIX_UTIL_H

#include "pa_cpuload.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define PA_MIN(x,y) ( (x) < (y) ? (x) : (y) )
#define PA_MAX(x,y) ( (x) > (y) ? (x) : (y) )

/* Utilize GCC branch prediction for error tests */
#if defined __GNUC__ && __GNUC__ >= 3
#define UNLIKELY(expr) __builtin_expect( (expr), 0 )
#else
#define UNLIKELY(expr) (expr)
#endif

#define STRINGIZE_HELPER(expr) #expr
#define STRINGIZE(expr) STRINGIZE_HELPER(expr)

#define PA_UNLESS(expr, code) \
    do { \
        if( UNLIKELY( (expr) == 0 ) ) \
        { \
            PaUtil_DebugPrint(( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" )); \
            result = (code); \
            goto error; \
        } \
    } while (0);

static PaError paUtilErr_;          /* Used with PA_ENSURE */

/* Check PaError */
#define PA_ENSURE(expr) \
    do { \
        if( UNLIKELY( (paUtilErr_ = (expr)) < paNoError ) ) \
        { \
            PaUtil_DebugPrint(( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" )); \
            result = paUtilErr_; \
            goto error; \
        } \
    } while (0);

typedef struct {
    pthread_t callbackThread;
} PaUtilThreading;

PaError PaUtil_InitializeThreading( PaUtilThreading *threading );
void PaUtil_TerminateThreading( PaUtilThreading *threading );
PaError PaUtil_StartThreading( PaUtilThreading *threading, void *(*threadRoutine)(void *), void *data );
PaError PaUtil_CancelThreading( PaUtilThreading *threading, int wait, PaError *exitResult );

/* State accessed by utility functions */

/*
void PaUnix_SetRealtimeScheduling( int rt );

void PaUtil_InitializeThreading( PaUtilThreading *th, PaUtilCpuLoadMeasurer *clm );

PaError PaUtil_CreateCallbackThread( PaUtilThreading *th, void *(*CallbackThreadFunc)( void * ), PaStream *s );

PaError PaUtil_KillCallbackThread( PaUtilThreading *th, PaError *exitResult );

void PaUtil_CallbackUpdate( PaUtilThreading *th );
*/

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
