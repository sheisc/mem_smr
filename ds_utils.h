#ifndef DS_UTILS_H
#define DS_UTILS_H
//#include <stdio.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/vmalloc.h>


#define DS_ENABLE_DEBUG
#ifdef  DS_ENABLE_DEBUG
    //#define DS_PRINTF(args)   do{printk("%s %d:",__FILE__,__LINE__);printk args ;}while(0)
    #define DS_PRINTF(args)   do{printk args ;}while(0)
#else
    #define DS_PRINTF(args)
#endif

#define		PRINT_PREFIX		KERN_NOTICE"iron5:"
#define PRT_INT_FIELD(ptr,name)  DS_PRINTF((PRINT_PREFIX #name " = 0x%x \n",ptr->name))
#define PRT_STR_FIELD(ptr,name)  DS_PRINTF((PRINT_PREFIX #name " = %s \n",ptr->name))

#if 0
#define     ALLOC_MEM       malloc
#define     FREE_MEM        free
#endif
#define     ALLOC_MEM       vmalloc
#define     FREE_MEM        vfree


void ds_memcpy(void * dst,const void *src, int n);

int is_bit_set(int * bitmap, int i);

#endif // DS_UTILS_H
