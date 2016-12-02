#ifndef SMR_H
#define SMR_H
#include "ds_types.h"
#include "hash_set.h"
#include "read_ext2.h"
#include "ds_counter.h"

////////////////////////////////////Macros & Structs//////////////////////////////////////

/********************FOR  mapping_flag***********************/
// when no mapping has been setup
#define     INVALID_MAPPING             0x00000001
// when data is in the raw SMR disk
#define     IN_SMR_DISK         0x00000002
// when data is in the Hard Disk Drive space, used as buffer for SMR
#define     IN_HDD_BUF       0x00000004

/********************FOR meta_flag**************************/
// block group descriptors
#define     META_BG_DESCRIPTORS         0x00000001
// super block
#define     META_SUPER_BLOCK            0x00000002
// block bitmap
#define     META_BLOCK_BITMAP           0x00000003
// inode bitmap
#define     META_INODE_BITMAP           0x00000004
// inode table
#define     META_INODE_TABLE            0x00000005

/*****The following metas are not supported int this version************/
// directory entry
#define     META_DENTRY                 0x00000006
// indirect block
#define     META_IND_BLOCK              0x00000007
// double indirect
#define     META_DOUBLE_IND             0x00000008
// triple indirect
#define     META_TRIPLE_IND             0x00000009

/***********************************************************/
            
#define     INVALID_PHY_BLOCK           0x00000001               
#define     VALID_PHY_BLOCK             0x00000002              
#define     FREE_PHY_BLOCK              0x00000004





// in blocks,  SMR_DISK_SIZE must be equal with(or larger than) file size of DISK_FILE_PATH
#define     SMR_DISK_SIZE       (1024*1024*1024)
// 512 * 4 * 4K = 8MB
#define     BLOCKS_PER_BAND         (512*4)
//
#define     TOTAL_SMR_BLOCKS_CNT      (SMR_DISK_SIZE/EXT2_BLOCK_SIZE)
//
#define     TOTAL_SMR_BANDS_CNT       (TOTAL_SMR_BLOCKS_CNT/BLOCKS_PER_BAND)


#define     SMR_DISK_FILE_PATH	"./image/smr_disk.img"
#define     SMR_CTRL_FILE_PATH	"./image/smr_disk_ctrl.img"


typedef struct{
    unsigned int mapping_flag:16;
	unsigned int meta_flag:16;
    block_t paddr;
}mapping_entry;

// mainly for SMR, not for HDD_BUF / NVM
typedef struct{
    //unsigned int flag;
    block_t blk_no;
    unsigned int phy_status;
}reverse_mapping_entry;



typedef struct{
    int invalid_cnt;
    int free_cnt;
    int valid_cnt;
}band_info;


typedef struct{
	// global mapping table:  NVM/SMR/HDD_BUF
	mapping_entry mapping_table[TOTAL_SMR_BLOCKS_CNT];
	// reverse mapping table for smr disk, not for NVM/HDD_BUF
	reverse_mapping_entry smr_reverse_table[TOTAL_SMR_BLOCKS_CNT];
	band_info phy_bands[TOTAL_SMR_BANDS_CNT];
	// current free block
	block_t  cur_paddr;
	ds_counter smr_cnt;
	// 
	ds_counter hdd_cnt;

	sorted_vector * fully_invalid_bands;
	// used when loading or saving
	unsigned int vec_len;

}smr_ctr_data;


////////////////////////////////////Function Declarations///////////////////////////////////

int init_smr_disk(void);
int release_smr_disk(void);
//hash_set * get_meta_hset(void);
//int get_mapping_status(block_t blk_no);
//void set_mapping_status(block_t blk_no,int status);

int has_mapping(block_t blk_no);

void setup_global_mapping_entry(block_t blk_no, unsigned int paddr, unsigned int flag);
//int in_nvm(block_t blk_no);
int in_smr_disk(block_t blk_no);
int in_hdd_buf(block_t blk_no);
int append_smr_blocks(void *buf, block_t blk_no, block_t num_in_blocks);
int write_smr_blocks(void *buf, block_t blk_no, block_t num_in_blocks);
int read_smr_blocks(void *buf, block_t blk_no, block_t num_in_blocks);
//reverse_mapping_entry * get_reverse_mapping_table(void);
mapping_entry * get_mapping_table(void);
smr_ctr_data * get_smr_ctrl_data(void);
void setup_meta_flag(block_t blk_no, unsigned int meta_flag);
unsigned int get_meta_flag(block_t blk_no);
void invalidate_mapping_entry(block_t blk_no);


int has_mapping(block_t blk_no);
void reset_all_counters(void);

int is_meta_favored(void);
int is_counter_reset(void);
int is_ext2_awared(void);

int has_meta_data(block_t blk_no);
ds_counter * get_smr_rw_counter(void);


#endif // SMR_H
