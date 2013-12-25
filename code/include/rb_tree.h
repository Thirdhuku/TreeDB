#ifndef _RB_TREE_H
#define _RB_TREE_H

/*
red-black properties:
1.Every node is either red or black;
2.The root is black;
3.Every leaf (NIL) is black;
4.If a node is red,then both its children are black
5.For each node,all paths from the node to descendant leaves contain th same
  number of black nodes;
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define RED 	1
#define BLACK	0

struct rb_node {
  struct rb_node *parent;
  struct rb_node *left;
  struct rb_node *right;
  unsigned int color:1;
  uint64_t key;
};

struct rb_root {
  struct rb_node *root;
  uint64_t len;
};


/*
static struct rb_node nil = {
	NULL,
	NULL,
	NULL,
	BLACK,
	};
*/

#define _init_rb_node(_node) {(_node).parent = NULL;\
			     (_node).left   = NULL;\
			     (_node).right  = NULL;\
			     (_node).color  = BLACK;\
			     (_node).key = -1;}

#define _init_rb_root(_root,_key) { _init_rb_node(((_root.root)));\
				  _root.root.key = _key;\
				  _root.root.color = BLACK;}

#define rb_entry(ptr,type,member)	container_of(ptr,type,member)

static inline void init_rb_root(struct rb_root *root,
		struct rb_node *node)
{
	root->root = node;
	if(node)
	{
		root->len = 1;
		node->color = BLACK;
	}
	else	
		root->len = 0;
}

static inline void init_rb_node(struct rb_node *node,uint64_t key)
{
	node->parent = NULL;
	node->left = NULL;
	node->right = NULL;
	node->color = RED;
	node->key = key;	
}

static inline void _left_rotate(struct rb_root *root,struct rb_node *node)
{
	struct rb_node *right;
	
	right = node->right;	
	right->parent = node->parent;
	node->right = right->left;
	if(right->left)
	right->left->parent = node;
	node->parent = right;
	right->left = node;
	
	if( (node == root->root) || !right->parent)
	   root->root = right;
	else
    {
	if(right->parent->left == node)
	   right->parent->left = right;
	else 
	   right->parent->right = right;
    }
}

static void left_rotate(struct rb_root *root,struct rb_node *node)
{
	_left_rotate(root,node);
}
/*
  traverse similar with binary search tree of tree
  @root:which tree you want to get
  @node:which node/root's min value
*/
static inline struct rb_node * tree_min(struct rb_root *root ,\
			struct rb_node *node)
{
	struct rb_node *p,*q;

	p = node->left;
	q = node;

	while(p)
	{
		q = p;
		p = p->left;
	}
	return q;
}

static inline uint64_t rb_tree_min(struct rb_root *root,struct rb_node *node)
{
	struct rb_node *p,*q;
	
	p = node->left;
	q = node;
	
	while(p)
	{
	   q = p;
	   p = p->left;
	}
	return q->key;
}

/*
 traverse similar with binary search tree to get max value of tree,
 various argument equivalent to tree_min method
*/
static inline uint64_t tree_max(struct rb_root *root,struct rb_node *node)
{
	struct rb_node *p,*q;
	
	p = node->right;
	q = node;
	
	while(p)
	{
		q = p;
		p = p->right;
	}
	return q->key;
}
/*
 traverse to get node->key's successor	
 @root:the root of red-black tree
 @node:which the node's successor
 @key:return value about the node's successor 
*/
static inline int tree_successor(struct rb_root *root,struct rb_node *node,
				  uint64_t *key)
{
	struct rb_node *p,*q;

	if(node->right) 
	{
	  q = tree_min(root,node->right);
	  *key = q->key;
	  return 0;
	}

	p = node->parent;
	q = node;

	while(p)
	{
	 if(p->right != q)  break;
	 q = p;
	 p = p->parent;	
	}

	if(p) {
		*key = p->key;
		return 0;
	}

	return -1;	
}

static inline int rb_tree_successor(struct rb_root *root,struct rb_node *node,\
			struct rb_node **key)
{
	struct rb_node *p,*q;
	
	if(node->right)
	{
	  p = tree_min(root,node->right);
	  *key = p;
	  return 0;
	}
	p = node->parent;
	q = node;
	
	while(p)
	{
	  if(p->right != q) break;
	  q = p;
	  p = p->parent;
	}
	if(p) {
	    *key = p;
	    return 0;
	}
	return -1;
}

static inline void _right_rotate(struct rb_root *root,struct rb_node *node)
{
	struct rb_node *left;
	
	left = node->left;
	left->parent = node->parent;
	node->left = left->right;
	if(left->right)
	left->right->parent = node;
	node->parent = left;
	left->right = node;

	if(!left->parent || (root->root == node))
	{
	    root->root = left;
	} 
	else if(left->parent->left == node)
	{
	    left->parent->left =  left;
	}
	else 
	{
	    left->parent->right = left;
	}
	
}

static void right_rotate(struct rb_root *root,struct rb_node *node)
{
	_right_rotate(root,node);
}

int rb_tree_insert(struct rb_root *,struct rb_node *);
struct rb_node *rb_tree_find(const struct rb_root *,uint64_t);
int rb_tree_erase(struct rb_root *,struct rb_node *);

#endif
