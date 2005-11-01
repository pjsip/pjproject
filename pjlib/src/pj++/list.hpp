/* $Id$
 *
 */
#ifndef __PJPP_LIST_H__
#define __PJPP_LIST_H__

#include <pj/list.h>

template <typename T>
struct PJ_List_Node
{
    PJ_DECL_LIST_MEMBER(T)
};


template <class Node>
class PJ_List
{
public:
    PJ_List() { pj_list_init(&root_); if (0) compiletest(); }
    ~PJ_List() {}

    class const_iterator
    {
    public:
	const_iterator() : node_(NULL) {}
	const_iterator(const Node *nd) : node_((Node*)nd) {}
	const Node * operator *() { return node_; }
	const Node * operator -> () { return node_; }
	const_iterator operator++() { return const_iterator(node_->next); }
	bool operator==(const const_iterator &rhs) { return node_ == rhs.node_; }
	bool operator!=(const const_iterator &rhs) { return node_ != rhs.node_; }

    protected:
	Node *node_;
    };

    class iterator : public const_iterator
    {
    public:
	iterator() {}
	iterator(Node *nd) : const_iterator(nd) {}
	Node * operator *() { return node_; }
	Node * operator -> () { return node_; }
	iterator operator++() { return iterator(node_->next); }
	bool operator==(const iterator &rhs) { return node_ == rhs.node_; }
	bool operator!=(const iterator &rhs) { return node_ != rhs.node_; }
    };

    bool empty() const
    {
	return pj_list_empty(&root_);
    }

    iterator begin()
    {
	return iterator(root_.next);
    }

    const_iterator begin() const
    {
	return const_iterator(root_.next);
    }

    const_iterator end() const
    {
	return const_iterator((Node*)&root_);
    }

    iterator end()
    {
	return iterator((Node*)&root_);
    }

    void insert_before (iterator &pos, Node *node)
    {
	pj_list_insert_before( *pos, node );
    }

    void insert_after(iterator &pos, Node *node)
    {
	pj_list_insert_after(*pos, node);
    }

    void merge_first(Node *list2)
    {
	pj_list_merge_first(&root_, list2);
    }

    void merge_last(PJ_List *list)
    {
	pj_list_merge_last(&root_, &list->root_);
    }

    void insert_nodes_before(iterator &pos, PJ_List *list2)
    {
	pj_list_insert_nodes_before(*pos, &list2->root_);
    }

    void insert_nodes_after(iterator &pos, PJ_List *list2)
    {
	pj_list_insert_nodes_after(*pos, &list2->root_);
    }

    void erase(iterator &it)
    {
	pj_list_erase(*it);
    }

    Node *front()
    {
	return root_.next;
    }

    const Node *front() const
    {
	return root_.next;
    }

    void pop_front()
    {
	pj_list_erase(root_.next);
    }

    Node *back()
    {
	return root_.prev;
    }

    const Node *back() const
    {
	return root_.prev;
    }

    void pop_back()
    {
	pj_list_erase(root_.prev);
    }

    iterator find(Node *node)
    {
	Node *n = pj_list_find_node(&root_, node);
	return n ? iterator(n) : end();
    }

    const_iterator find(Node *node) const
    {
	Node *n = pj_list_find_node(&root_, node);
	return n ? const_iterator(n) : end();
    }

    void push_back(Node *node)
    {
	pj_list_insert_after(root_.prev, node);
    }

    void push_front(Node *node)
    {
	pj_list_insert_before(root_.next, node);
    }

    void clear()
    {
	root_.next = &root_;
	root_.prev = &root_;
    }

private:
    struct RootNode
    {
	PJ_DECL_LIST_MEMBER(Node)
    } root_;

    void compiletest()
    {
	// If you see error in this line, 
	// it's because Node is not derived from PJ_List_Node.
	Node *n = (Node*)0;
	n = n->next; n = n->prev;
    }
};


#endif	/* __PJPP_LIST_H__ */
