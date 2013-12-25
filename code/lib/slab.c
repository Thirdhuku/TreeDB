
#include "slab.h"
#include "list.h"
#include "hlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <string.h>

#define N 17
#define PAGE_THRESHOLD 10
#define PAGE_FREE  4

static struct hlist_head cache_chain[N];//link all slab together

#define __lock_wait(cachep) ({                  \
  if((cachep)->flags & SLAB_NOSLEEP)            \
	pthread_spin_lock(&(cachep)->lock.spinlock);    \
  else if((cachep)->flags & SLAB_SLEEP)                     \
	sem_wait(&(cachep)->lock.sem);                          \
 else fprintf(stderr,"WARNING:SLAB LACK OF LOCK.\n");})


#define __lock_post(cachep) ({                    \
   if((cachep)->flags & SLAB_NOSLEEP)             \
     pthread_spin_unlock(&(cachep)->lock.spinlock);         \
   else if((cachep)->flags & SLAB_SLEEP)                    \
   sem_post(&(cachep)->lock.sem);                           \
   else fprintf(stderr,"WARNING:SLAB LACK OF LOCK.\n");})


/** 
 * using the hash function,see http://www.cse.yorku.ca/~oz/hash.html
 * 
 * @param name cache pointer name
 * 
 * @return hash num
 */
static u64 hash_name(const char *name)
{
  u64 hash = 0;
  int c;
  const char *p = name;
  
  while(c = *p++)
  {
    hash = c + (hash << 6) + (hash << 16) - hash;
  }
  
  return hash;
}

static void unhash_name(umem_cache_t *cachep)
{
  u64 hash;
  umem_cache_t *p;
  struct hlist_node *pos;
  
  hash = hash_name(cachep->name); 
  hlist_del(&cache_chain[hash%N],&cachep->next);
}




static int ucache_exist(const char *name)
{
  u64 hash = hash_name(name);
  umem_cache_t *cachep;
  struct hlist_node *pos;
  
  hash = hash % N;
  
  hlist_for_each_entry(cachep,pos,&cache_chain[hash],next)
  {
    if(!strcmp(name,cachep->name))
      return 1;
  }
  
  return 0;
}
/** 
 * assume lock before calling this function
 * 
 * @param cachep 
 * @param slab 
 * 
 * @return 
 */
static int release_one_page(umem_cache_t *cachep,struct slab *slab) 
{
  void *mem;
  int flags;
  
  #ifdef DEBUG
  
  fprintf(stderr,"release one page function is here\n");
  
  #endif
  if(slab->used != 0) return -EINVAL;
  
  
  cachep->nrpage --;
  list_del_element(&slab->list);
  cachep->nrfree -= cachep->nrsize;

  //mem = (void *)slab + sizeof(struct slab) - cachep->pagesize;
  mem = (void *)((ptr_t)slab & cachep->pagesize);
  //  printf("%s,mem:%x\n",__FUNCTION__,mem);
  
  #ifdef DEBUG

  fprintf(stderr,"release one page,total:%lu,object alloc:%lu,object free:%lu\n",\
          cachep->nrpage,cachep->nralloc,cachep->nrfree);
  // getchar();
  #endif

  if((flags = munmap(mem,cachep->pagesize)) < 0) {
    fprintf(stderr,"%s:munmap error:%s\n",__FUNCTION__,strerror(errno));
  }

  return flags;
}

static struct slab * get_slab(void *mem,unsigned int);


/** 
 * assume get the lock before calling the following function
 * 
 * @param cachep 
 * @param freelist 
 * 
 * @return 
 */
