/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/os_win32.c,v 1.1 2005/12/02 20:02:30 nn Exp $ */
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
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <stddef.h>
#ifndef PJ_WIN32_WINCE
#  include <sys/timeb.h>
#else

#  include <windows.h>

struct timeb {
	time_t time;
	unsigned short millitm;
};

static void ftime( struct timeb *tb )
{
	SYSTEMTIME st;
	int days, years, leapyears;
	
	if(tb == NULL)
	{
		//nlSetError(NL_NULL_POINTER);
		assert(tb);
		return;
	}
	GetSystemTime(&st);
	leapyears = (st.wYear - 1970 + 1) / 4;
	years = st.wYear - 1970 - leapyears;
	
	days = years * 365 + leapyears * 366;
	
	switch (st.wMonth) {
	case 1:
	case 3:
	case 5:
	case 7:
	case 8:
	case 10:
	case 12:
		days += 31;
		break;
	case 4:
	case 6:
	case 9:
	case 11:
		days += 30;
		break;
	case 2:
		days += (st.wYear%4 == 0) ? 29 : 28;
		break;
	default:
		break;
	}
	days += st.wDay;
	tb->time = days * 86400 + st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
	tb->millitm = st.wMilliseconds;
}

time_t time(time_t *t)
{
	struct timeb tb;
	
	ftime(&tb);
	*t = tb.time;
	
	return *t;
} 
#endif

#include <time.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT	PJ_WIN32_WINNT
#include <winsock.h>

struct pj_thread_t
{
    char	    obj_name[PJ_MAX_OBJ_NAME];
    HANDLE	    hthread;
    DWORD	    idthread;
    pj_thread_proc *proc;
    void	   *arg;
};


struct pj_mutex_t
{
#if PJ_WIN32_WINNT >= 0x0400
    CRITICAL_SECTION	crit;
#else
    HANDLE		hMutex;
#endif
    char		obj_name[PJ_MAX_OBJ_NAME];
#if PJ_DEBUG
    int		        nesting_level;
    pj_thread_t	       *owner;
#endif
};


typedef struct pj_sem_t
{
    HANDLE		hSemaphore;
    char		obj_name[PJ_MAX_OBJ_NAME];
} pj_mem_t;

struct pj_event_t
{
    HANDLE		hEvent;
    char		obj_name[PJ_MAX_OBJ_NAME];
};

static pj_thread_desc main_thread;
static int thread_tls_id;
static pj_mutex_t critical_section_mutex;

static int init_mutex(pj_mutex_t *mutex, const char *name);

PJ_DEF(pj_status_t) pj_init(void)
{
    WSADATA wsa;
    char dummy_guid[32]; /* use maximum GUID length */
    pj_str_t guid;

    PJ_LOG(5, ("pj_init", "Initializing PJ Library.."));

    /* Init Winsock.. */
    if (WSAStartup(MAKEWORD(2,0), &wsa) != 0) {
	PJ_LOG(1, ("pj_init", "Winsock initialization has returned an error"));
	return -1;
    }

    /* Init this thread's TLS. */
    if (pj_thread_init() != 0) {
	PJ_LOG(1, ("pj_init", "Thread initialization has returned an error"));
	return -1;
    }
    
    /* Init random seed. */
    srand( GetCurrentProcessId() );

    /* Startup GUID. */
    guid.ptr = dummy_guid;
    pj_generate_unique_string( &guid );

    /* Initialize critical section. */
    if (init_mutex(&critical_section_mutex, "pj%p") != 0)
	return -1;

    return PJ_OK;
}

PJ_DEF(pj_uint32_t) pj_getpid(void)
{
    return GetCurrentProcessId();
}

PJ_DEF(void) pj_perror(const char *src, const char *format, ...)
{
    char msg[256];
    int len;
    va_list marker;

    va_start(marker, format);
    len = _vsnprintf(msg, sizeof(msg), format, marker);
    va_end(marker);

    msg[len++] = ':';
    msg[len++] = ' ';
    len += sprintf( &msg[len], "error %u", (unsigned)GetLastError());
    msg[len] = '\0';

    PJ_LOG(1, (src, "%s", msg));
}

PJ_DEF(pj_status_t) pj_getlasterror(void)
{
    return (pj_status_t) GetLastError();
}

