/* 
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJ_COMPAT_RAND_H__
#define __PJ_COMPAT_RAND_H__

/**
 * @file rand.h
 * @brief Provides platform_rand() and platform_srand() functions.
 */

#if defined(PJ_ZEPHYR) && PJ_ZEPHYR != 0
   /*
    * Zephyr's generator is fed by the entropy driver, so it needs no seed,
    * whereas the C library's rand() would repeat the same sequence at
    * every boot.
    */
#  include <zephyr/random/random.h>
#  define platform_srand(seed)  ((void)(seed))
   PJ_INLINE(int) platform_rand(void)
   {
       return (int)(sys_rand32_get() & 0x7FFFFFFF);
   }
#elif defined(PJ_HAS_STDLIB_H) && PJ_HAS_STDLIB_H != 0
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

#else
#  warning "platform_rand() is not implemented"
#  define platform_rand()       1
#  define platform_srand(seed)

#endif


#endif  /* __PJ_COMPAT_RAND_H__ */

