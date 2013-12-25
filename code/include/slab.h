#ifndef __SLAB_H__
#define __SLAB_H__

#include "stp_types.h"
#include "list.h"
#include "hlist.h"


#include <pthread.h>
#include <semaphore.h>

//unit of slab/page
struct slab {
  struct list list;//for cachep
  struct list slab_list;//for allocated page object
  int used;
};

  
//may have some synchronous problem
typedef struct umem_cache {
  
  const char *name;//slab name
  unsigned long size;//object size,object size aligned(unused)
  unsigned int flags;
  unsigned int nrsize;//one page contain number of pages
  
  //statistics
  unsigned long nractive;
  unsigned long nrfree;
  unsigned long nralloc;
  unsigned long nrpage;//total page consumed
  unsigned long pagesize;
  
  
  //constructor and destructor function
  void (*ctor)(void *);
  void (*dtor)(void *);
  
  struct list slabs_partial;
  struct list slabs_full;
  struct list slabs_free;
  
  struct hlist_node next;//link all cache together
  //synchronous variable,see flags for different mechanism
  union {
    sem_t sem;
    pthread_spinlock_t spinlock;
  } lock;
  
}umem_cache_t;

#define ALIGN4  4
#define ALIGN8  8

//#define ALIGN16 16

#define SLAB_NOSLEEP  (1<<2)
#define SLAB_SLEEP    (1<<1)

//may be some useful argument(experimental)
umem_cache_t* umem_cache_create(const char *name,size_t size,unsigned int align,unsigned int flags,
                                void (*ctor)(void *),void (*dctor)(void *));
void *umem_cache_alloc(umem_cache_t *cp);
void umem_cache_free(umem_cache_t *cp,void *buf);
//All object must have been returned to the cache,when the cp is destroyed. 
void umem_cache_destroy(umem_cache_t *cp);

#endif
