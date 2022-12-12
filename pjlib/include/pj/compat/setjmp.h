/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_COMPAT_SETJMP_H__
#define __PJ_COMPAT_SETJMP_H__

/**
 * @file setjmp.h
 * @brief Provides setjmp.h functionality.
 */

#if defined(PJ_HAS_SETJMP_H) && PJ_HAS_SETJMP_H != 0
#  include <setjmp.h>
   typedef jmp_buf pj_jmp_buf;
#  ifndef pj_setjmp
#    define pj_setjmp(buf)      setjmp(buf)
#  endif
#  ifndef pj_longjmp
#    define pj_longjmp(buf,d)   longjmp(buf,d)
#  endif

#elif defined(PJ_SYMBIAN) && PJ_SYMBIAN!=0
    /* Symbian framework don't use setjmp/longjmp */
    
#else
#  warning "setjmp()/longjmp() is not implemented"
   typedef int pj_jmp_buf[1];
#  define pj_setjmp(buf)        0
#  define pj_longjmp(buf,d)     0
#endif


#endif  /* __PJ_COMPAT_SETJMP_H__ */

