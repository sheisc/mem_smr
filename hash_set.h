#ifndef HASH_SET_H
#define HASH_SET_H

#include "ds_types.h"
//////////////////////////////////////////////////////////////////////////////
#define     HASH_BUCKETS_COUNT  1024
//1024
#define     DEF_CAPACITY_OF_BUCKET  8

#define		HSET_ACTION_FAIL		-1


typedef struct {
    // count of blocks in this bucket
    int blks_cnt;
    int capacity;    
    block_t * blks;
}blocks_bucket;

typedef struct {
    blocks_bucket ** buckets;
    int buckets_cnt;
    int blocks_cnt;
}hash_set;

typedef hash_set sorted_vector;

enum PRT_ORDER{
    BY_BUCKET = 1,
    BY_BLK_NO,
};

///////////////////////////////////////////////////////////////////////////////
int is_existing(sorted_vector * hset,block_t blk_no);
int search_in_sorted_vector(sorted_vector * hset,block_t blk_no);
int insert_into_sorted_vector(sorted_vector * hset,block_t blk_no);
int remove_from_sorted_vector(sorted_vector * hset,block_t blk_no);
//hash_set * create_hash_set(int buckets_cnt);
//void release_hash_set(hash_set * hset);
//void prt_hash_set(hash_set * hset, enum PRT_ORDER order);
//void prt_hash_set_by_bucket(hash_set * hset);
int get_vect_size(sorted_vector * hset);
// only one bucket 
sorted_vector * create_sorted_vector(void);
void release_sorted_vector(sorted_vector * vect);
void remove_all_elements(sorted_vector * vect);
block_t  get_vect_element(sorted_vector * vect, int pos);
void copy_all_elements(sorted_vector * dst,sorted_vector * src);
void prt_sorted_vect(sorted_vector * vect);


#endif // HASH_SET_H
