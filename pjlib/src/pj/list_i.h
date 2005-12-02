/* $Header: /pjproject/pjlib/src/pj/list_i.h 2     2/24/05 10:34a Bennylp $ */
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


/* Internal */
PJ_IDEF(void) pj_link_node(pj_list_type *prev, pj_list_type *next)
{
    ((pj_list*)prev)->next = next;
    ((pj_list*)next)->prev = prev;
}

PJ_IDEF(void) 
pj_list_init(pj_list_type * node)
{
    ((pj_list*)node)->next = ((pj_list*)node)->prev = node;
}

PJ_IDEF(int) pj_list_empty(const pj_list_type * node)
{
    return ((pj_list*)node)->next == node;
}

PJ_IDEF(void) 
pj_list_insert_after(pj_list_type *pos, pj_list_type *node)
{
    ((pj_list*)node)->prev = pos;
    ((pj_list*)node)->next = ((pj_list*)pos)->next;
    ((pj_list*) ((pj_list*)pos)->next) ->prev = node;
    ((pj_list*)pos)->next = node;
}


PJ_IDEF(void) 
pj_list_insert_before(pj_list_type *pos, pj_list_type *node)
{
    pj_list_insert_after(((pj_list*)pos)->prev, node);
}


PJ_IDEF(void)	    
pj_list_insert_nodes_after(pj_list_type *pos, pj_list_type *lst)
{
    pj_list *lst_last = (pj_list *) ((pj_list*)lst)->prev;
    pj_list *pos_next = (pj_list *) ((pj_list*)pos)->next;

    pj_link_node(pos, lst);
    pj_link_node(lst_last, pos_next);
}

PJ_IDEF(void) 
pj_list_insert_nodes_before(pj_list_type *pos, pj_list_type *lst)
{
    pj_list_insert_nodes_after(((pj_list*)pos)->prev, lst);
}

PJ_IDEF(void)
pj_list_merge_last(pj_list_type *lst1, pj_list_type *lst2)
{
    pj_link_node(((pj_list*)lst1)->prev, ((pj_list*)lst2)->next);
    pj_link_node(((pj_list*)lst2)->prev, lst1);
    pj_list_init(lst2);
}

PJ_IDEF(void)
pj_list_merge_first(pj_list_type *lst1, pj_list_type *lst2)
{
    pj_link_node(((pj_list*)lst2)->prev, ((pj_list*)lst1)->next);
    pj_link_node(((pj_list*)lst1), ((pj_list*)lst2)->next);
    pj_list_init(lst2);
}

PJ_IDEF(void) 
pj_list_erase(pj_list_type *node)
{
    pj_link_node( ((pj_list*)node)->prev, ((pj_list*)node)->next);
}


PJ_IDEF(pj_list_type*) 
pj_list_find_node(pj_list_type *list, pj_list_type *node)
{
    pj_list *p = (pj_list *) ((pj_list*)list)->next;
    while (p != list && p != node)
	p = (pj_list *) p->next;

    return p==node ? p : NULL;
}


PJ_IDEF(pj_list_type*) 
pj_list_search(pj_list_type *list, void *value,
	       int (*comp)(void *value, const pj_list_type *node))
{
    pj_list *p = (pj_list *) ((pj_list*)list)->next;
    while (p != list && (*comp)(value, p) != 0)
	p = (pj_list *) p->next;

    return p==list ? NULL : p;
}

