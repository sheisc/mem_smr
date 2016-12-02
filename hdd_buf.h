#ifndef HDD_BUF_H
#define HDD_BUF_H
#include "ds_counter.h"
#include "ds_types.h"
#include "hash_set.h"
#include "read_ext2.h"
/******************************************Macros**********************************************/


// 16K * 4K = 64MB
#define    BLOCKS_OF_HDD_BUF        (16*1024)
// SWAP_OUT_CNT must be smaller than BLOCKS_OF_HDD_BUF
#define    SWAP_OUT_CNT              128
/******************************************structs*********************************************/



typedef struct{
    block_t blk_no;     // logical smr block number
    int read_cnt;
    int write_cnt;
}hdd_buf_entry;




typedef struct{
	char hdd_buf[BLOCKS_OF_HDD_BUF*EXT2_BLOCK_SIZE];
	// reverse mapping table, from hdd buffer address to smr disk block number
	hdd_buf_entry reverse_table[BLOCKS_OF_HDD_BUF];
	int cold_bufs[SWAP_OUT_CNT];
	int hit_cnt;	
	ds_counter rw_counter;
	// to keep all the free hdd buffer blocks
	sorted_vector* free_hdd_bufs;	
	// used when loading /saving
	unsigned int vect_len;
}hdd_buf_ctr_data;

/*******************************************Global Variable Declarations************************/









/*******************************************Global Function Declarations************************/
int read_block_from_hdd_buf(block_t blk_no, char * buf);
int write_block_into_hdd_buf(block_t blk_no,char * buf);
int init_hdd_buf(void);
int release_hdd_buf(void);
void print_hdd_buf_info(void);
int init_hdd_buf(void);
int release_hdd_buf(void);
// for test
void test_hdd_buf(void);
hdd_buf_entry * get_hdd_buf_table(void);
void reclaim_free_hdd_buf(block_t blk);
ds_counter * get_buf_rw_counter(void);
hdd_buf_ctr_data * get_hdd_buf_ctr_data(void);

#endif // HDD_BUF_H