PJ_DEF(pj_thread_t*) pj_thread_register (const char *cstr_thread_name,
					 pj_thread_desc desc)
{
    pj_thread_t *thread = (pj_thread_t *)desc;
    pj_str_t thread_name = pj_str((char*)cstr_thread_name);

    /* Size sanity check. */
    if (sizeof(pj_thread_desc) < sizeof(pj_thread_t)) {
	pj_assert(!"Not enough pj_thread_desc size!");
	return NULL;
    }

    /* If a thread descriptor has been registered before, just return it. */
    if (pj_thread_local_get (thread_tls_id) != 0) {
	return (pj_thread_t*)pj_thread_local_get (thread_tls_id);
    }

    /* Initialize and set the thread entry. */
    pj_memset(desc, 0, sizeof(pj_thread_desc));
    thread->hthread = GetCurrentThread();
    thread->idthread = GetCurrentThreadId();

    if (cstr_thread_name && pj_strlen(&thread_name) < sizeof(thread->obj_name)-1)
	sprintf(thread->obj_name, cstr_thread_name, thread->idthread);
    else
	sprintf(thread->obj_name, "thr%p", (void*)thread->idthread);
    
    pj_thread_local_set(thread_tls_id, thread);

    return thread;
}

pj_status_t pj_thread_init(void)
{
    thread_tls_id = pj_thread_local_alloc();
    if (thread_tls_id == -1)
	return -1;

    if (pj_thread_register("thr%p", main_thread) == NULL)
	return -1;

    return PJ_OK;
}

static DWORD WINAPI thread_main(void *param)
{
    pj_thread_t *rec = param;
    void *result;

    PJ_LOG(6,(rec->obj_name, "Thread started"));

    pj_thread_local_set(thread_tls_id, rec);
    result = (*rec->proc)(rec->arg);

    PJ_LOG(6,(rec->obj_name, "Thread quitting"));
    return (DWORD)result;
}

PJ_DEF(pj_thread_t*) pj_thread_create(pj_pool_t *pool, const char *thread_name,
				      pj_thread_proc *proc, void *arg,
				      pj_size_t stack_size, void *stack, 
				      unsigned flags)
{
    DWORD dwflags = 0;
    pj_thread_t *rec;

    /* Stack argument must be NULL on WIN32 */
    if (stack != NULL)
	return NULL;

    /* Set flags */
    if (flags & PJ_THREAD_SUSPENDED)
	dwflags |= CREATE_SUSPENDED;

    /* Create thread record and assign name for the thread */
    rec = (struct pj_thread_t*) pj_pool_calloc(pool, 1, sizeof(pj_thread_t));
    if (!rec) {
	return NULL;
    }
    /* Set name. */
    if (!thread_name) {
	thread_name = "thr%p";
    }
    if (strchr(thread_name, '%')) {
	pj_snprintf(rec->obj_name, PJ_MAX_OBJ_NAME, thread_name, rec);
    } else {
	strncpy(rec->obj_name, thread_name, PJ_MAX_OBJ_NAME);
	rec->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    }

    PJ_LOG(6, (rec->obj_name, "Thread created"));

    /* Create the thread. */
    rec->proc = proc;
    rec->arg = arg;
    rec->hthread = CreateThread(NULL, stack_size, 
				thread_main, rec,
				dwflags, &rec->idthread);
    if (rec->hthread == NULL) {
	return NULL;
    }
    return rec;
}

PJ_DEF(const char*) pj_thread_get_name(pj_thread_t *p)
{
    pj_thread_t *rec = (pj_thread_t*)p;
    return rec->obj_name;
}

PJ_DEF(pj_status_t) pj_thread_resume(pj_thread_t *p)
{
    pj_thread_t *rec = (pj_thread_t*)p;
    return ResumeThread(rec->hthread) == -1 ? -1 : PJ_OK;
}

PJ_DEF(pj_thread_t*) pj_thread_this(void)
{
    pj_thread_t *rec = pj_thread_local_get(thread_tls_id);
    pj_assert(rec != NULL);
    return rec;
}

PJ_DEF(pj_status_t) pj_thread_join(pj_thread_t *p)
{
    pj_thread_t *rec = (pj_thread_t *)p;
    PJ_LOG(6, (pj_thread_this()->obj_name, "Joining thread %s", p->obj_name));
    return WaitForSingleObject(rec->hthread, INFINITE)==WAIT_OBJECT_0 ? PJ_OK : -1;
}

