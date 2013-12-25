#include "rb_tree.h"
#include <stdio.h>

static void _rb_tree_find(struct rb_root *root,struct rb_node *node
			        ,struct rb_node **q)
{
	struct rb_node *p = root->root;
	

	while(p && root->len)
	{
	   *q = p;
#ifdef DEUG
	   printf("key:%llu,parent;%llu\n",(*q)->key,(*q)->parent?\
		(*q)->parent->key:-1);
#endif
	   if(p->key < node->key)
		p = p->right;
	   else p = p->left;
	}

}

struct rb_node * rb_tree_find(const struct rb_root *root,uint64_t key)
{
//	struct rb_node *node = NULL;

	struct rb_node *p = root->root;
	while(p)
	{
		if(p->key < key)	p = p->right;
		else if(p->key == key)	return p;
		else	p = p->left;
	}	

	return NULL;
}

static void rb_tree_find1(struct rb_root *root)
{
	uint64_t i,j;
	struct rb_node *list[root->len];
	struct rb_node *p;

	i=0;
	j=0;

	list[i++] = root->root;
	p  = list[j++];
	
	printf("length:%llu\n",root->len);

	while( j<= i && p)
	{
	  if(i==1)  printf("(root)");	
	  printf("p->key:%llu(color:%d),parent:%llu ",p->key,p->color,\
			p->parent?p->parent->key:0);
	  printf("left:%llu,right:%llu\n",p->left?p->left->key:0,\
			p->right?p->right->key:0);
	
	  if(p->left) list[i++] = p->left;
	  else printf("p->left child is null\n");
	
	  if(p->right) list[i++] = p->right;
	  else printf("p->right child is null\n");
	
	 p = list[j++];
	 
	 printf("i:%llu,j:%llu\n",i,j);
	}
}

static void _rb_tree_insert_fixup(struct rb_root *root,struct rb_node *p)
{
	struct rb_node *q;

	while(p && p->parent && p->parent->color == RED)
	{
 	
	if(p->parent->parent && p->parent == p->parent->parent->left)	
	  {
		q = p->parent->parent->right;
		if(q && q->color == RED)
		{
		  p->parent->color = BLACK;
		  p->parent->parent->color = RED;
		  q->color = BLACK;
		  p = p->parent->parent;
		  continue;
		}

		if(p->parent->right && p->parent->right == p)
		 {
		      p = p->parent;
		      left_rotate(root,p);
		 }
		 
		if(p && p->parent)
		p->parent->color = BLACK;
		if(p && p->parent && p->parent->parent)
		{
		  p->parent->parent->color = RED;
		  right_rotate(root,p->parent->parent);
		}			
	 }
	else 
	{	
		if(p->parent->parent && p->parent->parent->left)
		{
		  q = p->parent->parent->left;
		  if(q && q->color == RED)
		  {
			p->parent->parent->color = RED;
			p->parent->color = BLACK;
			q->color = BLACK;
			p = p->parent->parent;
			continue;
		  }
		}

		if(p->parent->left && p->parent->left == p)
		{
			p = p->parent;
			right_rotate(root,p);
		}
	
		if(p && p->parent)
		p->parent->color = BLACK;
		if(p && p->parent && p->parent->parent)
		{
			p->parent->parent->color = RED;
			left_rotate(root,p->parent->parent);
		}
	}	
	}

	root->root->color = BLACK;
	
}

int rb_tree_insert(struct rb_root *root,struct rb_node *node)
{
	struct rb_node *p = NULL;
		
	_rb_tree_find(root,node,&p);
	
	if(!p) {
		root->root = node;
		node->color = BLACK;
		root->len++;
		return;		
	}
	
	if(p->key < node->key)
	{
	   p->right = node;
	   node->parent = p;
	}
	else {
	   p->left = node;
	   node->parent = p;
	}

	root->len ++;

	_rb_tree_insert_fixup(root,node);

	return 0;
}

static void __rb_tree_erase_fixup(struct rb_root *root,struct rb_node *parent,\
				struct rb_node *x)
{
	struct rb_node *w;

