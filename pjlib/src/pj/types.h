/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/types.h,v 1.1 2005/12/02 20:02:31 nn Exp $ */
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

#ifndef __PJ_TYPES_H__
#define __PJ_TYPES_H__


/**
 * @defgroup PJ PJ Library
 */
/**
 * @file types.h
 * @brief Declaration of basic types and utility.
 */
/**
 * @defgroup PJ_BASIC Basic Data Types and Library Functionality.
 * @ingroup PJ_DS
 * @{
 */
#include <pj/config.h>
#include <pj/compat.h>

PJ_BEGIN_DECL

///////////////////////////////////////////////////////////////////////////////
/* 
 * Basic types.
 */
#if defined(_MSC_VER)
  typedef __int64	    pj_int64_t;
  typedef unsigned __int64  pj_uint64_t;
#elif defined(__GNUC__)
  typedef long long	    pj_int64_t;
  typedef unsigned long long pj_uint64_t;
#else
# error Got to define 64bit integers for this compiler.
#endif

/** Unsigned 32bit integer. */
typedef int		pj_int32_t;

/** Signed 32bit integer. */
typedef unsigned int	pj_uint32_t;

/** Unsigned 16bit integer. */
typedef short		pj_int16_t;

/** Signed 16bit integer. */
typedef unsigned short	pj_uint16_t;

/** Unsigned 8bit integer. */
typedef signed char	pj_int8_t;

/** Signed 16bit integer. */
typedef unsigned char	pj_uint8_t;

/** Large unsigned integer. */
typedef size_t		pj_size_t;

/** Large signed integer. */
typedef long		pj_ssize_t;

/** Status code. */
typedef int		pj_status_t;

/** Boolean. */
typedef int		pj_bool_t;

/** Status is OK. */
#define PJ_OK	    0

/** True value. */
#define PJ_TRUE	    1

/** False value. */
#define PJ_FALSE    0

/* Input or output argument type. */
#define PJ_ARG_IN
#define PJ_ARG_OUT


///////////////////////////////////////////////////////////////////////////////
/*
 * Data structure types.
 */
/**
 * This type is used as replacement to legacy C string, and used throughout
 * the library. By convention, the string is NOT null terminated.
 */
struct pj_str_t
{
    /** Buffer pointer, which is by convention NOT null terminated. */
    char       *ptr;

    /** The length of the string. */
    pj_ssize_t  slen;
};


/**
 * The opaque data type for linked list, which is used as arguments throughout
 * the linked list operations.
 */
typedef void pj_list_type;

/** 
 * List.
 */
typedef struct pj_list pj_list;

/**
 * Opaque data type for hash tables.
 */
typedef struct pj_hash_table_t pj_hash_table_t;

/**
 * Opaque data type for hash entry (only used internally by hash table).
 */
typedef struct pj_hash_entry pj_hash_entry;

/**
 * Data type for hash search iterator.
 */
struct pj_hash_iterator_t
{
    pj_uint32_t	     index;
    pj_hash_entry   *entry;
};

typedef struct pj_hash_iterator_t pj_hash_iterator_t;

/**
 * Forward declaration for memory pool factory.
 */
typedef struct pj_pool_factory pj_pool_factory;

/**
 * Opaque data type for memory pool.
 */
typedef struct pj_pool_t pj_pool_t;

/**
 * Forward declaration for caching pool, a pool factory implementation.
 */
typedef struct pj_caching_pool pj_caching_pool;

/**
 * This type is used as replacement to legacy C string, and used throughout
 * the library.
 */
typedef struct pj_str_t pj_str_t;

/**
 * Opaque data type for I/O Queue structure.
 */
typedef struct pj_ioqueue_t pj_ioqueue_t;

/**
 * Opaque data type for key that identifies a handle registered to the
 * I/O queue framework.
 */
typedef struct pj_ioqueue_key_t pj_ioqueue_key_t;

/**
 * Opaque data to identify timer heap.
 */
typedef struct pj_timer_heap_t pj_timer_heap_t;

/**
 * Forward declaration for timer entry.
 */
typedef struct pj_timer_entry pj_timer_entry;

/** 
 * Opaque data type for atomic operations.
 */
typedef struct pj_atomic_t pj_atomic_t;

///////////////////////////////////////////////////////////////////////////////

/** Thread handle. */
typedef struct pj_thread_t pj_thread_t;

/** Mutex handle. */
typedef struct pj_mutex_t pj_mutex_t;

/** Semaphore handle. */
typedef struct pj_sem_t pj_sem_t;

/** Event object. */
typedef struct pj_event_t pj_event_t;

/** Unidirectional stream pipe object. */
typedef struct pj_pipe_t pj_pipe_t;

/** Operating system handle. */
typedef void *pj_oshandle_t;

/** Socket handle. */
typedef long pj_sock_t;

/** Generic socket address. */
typedef void pj_sockaddr_t;

/** Color type. */
typedef unsigned int pj_color_t;

///////////////////////////////////////////////////////////////////////////////