PJ_DEF(pj_status_t) pj_thread_destroy(pj_thread_t *p)
{
    pj_thread_t *rec = (pj_thread_t *)p;
    CloseHandle(rec->hthread);
    return PJ_OK;
}

PJ_DEF(pj_status_t) pj_thread_sleep(unsigned msec)
{
    Sleep(msec);
    return PJ_OK;
}

///////////////////////////////////////////////////////////////////////////////
struct pj_atomic_t
{
    long value;
};

PJ_DEF(pj_atomic_t*) pj_atomic_create( pj_pool_t *pool, long initial )
{
    pj_atomic_t *atomic_var = pj_pool_alloc(pool, sizeof(pj_atomic_t));
    if (!atomic_var)
	return NULL;

    atomic_var->value = initial;
    return atomic_var;
}

PJ_DEF(pj_status_t) pj_atomic_destroy( pj_atomic_t *var )
{
    PJ_UNUSED_ARG(var)
    return 0;
}

PJ_DEF(long) pj_atomic_set(pj_atomic_t *atomic_var, long value)
{
    return InterlockedExchange(&atomic_var->value, value);
}

PJ_DEF(long) pj_atomic_get(pj_atomic_t *atomic_var)
{
    return atomic_var->value;
}

PJ_DEF(long) pj_atomic_inc(pj_atomic_t *atomic_var)
{
    PJ_TODO(RUNTIME_DETECTION_OF_WIN32_OSES);

#if defined(PJ_WIN32_WINNT) && PJ_WIN32_WINNT >= 0x0400
    return InterlockedIncrement(&atomic_var->value);
#elif defined(PJ_WIN32_WINCE)
    return InterlockedIncrement(&atomic_var->value);
#else
#   error Fix Me
#endif
}

PJ_DEF(long) pj_atomic_dec(pj_atomic_t *atomic_var)
{
    PJ_TODO(RUNTIME_DETECTION_OF_WIN32_OSES);

#if defined(PJ_WIN32_WINNT) && PJ_WIN32_WINNT >= 0x0400
    return InterlockedDecrement(&atomic_var->value);
#elif defined(PJ_WIN32_WINCE)
    return InterlockedIncrement(&atomic_var->value);
#else
#   error Fix me
#endif
}


///////////////////////////////////////////////////////////////////////////////
PJ_DEF(long) pj_thread_local_alloc(void)
{
    return (long)TlsAlloc();
}

PJ_DEF(void) pj_thread_local_free(long index)
{
    TlsFree(index);
}

PJ_DEF(void) pj_thread_local_set(long index, void *value)
{
    TlsSetValue(index, value);
}

PJ_DEF(void*) pj_thread_local_get(long index)
{
    return TlsGetValue(index);
}

///////////////////////////////////////////////////////////////////////////////
static int init_mutex(pj_mutex_t *mutex, const char *name)
{
#if PJ_WIN32_WINNT >= 0x0400
    InitializeCriticalSection(&mutex->crit);
#else
    mutex->hMutex = CreateMutex(NULL, FALSE, NULL);
    if (!mutex->hMutex) {
	return -1;
    }
#endif

#if PJ_DEBUG
    /* Set owner. */
    mutex->nesting_level = 0;
    mutex->owner = NULL;
#endif

    /* Set name. */
    if (!name) {
	name = "mtx%p";
    }
    if (strchr(name, '%')) {
	pj_snprintf(mutex->obj_name, PJ_MAX_OBJ_NAME, name, mutex);
    } else {
	strncpy(mutex->obj_name, name, PJ_MAX_OBJ_NAME);
	mutex->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    }

    PJ_LOG(6, (mutex->obj_name, "Mutex created"));
    return 0;
}

PJ_DEF(pj_mutex_t*) pj_mutex_create(pj_pool_t *pool, const char *name, int type)
{
    pj_mutex_t *mutex = pj_pool_alloc(pool, sizeof(*mutex));

    PJ_UNUSED_ARG(type)

    return init_mutex(mutex, name)==0 ? mutex : NULL;
}

