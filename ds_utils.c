
//#include <string.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/vmalloc.h>

#include "ds_utils.h"


int is_bit_set(int * bitmap, int i){
    int n = i / (sizeof(int)*8);
    int offset = i % (sizeof(int)*8);
    return (bitmap[n] & (1 << offset)) != 0;
}


void ds_memcpy(void * dst,const void *src, int n){
#if 1
    memcpy(dst,src,n);
#endif
}
