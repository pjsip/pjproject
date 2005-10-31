/* $Header: /pjproject-0.3/pjlib/include/pj/types.h 11    10/14/05 12:26a Bennylp $ */

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

PJ_BEGIN_DECL

///////////////////////////////////////////////////////////////////////////////

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
#define PJ_SUCCESS  0

/** True value. */
#define PJ_TRUE	    1

/** False value. */
#define PJ_FALSE    0


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
 * This structure should be opaque, however applications need to declare
 * concrete variable of this type, that's why the declaration is visible here.
 */
typedef struct pj_hash_iterator_t
{
    pj_uint32_t	     index;     /**< Internal index.     */
    pj_hash_entry   *entry;     /**< Internal entry.     */
} pj_hash_iterator_t;


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

/**
 * Value type of an atomic variable.
 */
typedef PJ_ATOMIC_VALUE_TYPE pj_atomic_value_t;
 
///////////////////////////////////////////////////////////////////////////////

/** Thread handle. */
typedef struct pj_thread_t pj_thread_t;

/** Lock object. */
typedef struct pj_lock_t pj_lock_t;

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

/** Exception id. */
typedef int pj_exception_id_t;

///////////////////////////////////////////////////////////////////////////////

/** Utility macro to compute the number of elements in static array. */
#define PJ_ARRAY_SIZE(a)    (sizeof(a)/sizeof(a[0]))

/** Maximum value for signed 32-bit integer. */
#define PJ_MAXINT32  0x7FFFFFFFL

/**
 * Length of object names.
 */
#define PJ_MAX_OBJ_NAME	16

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
 * @param t     Time value to be normalized.
 */
PJ_DECL(void) pj_time_val_normalize(pj_time_val *t);

/**
 * Get the total time value in miliseconds. This is the same as
 * multiplying the second part with 1000 and then add the miliseconds
 * part to the result.
 *
 * @param t     The time value.
 * @return      Total time in miliseconds.
 * @hideinitializer
 */
#define PJ_TIME_VAL_MSEC(t)	((t).sec * 1000 + (t).msec)

/**
 * This macro will check if \a t1 is equal to \a t2.
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if both time values are equal.
 * @hideinitializer
 */
#define PJ_TIME_VAL_EQ(t1, t2)	((t1).sec==(t2).sec && (t1).msec==(t2).msec)

/**
 * This macro will check if \a t1 is greater than \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is greater than t2.
 * @hideinitializer
 */
#define PJ_TIME_VAL_GT(t1, t2)	((t1).sec>(t2).sec || \
                                ((t1).sec==(t2).sec && (t1).msec>(t2).msec))

/**
 * This macro will check if \a t1 is greater than or equal to \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is greater than or equal to t2.
 * @hideinitializer
 */
#define PJ_TIME_VAL_GTE(t1, t2)	(PJ_TIME_VAL_GT(t1,t2) || \
                                 PJ_TIME_VAL_EQ(t1,t2))

/**
 * This macro will check if \a t1 is less than \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is less than t2.
 * @hideinitializer
 */
#define PJ_TIME_VAL_LT(t1, t2)	(!(PJ_TIME_VAL_GTE(t1,t2)))

/**
 * This macro will check if \a t1 is less than or equal to \a t2.
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is less than or equal to t2.
 * @hideinitializer
 */
#define PJ_TIME_VAL_LTE(t1, t2)	(!PJ_TIME_VAL_GT(t1, t2))

/**
 * Add \a t2 to \a t1 and store the result in \a t1. Effectively
 *
 * this macro will expand as: (\a t1 += \a t2).
 * @param t1    The time value to add.
 * @param t2    The time value to be added to \a t1.
 * @hideinitializer
 */
#define PJ_TIME_VAL_ADD(t1, t2)	    do {			    \
					(t1).sec += (t2).sec;	    \
					(t1).msec += (t2).msec;	    \
					pj_time_val_normalize(&(t1)); \
				    } while (0)


/**
 * Substract \a t2 from \a t1 and store the result in \a t1. Effectively
 * this macro will expand as (\a t1 -= \a t2).
 *
 * @param t1    The time value to subsctract.
 * @param t2    The time value to be substracted from \a t1.
 * @hideinitializer
 */
#define PJ_TIME_VAL_SUB(t1, t2)	    do {			    \
					(t1).sec -= (t2).sec;	    \
					(t1).msec -= (t2).msec;	    \
					pj_time_val_normalize(&(t1)); \
				    } while (0)


/**
 * This structure represent the parsed representation of time.
 * It is acquired by calling #pj_time_decode().
 */
typedef struct pj_parsed_time
{
    /** This represents day of week where value zero means Sunday */
    int wday;

    /** This represents day of the year, 0-365, where zero means
     *  1st of January.
     */
    int yday;

    /** This represents day of month: 1-31 */
    int day;

    /** This represents month, with the value is 0 - 11 (zero is January) */
    int mon;

    /** This represent the actual year (unlike in ANSI libc where
     *  the value must be added by 1900).
     */
    int year;

    /** This represents the second part, with the value is 0-59 */
    int sec;

    /** This represents the minute part, with the value is: 0-59 */
    int min;

    /** This represents the hour part, with the value is 0-23 */
    int hour;

    /** This represents the milisecond part, with the value is 0-999 */
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
    PJ_TERM_COLOR_R	= 2,    /**< Red            */
    PJ_TERM_COLOR_G	= 4,    /**< Green          */
    PJ_TERM_COLOR_B	= 1,    /**< Blue.          */
    PJ_TERM_COLOR_BRIGHT = 8    /**< Bright mask.   */
};




PJ_END_DECL


#endif /* __PJ_TYPES_H__ */