PJ_DEF(pj_status_t) pj_mutex_lock(pj_mutex_t *mutex)
{
    pj_status_t status;

    PJ_LOG(6,(mutex->obj_name, "Mutex: thread %s is waiting", 
				pj_thread_this()->obj_name));

#if PJ_WIN32_WINNT >= 0x0400
    EnterCriticalSection(&mutex->crit);
    status=PJ_OK;
#else
    status=WaitForSingleObject(mutex->hMutex, INFINITE)==WAIT_OBJECT_0 ? PJ_OK : -1;
#endif
    PJ_LOG(6,(mutex->obj_name, 
	      (status==PJ_OK ? "Mutex acquired by thread %s" : "FAILED by %s"),
	      pj_thread_this()->obj_name));

#if PJ_DEBUG
    if (status == PJ_OK) {
	mutex->owner = pj_thread_this();
	++mutex->nesting_level;
    }
#endif

    return status;
}

PJ_DEF(pj_status_t) pj_mutex_unlock(pj_mutex_t *mutex)
{
    pj_status_t status;

#if PJ_DEBUG
    pj_assert(mutex->owner == pj_thread_this());
    if (--mutex->nesting_level == 0) {
	mutex->owner = NULL;
    }
#endif

    PJ_LOG(6,(mutex->obj_name, "Mutex released by thread %s", 
				pj_thread_this()->obj_name));

#if PJ_WIN32_WINNT >= 0x0400
    LeaveCriticalSection(&mutex->crit);
    status=PJ_OK;
#else
    status=ReleaseMutex(mutex->hMutex) ? PJ_OK : -1;
#endif
    return status;
}

PJ_DEF(pj_status_t) pj_mutex_trylock(pj_mutex_t *mutex)
{
    pj_status_t status;

#if PJ_WIN32_WINNT >= 0x0400
    status=TryEnterCriticalSection(&mutex->crit) ? PJ_OK : -1;
#else
    status=WaitForSingleObject(mutex->hMutex, 0)==WAIT_OBJECT_0 ? PJ_OK : -1;
#endif
    if (status==PJ_OK) {
	PJ_LOG(6,(mutex->obj_name, "Mutex acquired by thread %s", 
				  pj_thread_this()->obj_name));

#if PJ_DEBUG
	mutex->owner = pj_thread_this();
	++mutex->nesting_level;
#endif
    }
    return status;
}

PJ_DEF(pj_status_t) pj_mutex_destroy(pj_mutex_t *mutex)
{
    PJ_LOG(6,(mutex->obj_name, "Mutex destroyed"));

#if PJ_WIN32_WINNT >= 0x0400
    DeleteCriticalSection(&mutex->crit);
    return PJ_OK;
#else
    return CloseHandle(mutex->hMutex) ? PJ_OK : -1;
#endif
}

#if PJ_DEBUG
PJ_DEF(pj_status_t) pj_mutex_is_locked(pj_mutex_t *mutex)
{
    return mutex->owner == pj_thread_this();
}
#endif

///////////////////////////////////////////////////////////////////////////////
PJ_DEF(void) pj_enter_critical_section(void)
{
    pj_mutex_lock(&critical_section_mutex);
}

PJ_DEF(void) pj_leave_critical_section(void)
{
    pj_mutex_unlock(&critical_section_mutex);
}

///////////////////////////////////////////////////////////////////////////////
PJ_DEF(pj_sem_t*) pj_sem_create(pj_pool_t *pool, const char *name,
				unsigned initial, unsigned max)
{
    pj_sem_t *sem;

    sem = pj_pool_alloc(pool, sizeof(*sem));    
    sem->hSemaphore = CreateSemaphore(NULL, initial, max, NULL);
    if (!sem->hSemaphore)
	return NULL;

    /* Set name. */
    if (!name) {
	name = "sem%p";
    }
    if (strchr(name, '%')) {
	pj_snprintf(sem->obj_name, PJ_MAX_OBJ_NAME, name, sem);
    } else {
	strncpy(sem->obj_name, name, PJ_MAX_OBJ_NAME);
	sem->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    }

    PJ_LOG(6, (sem->obj_name, "Semaphore created"));
    return sem;
}