static int get_one_page(umem_cache_t *cachep,struct list *freelist)
{
  void *mem;
  int nrsize,i;
  struct slab *slab;

  //����ӳ��һ��ҳ��С���ڴ�
  mem = mmap(NULL,cachep->pagesize,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(mem == MAP_FAILED) { 
    fprintf(stderr,"FAIL TO GET ONE PAGE:%s\n",strerror(errno));
    return -1;
  }
  //  printf("%s,mem:%x\n",__FUNCTION__,mem);
  
  //carved up into servel piece
  nrsize = cachep->nrsize;
  //slab = (struct slab *)((ptr_t)mem |( ~(cachep->pagesize-1) >> sizeof(ptr_t) - __get_order(pagesize)) - sizeof(struct slab));
  
  slab = (struct slab *)get_slab(mem,cachep->pagesize);
  
  #ifdef DEBUG
  
  fprintf(stderr,"slab mem:%p\n",slab);
  
  #endif

  //��ʼ�����slab��list
  list_init(&slab->list);
  //��ʼ�����slab��slab_list
  list_init(&slab->slab_list);
  //��ʼ�����slab��used=0???
  slab->used = 0;

  //�����slab��nrsize��Ԫ�أ�����ʼ��ÿ����list��ÿ��Ԫ�ؼ���slab��slab_list����
  for(i = 0;i<nrsize;i++) {
    struct list *n;
    
    n = (struct list *)(mem + i * cachep->size);
    
    list_init(n);
    list_add_head(&slab->slab_list,n);
  }
  //slab��page����++
  cachep->nrpage ++;
  list_add_head(freelist,&slab->list);//��slab_listԪ���ٲ��뵽freelist
  cachep->nrfree += nrsize;

  #ifdef DEBUG
  fprintf(stderr,"DEBUG slab:%p,object size:%lu,nrsize:%u,mem:%p,new page,total:%lu\n",\
          slab,cachep->size,cachep->nrsize,mem,cachep->nrpage);
  //getchar();
  #endif

  return 0;
}

//��һ��slab���뵽ȫ�ֱ���cache_chain�в���ʼ��
static void find_umem_cache(umem_cache_t *cachep)
{
  if(!ucache_exist(cachep->name))
    hlist_add_head(&cache_chain[hash_name(cachep->name)%N],&cachep->next);
}

//�����ͳ�ʼ��һ��slab������
//@size Ϊһ��С����Ԫ�صĴ�С
//@align Ϊ�������ֽڶ��룬�ӿ��ڴ���ʣ�����Ӧ�ò��Ҫ��?
//@flags ��2��ѡ�SLAB_NOSLEEP����?SLAB_NOSLEEP����?
//@ctor ?
//@dctor ?
umem_cache_t * umem_cache_create(const char *name,size_t size,unsigned int align,unsigned int flags,
                                 void (*ctor)(void *),void (*dctor)(void *))
{
  umem_cache_t *cachep = NULL;
  
  if(!name || ucache_exist(name)) return NULL;

  //����һ��slab�������ռ��ռ�
  if(!(cachep = calloc(1,sizeof(umem_cache_t)))) return NULL;
  if(align <0 || ((align != ALIGN4) && (align != ALIGN8))) 
  { 
    free(cachep);
    return NULL;
  }

  //ÿ��slab������������������
  cachep->name = name;

  //slab��������size��align����
  if(size & (align - 1)) {
    size += align-1;
    size &= ~(align-1);
  }

  //��ʼ��slab�������ĸ����ֶ�
  cachep->size = size;//��С
  cachep->flags = flags;//���
  cachep->nractive = 0;//��Ծ��
  cachep->nrfree = 0;//���е�
  cachep->nralloc = 0;//�����
  cachep->nrpage = 0;//page
  cachep->ctor = ctor;//���캯��
  cachep->dtor = dctor;//��������
  cachep->pagesize = getpagesize();//�ڴ�page��С
  cachep->nrsize = (cachep->pagesize - sizeof(struct slab))/cachep->size;//ÿ��ҳ�ܴ���ٸ�Ԫ��

  list_init(&cachep->slabs_partial);//��ʼ���ֲ�slab
  list_init(&cachep->slabs_full);//��ʼ��full slab
  list_init(&cachep->slabs_free);//��ʼ��free slab
  
  hlist_init_node(&cachep->next);//��ʼ��slab��һ��hash��

  //���slab���Ϊ��˯�ߣ���ʹ��������
  if(flags & SLAB_NOSLEEP) 
    pthread_spin_init(&(cachep->lock.spinlock),PTHREAD_PROCESS_PRIVATE);
  else //���slab���Ϊ��˯�ߣ���ʹ���ź���
    sem_init(&(cachep->lock.sem),0,1);
  
  __lock_wait(cachep);
  get_one_page(cachep,&cachep->slabs_free);
  __lock_post(cachep);
  
  find_umem_cache(cachep);
  
  return cachep;
}
/** 
 * assume getting lock before calling the following function
 * 
 * @param cachep 
 * @param slab 
 * 
 * @return 
 */
static void * __alloc_node(umem_cache_t *cachep,struct list *slabs_list) {
  
  struct slab *slab;
  struct list *n;
  
  if(list_empty(slabs_list)) {
    fprintf(stderr,"ERROR: LIST IS EMPTY!\n");
    return NULL;
  }
  
  slab = list_entry(slabs_list,struct slab,list);
  #ifdef DEBUG

  fprintf(stderr,"DEBUG:slab:%p(__alloc_node),used:%d,slabs_list:%p\n",slab,slab->used,slabs_list);

  #endif

  if(slab->used == cachep->nrsize) return NULL;
  if(!slab->used) {
    list_move(&cachep->slabs_partial,&slab->list);
  }
  
  n = slab->slab_list.next;
  
  #ifdef DEBUG

  fprintf(stderr,"DEBUG:n:%p(__alloc_node)\n",n);
  
  #endif
 
  list_del_element(n);

  slab->used++;
  cachep->nralloc++;
  cachep->nrfree--;
  
  if(slab->used == cachep->nrsize) 
  {
    //    if(list_empty(&cachep->slabs_free) && get_one_page(cachep,&cachep->slabs_free) < 0)
    //  fprintf(stderr,"ALLOC_NODE/GET ONE PAGE ERROR\n");
    list_move(&cachep->slabs_full,&slab->list);
    
	#ifdef DEBUG
    
    fprintf(stderr,"DEBUG(_alloc_node) move cachep to slabs_full");
    
    #endif
  }
  
  #ifdef DEBUG

  fprintf(stderr,"DEBUG:before alloc_node,mem:%p\n",n);
  
  #endif

  return (void *)n;
  
}

//����Ӧ��slab�з���һ��Ԫ�ؿռ�Ľӿ�
void *umem_cache_alloc(umem_cache_t *cachep) {
  
  void *mem = NULL;
  int flags;
  
  __lock_wait(cachep);
  
  if(!cachep->nrfree && (get_one_page(cachep,&cachep->slabs_free))<0) {
    goto _last;
  }
  
  if(!list_empty(&cachep->slabs_partial)) {
    mem =  __alloc_node(cachep,cachep->slabs_partial.next);
    
	#ifdef DEBUG
    fprintf(stderr,"DEBUG:umem_cache_alloc,mem:%p,nrsize:%u in partial\n",mem,cachep->nrsize);
    #endif
    
    goto _last;
  }
  
  if(!list_empty(&cachep->slabs_free)) {
    list_move(&cachep->slabs_partial,cachep->slabs_free.next);
    mem = __alloc_node(cachep,cachep->slabs_partial.next);
    
	#ifdef DEBUG
    fprintf(stderr,"DEBUG:mem:%p(umem_alloc) in slabs_free\n",mem);
    #endif

  }
  
 _last:
  {
    __lock_post(cachep);
    if(mem) 
    {
      memset(mem,0,cachep->size);
      if( cachep->ctor) {
      cachep->ctor(mem);
    }
    }
    
  	return mem;
  }
}


static void check_free(umem_cache_t *cachep) {
  
  unsigned int nrsize = 0;
  struct slab *slab = NULL;
  
  if(cachep->nrpage < PAGE_THRESHOLD) return;
  
  #ifdef DEBUG
  
  fprintf(stderr,"check_free is here\n");
  
  #endif
  
  list_for_each_entry(slab,&cachep->slabs_free,list) {
    nrsize ++;
  }
  
  if(nrsize < PAGE_FREE) return;
  
  struct slab *n = NULL;
  list_for_each_entry_del(slab,n,&cachep->slabs_free,list) {
    
    if(nrsize <= PAGE_FREE) break;
    
    assert(slab->used == 0);
    
    release_one_page(cachep,slab);
    
    nrsize --;
    
  }

}

static inline unsigned int __get_order(unsigned int pagesize) {
  
  unsigned int order = 0;
  
  while(pagesize != 1) 
  {
    pagesize /= 2;
    order++;
  }
  
  return order;
}

//��mem����ڴ��д���һ��slab
static struct slab  * get_slab(void *mem,unsigned int pagesize) 
{
  #ifdef DEBUG
  fprintf(stderr,"slab at:%x\n",(pagesize-1));
  #endif
  //slab�Ĵ�С��ҳ��С����
  return (struct slab *)(((ptr_t)mem  | (pagesize - 1)) - sizeof(struct slab));
  //  return (struct slab *)((ptr_t)mem  | (~(pagesize - 1) >> ((sizeof(ptr_t) - __get_order(pagesize)))) - sizeof(struct slab));
}


void umem_cache_free(umem_cache_t *cp,void *mem) {
  
  struct slab *slab = get_slab(mem,cp->pagesize);
  struct list *n;
  
  if(cp->dtor && mem)
    cp->dtor(mem);
  
  memset(mem,0,cp->size);
  
  #ifdef DEBUG
    
  fprintf(stderr,"umem_cache_free,slab:%p,mem:%p(umem_cache_free),used:%u\n",slab,mem,slab->used);

  #endif

  n = (struct list *)mem;
  list_init(n);
  
  list_add_head(&slab->slab_list,n);
  
  __lock_wait(cp);
  
  cp->nrfree ++;
  cp->nralloc --;
  
  if(slab->used == 1) {
    slab->used--;
    list_move(&cp->slabs_free,&slab->list);
    check_free(cp);
  }
  else if(slab->used == cp->nrsize) {
    slab->used--;
    list_move(&cp->slabs_partial,&slab->list);
  }
  else
    slab->used--;
  
  __lock_post(cp);
}

void umem_cache_destroy(umem_cache_t *cachep) {
  
  __lock_wait(cachep);
  
  if(cachep->nralloc) 
  {
    fprintf(stderr,"ERROR:umem_cache:%s is not empty!\n",cachep->name);
  }
  else
  {
    struct slab *slab,*n;
    
    slab = NULL;
    n = NULL;
    
    list_for_each_entry_del(slab,n,&cachep->slabs_free,list) 
    {
      assert(slab->used == 0);
      release_one_page(cachep,slab);
    }
  }
  
  if(cachep->flags & SLAB_NOSLEEP) 
    pthread_spin_destroy(&(cachep->lock.spinlock));
  else 
    sem_destroy(&(cachep->lock.sem));

  //NOTICE!!:this function don't synchornize
  unhash_name(cachep);
  __lock_post(cachep);

  free(cachep);
}
