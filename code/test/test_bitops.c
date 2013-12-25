#include "bitops.h"
#include "stp_types.h"

#include <stdio.h>
#include <assert.h>

int main(int argc,char *argv[])
{
  int i;
  u32 v = 0x010;
  
  set_bit(0,&v);
  printf("after set bit 1,i:%d\n",v);
  clear_bit(4,&v);
  printf("after clear bit 4,i:%d\n",v);
  
  change_bit(4,&v);
  assert(v==17);
  
  change_bit(4,&v);
  assert(v == 1);
  
  v = 0xffffffff;
   i = find_first_zero_bit(&v,sizeof(v));
  set_bit(1,&v);
  printf("first zero bit:%d\n",i);

  i = find_next_zero_bit(v,sizeof(v),19);
  printf("after find_next_bit:%d\n",i);
  assert(10 == find_next_zero_bit(v,sizeof(v),10));
  
  return 0;
}