static pj_status_t pj_sem_wait_for(pj_sem_t *sem, unsigned timeout)
{
    DWORD result;

    PJ_LOG(6, (sem->obj_name, "Semaphore: thread %s is waiting", 
			      pj_thread_this()->obj_name));

    result = WaitForSingleObject(sem->hSemaphore, timeout);
    if (result == WAIT_OBJECT_0) {
	PJ_LOG(6, (sem->obj_name, "Semaphore acquired by thread %s", 
				  pj_thread_this()->obj_name));
    } else {
	PJ_LOG(6, (sem->obj_name, "Semaphore: thread %s FAILED to acquire", 
				  pj_thread_this()->obj_name));
    }

    return result==WAIT_OBJECT_0 ? PJ_OK : -1;
}

PJ_DEF(pj_status_t) pj_sem_wait(pj_sem_t *sem)
{
    return pj_sem_wait_for(sem, INFINITE);
}

PJ_DEF(pj_status_t) pj_sem_trywait(pj_sem_t *sem)
{
    return pj_sem_wait_for(sem, 0);
}

PJ_DEF(pj_status_t) pj_sem_post(pj_sem_t *sem)
{
    PJ_LOG(6, (sem->obj_name, "Semaphore released by thread %s",
			      pj_thread_this()->obj_name));
    return ReleaseSemaphore(sem->hSemaphore, 1, NULL) ? 0 : -1;
}

PJ_DEF(pj_status_t) pj_sem_destroy(pj_sem_t *sem)
{
    PJ_LOG(6, (sem->obj_name, "Semaphore destroyed by thread %s",
			      pj_thread_this()->obj_name));
    return CloseHandle(sem->hSemaphore) ? 0 : -1;
}

///////////////////////////////////////////////////////////////////////////////
PJ_DEF(pj_event_t*) pj_event_create(pj_pool_t *pool, const char *name,
				    pj_bool_t manual_reset, pj_bool_t initial)
{
    pj_event_t *event = pj_pool_alloc(pool, sizeof(*event));

    event->hEvent = CreateEvent(NULL, manual_reset?TRUE:FALSE, 
				initial?TRUE:FALSE, NULL);

    if (!event->hEvent)
	return NULL;

    /* Set name. */
    if (!name) {
	name = "evt%p";
    }
    if (strchr(name, '%')) {
	pj_snprintf(event->obj_name, PJ_MAX_OBJ_NAME, name, event);
    } else {
	strncpy(event->obj_name, name, PJ_MAX_OBJ_NAME);
	event->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    }

    PJ_LOG(6, (event->obj_name, "Event created"));
    return event;
}

static pj_status_t pj_event_wait_for(pj_event_t *event, unsigned timeout)
{
    DWORD result;

    PJ_LOG(6, (event->obj_name, "Event: thread %s is waiting", 
			        pj_thread_this()->obj_name));

    result = WaitForSingleObject(event->hEvent, timeout);
    if (result == WAIT_OBJECT_0) {
	PJ_LOG(6, (event->obj_name, "Event: thread %s is released", 
				    pj_thread_this()->obj_name));
    } else {
	PJ_LOG(6, (event->obj_name, "Event: thread %s FAILED to acquire", 
				    pj_thread_this()->obj_name));
    }

    return result==WAIT_OBJECT_0 ? PJ_OK : -1;
}

PJ_DEF(pj_status_t) pj_event_wait(pj_event_t *event)
{
    return pj_event_wait_for(event, INFINITE);
}

PJ_DEF(pj_status_t) pj_event_trywait(pj_event_t *event)
{
    return pj_event_wait_for(event, 0);
}

PJ_DEF(pj_status_t) pj_event_set(pj_event_t *event)
{
    PJ_LOG(6, (event->obj_name, "Setting event"));
    return SetEvent(event->hEvent) ? 0 : -1;
}

PJ_DEF(pj_status_t) pj_event_pulse(pj_event_t *event)
{
    PJ_LOG(6, (event->obj_name, "Pulsing event"));
    return PulseEvent(event->hEvent) ? 0 : -1;
}

PJ_DEF(pj_status_t) pj_event_reset(pj_event_t *event)
{
    PJ_LOG(6, (event->obj_name, "Event is reset"));
    return ResetEvent(event->hEvent) ? 0 : -1;
}

