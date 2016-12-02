/**********************************************************************************************
    (1) the first write is appended to smr disk directly
    (2) the update is written to HDD_BUF

***********************************************************************************************/
#include "hdd_buf.h"
#include "read_ext2.h"
#include "hash_set.h"
#include "smr.h"
#include "ds_utils.h"
#include "ds_counter.h"
#include "hdd_io.h"
#include "smr.h"

/******************************************Macros & structs*************************************/






/******************************************Static Function Declarations*************************/

static void swap_out_coldest(void);
static int is_hdd_buf_full(void);
static block_t get_one_free_buf(hash_set * hset);
static void swap_out_coldest(void);


/*******************************************Variable Definitions********************************/

static hdd_buf_ctr_data buf_ctr;

#if 0

// hdd buffer for smr disk
static char hdd_buf[BLOCKS_OF_HDD_BUF*EXT2_BLOCK_SIZE];
// reverse mapping table, from hdd buffer address to smr disk block number
static hdd_buf_entry hdd_buf_info[BLOCKS_OF_HDD_BUF];
// to keep all the free hdd buffer blocks
static sorted_vector* free_hdd_bufs;
//
static int cold_bufs[SWAP_OUT_CNT];
static int hit_cnt;

static ds_counter rw_counter = {
    DS_ENABLE,0,0
};
#endif


/*******************************************Function Definitions********************************/
hdd_buf_ctr_data * get_hdd_buf_ctr_data(void){
	return &buf_ctr;
}

static int is_hdd_buf_full(void){
    return buf_ctr.free_hdd_bufs->blocks_cnt == 0;
}

static int get_access_cnt(block_t buf_no){
	// We could adjust the weight value of read/write here
	return (buf_ctr.reverse_table[buf_no].write_cnt 
				+ buf_ctr.reverse_table[buf_no].read_cnt );
}

hdd_buf_entry * get_hdd_buf_table(void){
    return &buf_ctr.reverse_table[0];
}

static block_t get_one_free_buf(sorted_vector * hset){
    int buf_no = 0;
    if(get_vect_size(hset) == 0){
        DS_PRINTF((PRINT_PREFIX"hset is empty."));
        return 0;
    }
	buf_no = hset->buckets[0]->blks[0];
    remove_from_sorted_vector(hset,buf_no);
    return buf_no;
}

static void reset_phy_buf_counter(block_t buf_no){
	buf_ctr.reverse_table[buf_no].write_cnt = 0;
	buf_ctr.reverse_table[buf_no].read_cnt = 0;
}

static void update_cold_buf(block_t buf_no){
    int i = 0;
    int max_pos = -1;
    int max = INT_MIN;

    // at least one element in cold_bufs[]
    for(i = 0; i < SWAP_OUT_CNT; i++){
        int cur_no = buf_ctr.cold_bufs[i];
        if(cur_no == buf_no){ // already in cold_bufs[]
            return;
        }
        if(get_access_cnt(cur_no)> max){
            max = get_access_cnt(cur_no);
            max_pos = i;
        }
    }
    if(buf_ctr.reverse_table[buf_no].write_cnt < max){
        buf_ctr.cold_bufs[max_pos] = buf_no;
    }

}


static void swap_out_coldest(void){
    int i = 0;

    for(i = 0; i < BLOCKS_OF_HDD_BUF; i++){
        if(i < SWAP_OUT_CNT){
            buf_ctr.cold_bufs[i] = i;
        }else{
            update_cold_buf(i);
        }
    }

    for(i = 0; i < SWAP_OUT_CNT; i++){

        int buf_no = buf_ctr.cold_bufs[i];
		reset_phy_buf_counter(buf_no);
        append_smr_blocks(buf_ctr.hdd_buf+EXT2_BLOCK_SIZE*buf_no,
                          buf_ctr.reverse_table[buf_no].blk_no,
                          1);
        insert_into_sorted_vector(buf_ctr.free_hdd_bufs,buf_no);
    }
}


void reclaim_free_hdd_buf(block_t blk){
	block_t buf_no;
	mapping_entry * mtable;
	if(in_hdd_buf(blk)){
		mtable = get_mapping_table();
		buf_no = mtable[blk].paddr;
		reset_phy_buf_counter(buf_no);
	    insert_into_sorted_vector(buf_ctr.free_hdd_bufs,buf_no);
		setup_global_mapping_entry(blk,0,INVALID_MAPPING);
	}
	
}

int read_block_from_hdd_buf(block_t blk_no, char * buf){
    if(in_hdd_buf(blk_no)){
        mapping_entry * mtable = get_mapping_table();
        ds_memcpy(buf,buf_ctr.hdd_buf+EXT2_BLOCK_SIZE*mtable[blk_no].paddr,EXT2_BLOCK_SIZE);
        buf_ctr.reverse_table[mtable[blk_no].paddr].read_cnt++;
		if(is_meta_favored() && has_meta_data(blk_no)){
			buf_ctr.reverse_table[mtable[blk_no].paddr].read_cnt++;
		}
        buf_ctr.rw_counter.read_count++;		
		buf_ctr.hit_cnt++;
        return 1;
    }else{
        return 0;
    }
}



int write_block_into_hdd_buf(block_t blk_no,char * buf){

    block_t hdd_buf_no = 0;
    mapping_entry * mtable = get_mapping_table();


    if(in_hdd_buf(blk_no)){
        buf_ctr.hit_cnt++;
        hdd_buf_no = mtable[blk_no].paddr;
    }else{
        if(is_hdd_buf_full()){
            swap_out_coldest();
        }
        hdd_buf_no = get_one_free_buf(buf_ctr.free_hdd_bufs);
    }
    // do the actual writing
    ds_memcpy(buf_ctr.hdd_buf+hdd_buf_no*EXT2_BLOCK_SIZE,buf,EXT2_BLOCK_SIZE);
    setup_global_mapping_entry(blk_no,hdd_buf_no,IN_HDD_BUF);
    buf_ctr.reverse_table[hdd_buf_no].blk_no = blk_no;
    buf_ctr.reverse_table[hdd_buf_no].write_cnt++;
	if(is_meta_favored() && has_meta_data(blk_no)){
		/**
			In order to keep the meta data in HDD BUF,
			 we double the write_cnt for META data to favor it by making it HOT.			 
		 */
		buf_ctr.reverse_table[hdd_buf_no].write_cnt++;
	}
    buf_ctr.rw_counter.write_count++;
	
    return 1;
}


int init_hdd_buf(void){
    block_t i = 0;
    buf_ctr.free_hdd_bufs = create_sorted_vector();
    for(i = 0; i < BLOCKS_OF_HDD_BUF; i++){
        insert_into_sorted_vector(buf_ctr.free_hdd_bufs,i);
    }
	set_counter_state(&buf_ctr.rw_counter,DS_ENABLE);
    return 0;
}
int release_hdd_buf(void){
    release_sorted_vector(buf_ctr.free_hdd_bufs);

    buf_ctr.free_hdd_bufs = NULL;
    return 0;
}

void print_hdd_buf_info(void){
    DS_PRINTF((PRINT_PREFIX"HDD_BUF:  total read %lu ; total write %lu \n",
               (unsigned long)buf_ctr.rw_counter.read_count,
               (unsigned long)buf_ctr.rw_counter.write_count));
}

ds_counter * get_buf_rw_counter(void){
    return &buf_ctr.rw_counter;
}

