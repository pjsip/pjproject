/* $Header: /pjproject/pjlib/src/pj++/compiletest.cpp 4     8/24/05 10:29a Bennylp $ */
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
#include <pjlib++.hpp>


#if 0
struct MyNode
{
    PJ_DECL_LIST_MEMBER(struct MyNode)
    int data;
};

int test()
{
    typedef PJ_List<MyNode> MyList;
    MyList list;
    MyList::iterator it, end = list.end();

    for (it=list.begin(); it!=end; ++it) {
	MyNode *n = *it;
    }

    return 0;
}

int test_scan()
{
    PJ_Scanner scan;
    PJ_String s;
    PJ_CharSpec cs;

    scan.get(&cs, &s);
    return 0;
}

int test_scan_c()
{
    pj_scanner scan;
    pj_str_t s;
    pj_char_spec cs;

    pj_scan_get(&scan, cs, &s);
    return 0;
}
#endif