/** Utility macro to compute the number of elements in static array. */
#define PJ_ARRAY_SIZE(a)    (sizeof(a)/sizeof(a[0]))

/* Debugging flag. */
#ifndef NDEBUG
#   define PJ_DEBUG 1
#else
#   define PJ_DEBUG 0
#endif

#define PJ_MAXLONG  0x7FFFFFFFL
/*
 * Object name identification.
 */
#define PJ_MAX_OBJ_NAME	16
typedef char pj_obj_name[PJ_MAX_OBJ_NAME];

///////////////////////////////////////////////////////////////////////////////
/*
 * General.
 */
/**
 * Initialize the PJ Library.
 * This function must be called before using the library. The purpose of this
 * function is to initialize static library data, such as character table used
 * in random string generation, and to initialize operating system dependent
 * functionality (such as WSAStartup() in Windows).
 */
PJ_DECL(pj_status_t) pj_init(void);


/**
 * @}
 */
/**
 * @addtogroup PJ_TIME Time Data Type and Manipulation.
 * @ingroup PJ_MISC
 * @{
 */

/**
 * Representation of time value in this library.
 * This type can be used to represent either an interval or a specific time
 * or date. 
 */
typedef struct pj_time_val
{
    /** The seconds part of the time. */
    long    sec;

    /** The miliseconds fraction of the time. */
    long    msec;

} pj_time_val;

/**
 * Normalize the value in time value.
 */
PJ_DECL(void) pj_time_val_normalize(pj_time_val *t);

/**
 * Convert time value to miliseconds.
 */
#define PJ_TIME_VAL_MSEC(t)	((t).sec * 1000 + (t).msec)

/**
 * This macros returns non-zero if t1==t2.
 */
#define PJ_TIME_VAL_EQ(t1, t2)	((t1).sec==(t2).sec && (t1).msec==(t2).msec)

/**
 * This macros returns non-zero if t1 > t2
 */
#define PJ_TIME_VAL_GT(t1, t2)	((t1).sec>(t2).sec || ((t1).sec==(t2).sec && (t1).msec>(t2).msec))

/**
 * This macros returns non-zero if t1 >= t2
 */
#define PJ_TIME_VAL_GTE(t1, t2)	(PJ_TIME_VAL_GT(t1,t2) || PJ_TIME_VAL_EQ(t1,t2))

/**
 * This macros returns non-zero if t1 < t2
 */
#define PJ_TIME_VAL_LT(t1, t2)	(!(PJ_TIME_VAL_GTE(t1,t2)))

/**
 * This macros returns non-zero if t1 <= t2.
 */
#define PJ_TIME_VAL_LTE(t1, t2)	(!PJ_TIME_VAL_GT(t1, t2))

/**
 * Add t2 to t1 (t1 = t1 + t2).
 */
#define PJ_TIME_VAL_ADD(t1, t2)	    do {			    \
					(t1).sec += (t2).sec;	    \
					(t1).msec += (t2).msec;	    \
					pj_time_val_normalize(&(t1)); \
				    } while (0)


/**
 * Substract t2 from t1 (t1 = t1 - t2).
 */
#define PJ_TIME_VAL_SUB(t1, t2)	    do {			    \
					(t1).sec -= (t2).sec;	    \
					(t1).msec -= (t2).msec;	    \
					pj_time_val_normalize(&(t1)); \
				    } while (0)


/**
 * Parsed value of time.
 */
typedef struct pj_parsed_time
{
    /** Day of week: 0=Sunday */
    int wday;

    /** Day of the year, 0-365. */
    int yday;

    /** Day of month: 1-31 */
    int day;

    /** Month: 0 - 11 */
    int mon;

    /** Year: 1900+ */
    int year;

    /** Second: 0-59 */
    int sec;

    /** Minute: 0-59 */
    int min;

    /** Hour: 0-23 */
    int hour;

    /** Miliseconds. */
    int msec;

} pj_parsed_time;


/**
 * @}	// Time Management
 */

///////////////////////////////////////////////////////////////////////////////
/*
 * Terminal.
 */
/**
 * Color code combination.
 */
enum {
    PJ_TERM_COLOR_R	= 2,
    PJ_TERM_COLOR_G	= 4,
    PJ_TERM_COLOR_B	= 1,
    PJ_TERM_COLOR_BRIGHT = 8
};



///////////////////////////////////////////////////////////////////////////////
/* 
 * Util macros.
 */

/**
 * Assertion macro.
 */
#define pj_assert(expr)	    assert(expr)

///////////////////////////////////////////////////////////////////////////////
/** 
 * Base class for PJ classes.
 * Currently it used only to give name to instances, and this is only used
 * for logging purpose. 
 */
typedef struct pj_object
{
    char obj_name[32];
} pj_object;


#define PJ_OBJ_DECL_DERIVED_FROM(obj_type)   obj_type	obj_type##__
#define PJ_OBJ(var,obj_type)		     (&((var)->obj_type##__))


PJ_END_DECL


#endif /* __PJ_TYPES_H__ */

