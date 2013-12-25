#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

char str[] = "hello,world!\n";

int main(void) 
{
    int fd;
    char *addr;
    
    if((fd = open("test.txt",O_CREAT|O_RDWR,S_IRWXU|S_IRGRP|S_IROTH)) < 0) {
        printf("open test.txt error!\n");
        return -1;
    }
    ftruncate(fd,30);
    
    if((addr = (char*)mmap(NULL,30,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0)) == MAP_FAILED) {
        printf("mmap error!\n");
        return -1;
    }
    
    memset(addr,'\0',30);
    strcpy(addr,"hello");
    printf("addr:%s\n",addr);
    
    //fsync(fd);
    close(fd);
    

    return 0;
}
