#ifndef _HLIST_H_
#define _HLIST_H_

#include <stdlib.h>
#include <stdio.h>

#ifndef offsetof
#define offsetof(TYPE,MEMBER) ((size_t) & ((TYPE*)0)->MEMBER)
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF

#define container_of(ptr,type,member)	({	\
 const typeof( ((type*)0)->member) *__mptr = (ptr); \
 (type*)( (char*)__mptr - offsetof(type,member)); })

#endif

struct hlist_head {
 struct hlist_node *first;
};

struct hlist_node {
 struct hlist_node *next,**pprev;
};

//#define hlist_init_head (name) = {.first = NULL,}
//#define hlist_init_head(_head) _head.first = NULL

static inline void hlist_init_head(struct hlist_head *h)
{
	h->first = NULL;
}

static inline void  hlist_init_node(struct hlist_node *h) 
{
	h->next = NULL;
	h->pprev = NULL;
}

//在struct hlist_head中添加元素n,添加在链表头部
static inline void _hlist_add_head(struct hlist_head *h,struct hlist_node *n)
{
   struct hlist_node *first = h->first;
	
   n -> next = first;
   if(first)
   	first->pprev = &n->next;
   h ->first = n;
   n->pprev = &h->first;
  // printf("node pprev:%p,h->first:%p\n",n->pprev,h->first);
}
static inline int hlist_head(struct hlist_head *h,struct hlist_node *n)
{
	return h->first == n;
}
//在struct hlist_head中添加元素n,添加在链表头部
static inline void hlist_add_head(struct hlist_head *h,struct hlist_node *n)
{
	_hlist_add_head(h,n);
}
//添加节点,在node后面添加node1
static inline void _hlist_add_node(struct hlist_node *node,
				struct hlist_node *node1)
{
	struct hlist_node * n = node->next;
	
	node1 -> next = n;
	node -> next = node1;
	node1 -> pprev = &node -> next;
	
	if(n)
		n -> pprev = &node1 -> next;
	printf("node:%p,node1:%p,n:%p\n",node,node1,n);
}

//在node节点后添加n
static inline void hlist_add_after(struct hlist_node *node,
				    struct hlist_node *n)
{
	_hlist_add_node(node,n);
}
//在prev前面添加n(pprev不可能为空)
static inline void hlist_add_before(struct hlist_node *prev,
					struct hlist_node *n )
{
	n ->next = prev;
	n-> pprev = prev -> pprev;
	*(prev->pprev) = n;
	prev -> pprev = &n->next;
}
/*
*del the element n
*/
static inline void hlist_del(struct hlist_head *h,struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;
    
    if(pprev)
        *pprev = next;
    if(next)
         next->pprev = pprev;

    if(h->first == n) h->first = next;
    /*  
	if(n->pprev)
		*(n->pprev) =  next;
	if(next && next->pprev)
		next -> pprev = n->pprev;
     */
//	else if(!next)
//		printf("next is null\n");
//	else if(!next->pprev)
//		printf("next->pprev is null\n");

	n->next = NULL;
	n->pprev = NULL;
}
static inline void hlist_node_empty(struct hlist_node *n)
{
	if(!n) printf("node is empty!\n");
	else if(!n->next) printf("node->next is empty!\n");
	else if(!n->pprev) printf("node->pprev is empty!\n");
}
static inline int hlist_empty(struct hlist_head *h)
{
	return !h->first;
}

#define hlist_entry(ptr,type,member) container_of(ptr,type,member)

#define hlist_for_each_entry(tpos,pos,head,member) \
	for(pos = (head)->first; pos && \
			(tpos = hlist_entry(pos,typeof(*tpos),member));\
		pos = pos->next)

#define hlist_for_each_entry_del(tpos,pos,_pos,head,member)	\
	for(pos = (head)->first,_pos = pos?pos->next:NULL;\
	    pos &&  (tpos = hlist_entry(pos,typeof(*tpos),member));\
	    pos=_pos,_pos =_pos? _pos->next:NULL)
#endif
