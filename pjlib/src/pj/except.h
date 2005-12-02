/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/except.h,v 1.1 2005/12/02 20:02:29 nn Exp $ */
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

#ifndef __PJ_EXCEPTION_H__
#define __PJ_EXCEPTION_H__

/**
 * @file except.h
 * @brief Exception Handling in C.
 */

#include <setjmp.h>
#include <pj/types.h>

#ifdef __cplusplus
#  if !defined(PJ_EXCEPT_USE_CPP)
#     define PJ_EXCEPT_USE_CPP	0
#  endif

     PJ_BEGIN_DECL

#endif


/**
 * @defgroup PJ_EXCEPT Exception Handling
 * @ingroup PJ_MISC
 * @{
 *
 * This module provides exception handling functionality similar to C++ in
 * C language. The underlying mechanism use setjmp() and longjmp(), and since
 * these constructs are ANSI standard, the mechanism here should be available
 * on all platforms/compilers which are ANSI compliant (effectively almost all
 * platforms/compilers, including embedded ones).
 *
 * The exception handling mechanism is completely thread safe, so the exception
 * thrown by one thread will not interfere with other thread.
 *
 * CAVEATS:
 *  - unlike C++ exception, the scheme here won't call destructors of local
 *    objects if exception is thrown.
 *  - You CAN NOT make nested exception in one single function without using
 *    a nested PJ_USE_EXCEPTION.
 *  - Exceptions will always be caught by the first handle (unlike C++ where
 *    exception is only caught if the type matches.
 *
 * The exception handling constructs are similar to C++. The blocks will be
 * constructed similar to the following sample:
 *
 * <pre>
 * int main()
 * {
 *    PJ_USE_EXCEPTION;  // declare local exception stack.
 *
 *    ...
 *    PJ_TRY {
 *      ... 
 *      // do something..
 *      ...
 *    }
 *    PJ_CATCH( 1 ) {
 *      ...
 *      // handle exception 1
 *    }
 *    PJ_CATCH( 2 ) {
 *      ...
 *      // handle exception 2
 *    }
 *    PJ_DEFAULT {
 *      ...
 *      // handle other exceptions.
 *    }
 *    PJ_END;
 * }
 * </pre>
 *
 * Below is the keywords in the exception handling mechanism.
 *
 * \section PJ_EX_KEYWORDS Keywords
 *
 * \subsection PJ_THROW PJ_THROW(expression)
 * Throw an exception. The expression thrown is an integer as the result of
 * the \a expression. This keyword can be specified anywhere within the 
 * program.
 *
 * \subsection PJ_USE_EXCEPTION PJ_USE_EXCEPTION
 * Specify this in the variable definition section of the function block 
 * (or any blocks) to specify that the block has \a PJ_TRY/PJ_CATCH exception 
 * block. 
 * Actually, this is just a macro to declare local variable which is used to
 * push the exception state to the exception stack.
 *
 * \subsection PJ_TRY PJ_TRY
 * The \a PJ_TRY keyword is typically followed by a block. If an exception is
 * thrown in this block, then the execution will resume to the \a PJ_CATCH 
 * handler.
 *
 * \subsection PJ_CATCH PJ_CATCH(expression)
 * The \a PJ_CATCH is normally followed by a block. This block will be executed
 * if the exception being thrown is equal to the expression specified in the
 * \a PJ_CATCH.
 *
 * \subsection PJ_DEFAULT PJ_DEFAULT
 * The \a PJ_DEFAULT keyword is normally followed by a block. This block will
 * be executed if the exception being thrown doesn't match any of the \a
 * PJ_CATCH specification. The \a PJ_DEFAULT block \b MUST be placed as the
 * last block of the handlers.
 *
 * \subsection PJ_END PJ_END
 * Specify this keyword to mark the end of \a PJ_TRY / \a PJ_CATCH blocks.
 *
 * \subsection PJ_GET_EXCEPTION PJ_GET_EXCEPTION(void)
 * Get the last exception thrown. This macro is normally called inside the
 * \a PJ_CATCH or \a PJ_DEFAULT block, altough it can be used anywhere where
 * the \a PJ_USE_EXCEPTION definition is in scope.
 *
 * \section PJ_EX_SAMPLE Sample
 * Here's a complete sample of how to use the exception.
 *
 * \verbatim
   #define EX_NO_MEMORY	    1
  
   void randomly_throw_exception()
   {
	unsigned value = pj_rand();
	if (value < 10)
	    PJ_THROW(value);
   }

   void *my_malloc(size_t size)
   {
  	void *ptr = malloc(size);
  	if (!ptr)
  	    PJ_THROW(EX_NO_MEMORY);
  	return ptr;
   }
  
   int func()
   {
  	void *data;
  	PJ_USE_EXCEPTION;
  
  	PJ_TRY {
  	    data = my_malloc(200);
	    randomly_throw_exception();
  	}
  	PJ_CATCH( EX_NO_MEMORY ) {
  	    puts("Can't allocate memory");
  	    return 0;
  	}
	PJ_DEFAULT {
	    printf("Caught random exception %d\n", PJ_GET_EXCEPTION());
	}
  	PJ_END
    }
   \endverbatim
 *
 * @}
 */

#if PJ_EXCEPT_USE_CPP

#define PJ_USE_EXCEPTION
#define PJ_TRY		    try
#define PJ_CATCH(id)	    
#define PJ_DEFAULT	    
#define PJ_GET_EXCEPTION    
#define PJ_THROW(id)	    
#define PJ_END		    

#else	/* PJ_EXCEPT_USE_CPP */

/*
 * This structure (which should be invisible to user) manages the TRY handler
 * stack.
 */
struct pj_exception_state_t
{
    struct pj_exception_state_t *prev;
    jmp_buf state;
};

PJ_DECL_NO_RETURN(void) pj_throw_exception_(int id) PJ_ATTR_NORETURN;
PJ_DECL(void) pj_push_exception_handler_(struct pj_exception_state_t *rec);
PJ_DECL(void) pj_pop_exception_handler_();

#define PJ_USE_EXCEPTION    struct pj_exception_state_t pj_x_except__; int pj_x_code__

#define PJ_TRY		    if (1) { \
				pj_push_exception_handler_(&pj_x_except__); \
				pj_x_code__ = setjmp(pj_x_except__.state); \
				if (pj_x_code__ == 0)
#define PJ_CATCH(id)	    else if (pj_x_code__ == (id))
#define PJ_DEFAULT	    else
#define PJ_END			pj_pop_exception_handler_(); \
			    } else {}
#define PJ_THROW(exception_id)	pj_throw_exception_(exception_id)
#define PJ_GET_EXCEPTION()	(pj_x_code__)


#endif	/* PJ_EXCEPT_USE_CPP */


#if defined(__cplusplus)
  PJ_END_DECL
#endif

#endif	/* __PJ_EXCEPTION_H__ */


