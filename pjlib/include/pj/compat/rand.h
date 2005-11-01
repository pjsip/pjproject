/* $Header: /pjproject-0.3/pjlib/include/pj/compat/rand.h 3     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/include/pj/compat/rand.h $
 * 
 * 3     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 2     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_RAND_H__
#define __PJ_COMPAT_RAND_H__

/**
 * @file rand.h
 * @brief Provides platform_rand() and platform_srand() functions.
 */

#if defined(PJ_HAS_STDLIB_H) && PJ_HAS_STDLIB_H != 0
   /*
    * Use stdlib based rand() and srand().
    */
#  include <stdlib.h>
#  define platform_srand    srand
#  if defined(RAND_MAX) && RAND_MAX <= 0xFFFF
       /*
        * When rand() is only 16 bit strong, double the strength
	* by calling it twice!
	*/
       PJ_INLINE(int) platform_rand(void)
       {
	   return ((rand() & 0xFFFF) << 16) | (rand() & 0xFFFF);
       }
#  else
#      define platform_rand rand
#  endif

#elif defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL != 0
   /*
    * Linux kernel mode random number generator.
    */
#  include <linux/random.h>
#  define platform_srand(seed)

   PJ_INLINE(int) platform_rand(void)
   {
     int value;
     get_random_bytes((void*)&value, sizeof(value));
     return value;
   }

#else
#  warning "platform_rand() is not implemented"
#  define platform_rand()	1
#  define platform_srand(seed)

#endif


#endif	/* __PJ_COMPAT_RAND_H__ */

