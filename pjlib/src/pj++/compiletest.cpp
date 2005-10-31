/* $Header: /pjproject/pjlib/src/pj++/compiletest.cpp 4     8/24/05 10:29a Bennylp $ */
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
