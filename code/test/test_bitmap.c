#include "bitmap.h"
#include "stp_types.h"
#include "stp.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#define N 10
#define BITS (N * BITS_PER_U32)

int main_testbitmap(int argc,char *argv[]) 
{
    u32 b[N];
    int i;
    off_t off[BITS_PER_U32];
    
    bitmap_clean(b,BITS);
    memset(off,0,sizeof(off));
    
    b[0] |= ~0;
    b[1] |= ~0;
    b[2] |= ~0;
    b[2] &= 0xfffffff0;

    i = 0;
    while(i < BITS_PER_U32) 
    {
        off[i] = bitmap_find_first_zero_bit(b,11,BITS_PER_U32 * 2 - 10);
        if(!off[i]) {
            fprintf(stderr,"has no enough space to allocate.\n");
            break;
        }
        
        #ifdef DEBUG
        printf("set offset[%d]:%ld\n",i,off[i]);
        #endif
        bitmap_set(b,off[i]);
        i++;
    }
    
    i = 0;
    while(i < BITS_PER_U32) {
        bitmap_clear(b,off[i]);
        #ifdef DEBUG
        printf("clear offset:%ld\n",off[i]);
        #endif
        i++;
    }
    
    printf("sizeof node:%d\n",sizeof(struct stp_bnode_item));
    return 0;
}

static void print_stat(const struct stat *st);

int main(int argc,char *argv[]) 
{
    STP_FILE file;
    struct stat stbuf;
    u64 ino,num;
    char name[10];
    
    
    if(argc !=2 ) {
        fprintf(stderr,"usage:%s num\n",argv[0]);
        return -1;
    }
    
    num = atoi(argv[1]);
    
    memset(name,0,10);
    
    if(!(file = stp_open("stp.fs","stp.index",STP_FS_RDWR|STP_FS_CREAT)))
    {
        printf("open stp error:%s\n",stp_strerror(stp_errno));
        return -1;
    }
    
    /*
     * test b+ tree insert 
     **/
    ino = 1;
    if(stp_mkdir(file,1,"2",S_IRWXU|S_IRWXO|S_IRWXG) < 0) {
        fprintf(stderr,"mkdir error:%s\n",stp_strerror(stp_errno));
    } else 
        fprintf(stderr,"creat dir 2 successful\n");
    
    while(ino <= num) 
    {
        //printf("create file ino:%llu\n",ino);
        snprintf(name,10,"%llu",ino);
        printf("create file:%s\n",name);
        /*
        if(stp_creat(file,name,S_IRWXU|S_IRWXO|S_IRWXG) < 0) {
        //if(stp_unlink(file,"test1") < 0) {
            printf("creat file %s error:%s,errno:%d\n",name,stp_strerror(stp_errno),stp_errno);
        }
        else printf("create file %s successful\n",name);
        */
        if(stp_mkdir(file,2,name,S_IRWXU|S_IRWXO|S_IRWXG) < 0) {
            printf("mkdir %s error:%s,errno:%d\n",name,stp_strerror(stp_errno),stp_errno);
            if(stp_errno == STP_FS_ENTRY_FULL)
            {
                printf("directory entry is full,i:%llu\n",ino);
                getchar();
                break;
            }
            
        } else
            printf("mkdir %s in 2 susscessful\n",name);

        ino ++;
    }
    
    ino = 2;
    
    /*
     * test b+tree search
     **/
    if(stp_stat(file,ino,&stbuf) < 0) {
        fprintf(stderr,"stat file ino:%llu,error:%s\n",ino,stp_strerror(stp_errno));
    }
    else print_stat(&stbuf);
    
    ino = 1;
    
    if(stp_stat(file,ino,&stbuf) < 0) {
        fprintf(stderr,"stat file ino:%llu,error:%s\n",ino,stp_strerror(stp_errno));
    } else print_stat(&stbuf);
    
    /*
    if(stp_unlink(file,"1") < 0) {
        fprintf(stderr,"unlink file 1 error:%s\n",stp_strerror(stp_errno));
    } else
        printf("successful to unlink 2\n");
    */

    if(stp_readdir(file,2,NULL,0) < 0) {
        fprintf(stderr,"readdir 2 error:%s\n",stp_strerror(stp_errno));
    } else 
        printf("readdir 2 successful\n");

    if(stp_rmdir(file,2,"3",1) < 0) {
        fprintf(stderr,"rmdir 3 in 2 error:%s\n",stp_strerror(stp_errno));
    } else 
        fprintf(stderr,"rmdir 3 in 2 successful.\n");
    
    if(stp_readdir(file,2,NULL,0) < 0) {
        fprintf(stderr,"readdir 2 error:%s\n",stp_strerror(stp_errno));
    } else 
        printf("readdir 2 successful\n");
    
    //getchar();
    
    if(stp_rmdir(file,1,"3",1) < 0) {
        fprintf(stderr,"rmdir 3 in 1 error:%s\n",stp_strerror(stp_errno));
    } else 
        printf("rmdir 3 in 1successful.\n");
    
    if(stp_rmdir(file,1,"2",1) < 0) {
        fprintf(stderr,"rmdir 2 in 1 error:%s\n",stp_strerror(stp_errno));
    } else 
        printf("rmdir 2 in 1 successful.\n");
    
    dirent_t *handle;

    ino = 1;
    if(!(handle = stp_opendir(file,ino))) {
        fprintf(stderr,"open directory 1 error:%s\n",stp_strerror(stp_errno));
    }
    
    struct dirent *p;
    
    printf("readdir ino:%llu\n",ino);
    
    while((p = stp_readdir2(handle))) {
        //fprintf(stderr,"read directory error:%s\n",stp_strerror(stp_errno));
        printf("ino:%lu,off:%lu,name:%s\n",p->d_ino,p->d_off,p->d_name);
    }
    
    stp_closedir(handle);

    //getchar();
    
    ino = 2;
    /*
    if(!(handle = stp_opendir(file,ino))) {
        fprintf(stderr,"open directory 1 error:%s\n",stp_strerror(stp_errno));
    }
    
    printf("readdir ino:%llu\n",ino);
    
    while((p = stp_readdir2(handle))) {
        //fprintf(stderr,"read directory error:%s\n",stp_strerror(stp_errno));
        printf("ino:%lu,off:%lu,name:%s\n",p->d_ino,p->d_off,p->d_name);
    }
    
    stp_closedir(handle);
    */
    /*
    if(stp_readdir(file,ino,NULL,0) < 0) {
        fprintf(stderr,"readdir %llu error:%s\n",ino,stp_strerror(stp_errno));
    } else 
        fprintf(stderr,"readdir %llu successful.\n",ino);
    */

    /*
     * test destroy
     */
    stp_close(file);
    return 0;
}


static void print_stat(const struct stat *st)
{
    char buf[20];
    
    printf("ino:%lu,size:%lu\n",st->st_ino,st->st_size);
    memset(buf,0,20);
    strftime(buf,20,"%Y-%m-%d %H:%M:%S",localtime(&st->st_atime));
    printf("access time:%s\n",buf);
    memset(buf,0,20);
    strftime(buf,20,"%Y-%m-%d %H:%M:%S",localtime(&st->st_mtime));
    printf("modify time:%s\n",buf);
}