PJ_DEF(pj_status_t) pj_event_destroy(pj_event_t *event)
{
    PJ_LOG(6, (event->obj_name, "Event is destroying"));
    return CloseHandle(event->hEvent) ? 0 : -1;
}

///////////////////////////////////////////////////////////////////////////////

PJ_DEF(pj_status_t) pj_gettimeofday(pj_time_val *tv)
{
    struct timeb tb;
    ftime(&tb);
    tv->sec = tb.time;
    tv->msec = tb.millitm;
    return PJ_OK;
}

PJ_DEF(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt)
{
    #if defined(PJ_WIN32_WINCE)
	SYSTEMTIME local_time;
    GetLocalTime(&local_time);
	
    pt->year = local_time.wYear;
    pt->mon = local_time.wMonth;
    pt->day = local_time.wDay;
    pt->hour = local_time.wHour;
    pt->min = local_time.wMinute;
    pt->sec = local_time.wSecond;
    pt->wday = local_time.wDayOfWeek;
    pt->yday = 0; //note this
    pt->msec = local_time.wMilliseconds;
#else
    struct tm *local_time;
    local_time = localtime((time_t*)&tv->sec);

    pt->year = local_time->tm_year+1900;
    pt->mon = local_time->tm_mon;
    pt->day = local_time->tm_mday;
    pt->hour = local_time->tm_hour;
    pt->min = local_time->tm_min;
    pt->sec = local_time->tm_sec;
    pt->wday = local_time->tm_wday;
    pt->yday = local_time->tm_yday;
    pt->msec = tv->msec;
#endif


    return PJ_OK;
}

/**
 * Encode parsed time to time value.
 */
PJ_DEF(pj_status_t) pj_time_encode(const pj_parsed_time *pt, pj_time_val *tv);

/**
 * Convert local time to GMT.
 */
PJ_DEF(pj_status_t) pj_time_local_to_gmt(pj_time_val *tv);

/**
 * Convert GMT to local time.
 */
PJ_DEF(pj_status_t) pj_time_gmt_to_local(pj_time_val *tv);


///////////////////////////////////////////////////////////////////////////////
/*
 * Terminal
 */
#if !defined(PJ_WIN32_WINCE)

static WORD pj_color_to_os_attr(pj_color_t color)
{
    WORD attr = 0;

    if (color & PJ_TERM_COLOR_R)
	attr |= FOREGROUND_RED;
    if (color & PJ_TERM_COLOR_G)
	attr |= FOREGROUND_GREEN;
    if (color & PJ_TERM_COLOR_B)
	attr |= FOREGROUND_BLUE;
    if (color & PJ_TERM_COLOR_BRIGHT)
	attr |= FOREGROUND_INTENSITY;

    return attr;
}

static pj_color_t os_attr_to_pj_color(WORD attr)
{
    int color = 0;

    if (attr & FOREGROUND_RED)
	color |= PJ_TERM_COLOR_R;
    if (attr & FOREGROUND_GREEN)
	color |= PJ_TERM_COLOR_G;
    if (attr & FOREGROUND_BLUE)
	color |= PJ_TERM_COLOR_B;
    if (attr & FOREGROUND_INTENSITY)
	color |= PJ_TERM_COLOR_BRIGHT;

    return color;
}


/**
 * Set terminal color.
 */
PJ_DEF(pj_status_t) pj_term_set_color(pj_color_t color)
{
    BOOL rc;
    WORD attr = 0;

    attr = pj_color_to_os_attr(color);
    rc = SetConsoleTextAttribute( GetStdHandle(STD_OUTPUT_HANDLE), attr);
    return rc ? PJ_OK : -1;
}

/**
 * Get current terminal foreground color.
 */
PJ_DEF(pj_color_t) pj_term_get_color(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo( GetStdHandle(STD_OUTPUT_HANDLE), &info);
    return os_attr_to_pj_color(info.wAttributes);
}

#else

static short pj_color_to_os_attr(pj_color_t color)
{
     return 0;
}

static pj_color_t os_attr_to_pj_color(short attr)
{
    return 0;
}


/**
 * Set terminal color.
 */
PJ_DEF(pj_status_t) pj_term_set_color(pj_color_t color)
{
    return PJ_OK;
}

/**
 * Get current terminal foreground color.
 */
PJ_DEF(pj_color_t) pj_term_get_color(void)
{
    return 0;
}


#endif