	while( x != root->root && (!x || x->color==BLACK))
	{
		if( x == parent->left)
		{
			w = parent->right;
			//case 1
			if(w->color == RED)	
			{
				w->color = BLACK;
				parent->color = RED;
				left_rotate(root,parent);
				w = parent->right;
			}
			//case 2
			if((!w->left || w->left->color == BLACK) && \
				(!w->right || w->right->color==BLACK))
			{
				w->color = RED;
				x = parent;
				parent = x->parent;
			}
			// case 3
			else 
			{
			if( !w->right || w->right->color == BLACK)
			{
				w->color = RED;
				if(w->left)
					w->left->color = BLACK;
				right_rotate(root,w);
				w = parent->right;
			}
				
			//case 4
			w->color = parent->color;
			if(w->right)
				w->right->color = BLACK;
			parent->color = BLACK;
			left_rotate(root,parent);
			x = root->root;
			break;
			}
			
		} 
		else//same as if ,left<-->right
		{
			w = parent->left;
			//case 1
			if(RED == w->color)
			{
				w->color = BLACK;
				parent->color = RED;	
				right_rotate(root,parent);
				w = parent->left;
			}
			//case 2
			if((!w->right || w->right->color == BLACK) && \
				(!w->left || w->left->color == BLACK))
			{
				w->color = RED;
				x = parent;
				parent = x->parent;
			}
			else 
			{
			if(!w->left || w->left->color == BLACK)
			{
				w->color = RED;
				if(w->right)
					w->right->color = BLACK;
				left_rotate(root,w);
				w = parent->left;
			}	
			//case 4
			w->color = parent->color;
			if(w->left)
				w->left->color = BLACK;
			parent->color = BLACK;
			right_rotate(root,parent);
			x = root->root;
			break;
			}
		}
	}
	if(x)
	x->color = BLACK;
}

/*
* 	most principle is that:
*	node is replaced by p	
*	p    is replaced by q	
*/
static int _rb_tree_erase(struct rb_root *root,struct rb_node *node)
{
	struct rb_node *p;
	struct rb_node *q;
	struct rb_node *parent;

	if(!node->left || !node->right) 
		p = node;
	else  if(rb_tree_successor(root,node,&p)<0) return -1;
	
//	printf("node(%ld) successor:%ld\n",node->key,p->key);

	if(p->left)	q = p->left;
	else		q = p->right;
	
	if(q) q->parent = p->parent;
	
	if(!p->parent) root->root = q;
	else
	if(p->parent->left == p) p->parent->left = q;
	else		p->parent->right = q;
	
	parent = p->parent;
	
	if(p != node)
	{
		//change parent
		p->parent = node->parent;
		
		if(!node->parent)  root->root  = p;
		else
		if(node->parent->left == node)	node->parent->left = p;
		else				node->parent->right = p;
		//change left child
		if(node->left && node->right != p) 
		{
			p->left = node->left;
			node->left->parent = p;
		}
		//change right child
		if(node->right && node->right != p)
		{
			p->right = node->right;
			node->right->parent = p;
		}
		//make node clean
		init_rb_node(node,node->key);	
	}
	
	root->len --;
	
	//make up color
//	if(node->color == BLACK)
//	  	__rb_tree_erase_fixup(root,parent,q);
	return 0;
}

/*
*from linux kernel
*/
int rb_tree_erase(struct rb_root *root,struct rb_node *node)
{
	struct rb_node *child,*parent;
	int color;
	
	if(!node->left)	
		child = node->right;
	else if(!node->right)	
		child = node->left;
	else
	{
		struct rb_node *old = node,*left;

		node = node->right;
		while((left = node->left) != NULL)
			node = left;
		child = node->right;
		parent = node->parent;
		color = node->color;
		
		if(child)  child->parent = parent;
		if(parent == old) {
			parent->right = child;
			parent = node;
		} else
			parent->left = child;
		
		node->parent = old->parent;
		node->color = old->color;
		node->right = old->right;
		node->left = old->left;
		
		if(old->parent)
		{
			if(old->parent->left == old)
				old->parent->left = node;
			else	old->parent->right = node;
		} else
			root->root = node;
		
		old->left->parent = node;
		if(old->right)
			old->right->parent = node;
		goto color;
	}
	parent = node->parent;
	color = node->color;

	if(child)
		child->parent = parent;
	if(parent)
	{
		if(parent->left == node)
			parent->left = child;
		else 	parent->right = child;
	} else
		root->root = child;
color:
	if(color == BLACK)
		__rb_tree_erase_fixup(root,parent,child);

	root->len --;
	
	return 0;
}
