#ifndef DS_TYPES_H
#define DS_TYPES_H

#include <linux/types.h>	/* size_t */

#if 0
typedef signed char     __s8;
typedef unsigned char   __u8;

typedef signed short __s16;
typedef unsigned short __u16;

typedef signed int __s32;
typedef unsigned int __u32;

typedef signed long __s64;
typedef unsigned long __u64;
#endif

typedef __u64   block_t;

typedef size_t  ds_size_t;
typedef long    ds_offset_t;

#endif // DS_TYPES_H
