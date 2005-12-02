/* $Header: /pjproject/pjlib/src/test/libpj_test.h 5     6/04/05 4:30p Bennylp $ */
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
#ifndef __PJLIB_TEST_H__
#define __PJLIB_TEST_H__

#include <pj/types.h>

int pool_test();
int exception_test();
int list_test();
int timer_test();
int rbtree_test();
int os_test();
int udp_ioqueue_test();
int tcp_ioqueue_test();
int fifobuf_test();
int xml_test();

extern pj_pool_factory *mem;


#endif	/* __PJLIB_TEST_H__ */

