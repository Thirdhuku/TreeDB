#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct foo {
  int i;
  int j;
};

struct bar {
  int i;
  struct foo f;
  int j;
};

struct fb {
  struct bar *b;
};

  

int main(int argc,char *argv[])
{
    void * addr;
    int ffd,flag;
    struct stat stbuf;
    struct fb *fb = calloc(1,sizeof(struct fb));
    
    flag = 0;
    if(stat("test_mmap.fs",&stbuf)<0) 
    {
      flag = 1;
    }
    
        
    if((ffd = open("test_mmap.fs",O_RDWR|O_CREAT,S_IRWXU|S_IRGRP|S_IROTH)) < 0)  {
      	fprintf(stderr,"oepn file error:%s\n",strerror(errno));
      	return -1;
    }
    
    ftruncate(ffd,4*1024);
    if((addr = mmap(NULL,4*1024,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_LOCKED,ffd,0)) == MAP_FAILED) {
      	fprintf(stderr,"mmap error:%s\n",strerror(errno));
      	return -1;	
    }
    
    fb->b = (struct bar *)addr;
    printf("f:%p\n",fb->b);
    if(flag) {
      	memset(fb->b,0,sizeof(*(fb->b)));
      
    	fb->b->i = 4;
    	fb->b->f.i = 3;
    	fb->b->f.j = 3;
    	fb->b->j = 4;
    } else {
      	printf("f->i:%d,f->f.i:%d,f->f.j:%d,f->j:%d\n",fb->b->i,fb->b->f.i,fb->b->f.j,fb->b->j);
    }
    
    
    munmap(fb->b,4*1024);
    close(ffd);
    free(fb);
    
  	return 0;
}
