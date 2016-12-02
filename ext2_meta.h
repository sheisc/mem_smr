#ifndef EXT2_META_H
#define EXT2_META_H
#include "ds_types.h"

#include "read_ext2.h"
#include "ds_counter.h"
#include "hash_set.h"
///////////////////////////////////////////////////////////////////////////

typedef enum{
	NO_SB0 = 0,				// super block #0 not detected yet
	SB0_DETECTED,			// super block #0 detected, but some other meta blocks detected yet
	SBs_DETECTED,			// all super blocks detected, but block group descriptors not yet
	BG_DESCs_DETECTED,		// all block group descriptors detected
	BLK_BITMAP_DETECTED,
	INO_BITMAP_DETECTED,
	INODE_TABLE_DETECTED,	// all meta blocks marked
	EXT2_DETECTED,					// 
}meta_status;


// to index the following ext2_meta_ctr.meta_blks[]
typedef enum{
	SB_IDX = 0,
	BG_DESC_IDX,
	INO_TABLE_IDX,
	INO_BMP_IDX,
	BLK_BMP_IDX,
	TMP_IDX
}meta_vect_index;


typedef struct{
	//
	meta_status status;
	ds_counter meta_rw_cnt;
	struct ext2_super_block sb0;
	//
	unsigned int len[TMP_IDX+1];
	sorted_vector * meta_blks[TMP_IDX+1];	
}ext2_meta_ctr;
///////////////////////////////////////////////////////////////////////////////////

int read_super_block(struct ext2_super_block * sbp, int sb_no);
int read_group_desc(struct ext2_group_desc * gdp,int bg_no);
int read_ext2_inode(struct ext2_inode * ip,__u32 ino);

//
void detect_ext2_fs(char * buf,block_t blk_no);
int init_meta_ctr(void);
int release_meta_ctr(void);
ext2_meta_ctr * get_ext2_meta_ctr(void);
int is_ext2_fs(void);
void print_all_meta_blocks(void);
int mark_all_free_blks(void);

//int init_nvm(void);
int release_nvm(void);
#endif // EXT2_META_H
