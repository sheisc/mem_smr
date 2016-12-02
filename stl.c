

//#include <stdio.h>
#include "hdd_io.h"
#include "hash_set.h"
//#include <string.h>
//#include <time.h>
#include "ext2_meta.h"
#include "smr.h"
#include "ds_utils.h"
#include "read_ext2.h"
#include "hdd_buf.h"
#include "hdd_io.h"



/******************************************Macros & structs*************************************/

//#define     NVM_FILE_PATH       "../DataSplitter/img/nvm.img"
//#define     MAPPING_TABLE_PATH  "../DataSplitter/img/mapping_table.img"
//#define     REVERSE_MAPPING_TABLE_PATH      "../DataSplitter/img/reverse_mapping_table.img"



//#define     INVALID_ADDR        0xffffffff




/******************************************Static Function Declarations*************************/


static void init_global_mapping_table(void);
static void garbage_collecting(void);
static block_t get_free_smr_block(void);
static block_t do_gc_in_band(int b);
static void free_band(int b);
static void setup_smr_reverse_mapping(unsigned int paddr,block_t blk_no,unsigned int phy_status);

/*******************************************Variable Definitions********************************/


static smr_ctr_data smr_ctr;
// temporary buffer for RMW a band, no need to save/load it
static char band_buf[EXT2_BLOCK_SIZE*BLOCKS_PER_BAND];

/*******************************************Function Definitions********************************/
smr_ctr_data * get_smr_ctrl_data(void){
	return &smr_ctr;
}

ds_counter * get_smr_rw_counter(void){
	return &smr_ctr.smr_cnt;
}
int has_mapping(block_t blk_no){
	return in_smr_disk(blk_no) || in_hdd_buf(blk_no);
}

int has_meta_data(block_t blk_no){
	unsigned int meta_flag = smr_ctr.mapping_table[blk_no].meta_flag;
	// according to MACROS in smr.h
	return (meta_flag >= META_BG_DESCRIPTORS) && (meta_flag <= META_INODE_TABLE);
}


static void setup_smr_reverse_mapping(unsigned int paddr,block_t blk_no,unsigned int phy_status){
   smr_ctr.smr_reverse_table[paddr].blk_no = blk_no;
   smr_ctr.smr_reverse_table[paddr].phy_status = phy_status;
}

static void free_band(int b){
    block_t paddr = b * BLOCKS_PER_BAND;
    block_t i;
    for(i = 0; i < BLOCKS_PER_BAND; i++){
        smr_ctr.smr_reverse_table[paddr].blk_no = 0;
        smr_ctr.smr_reverse_table[paddr].phy_status = FREE_PHY_BLOCK;
        paddr++;
    }
    smr_ctr.phy_bands[b].free_cnt = BLOCKS_PER_BAND;
    smr_ctr.phy_bands[b].invalid_cnt = 0;
    smr_ctr.phy_bands[b].valid_cnt = 0;
}

static void check_band_state(int b){

    if(smr_ctr.phy_bands[b].invalid_cnt < 0 || smr_ctr.phy_bands[b].invalid_cnt > BLOCKS_PER_BAND){
        DS_PRINTF((PRINT_PREFIX"phy_bands[%d].invalid_cnt = %d \n", b,smr_ctr.phy_bands[b].invalid_cnt));
    }
    if(smr_ctr.phy_bands[b].valid_cnt < 0 || smr_ctr.phy_bands[b].valid_cnt > BLOCKS_PER_BAND){
        DS_PRINTF((PRINT_PREFIX"phy_bands[%d].valid_cnt = %d \n", b,smr_ctr.phy_bands[b].valid_cnt));
    }
    if(smr_ctr.phy_bands[b].free_cnt < 0 || smr_ctr.phy_bands[b].free_cnt > BLOCKS_PER_BAND){
        DS_PRINTF((PRINT_PREFIX"phy_bands[%d].free_cnt = %d \n", b,smr_ctr.phy_bands[b].free_cnt));
    }
    if(smr_ctr.phy_bands[b].free_cnt + smr_ctr.phy_bands[b].invalid_cnt+smr_ctr.phy_bands[b].valid_cnt != BLOCKS_PER_BAND){
        DS_PRINTF((PRINT_PREFIX"band[%d]: free_cnt(%d) + invalid_cnt(%d) + valid_cnt(%d) != %d \n",
                   b,
                   smr_ctr.phy_bands[b].free_cnt,
                   smr_ctr.phy_bands[b].invalid_cnt,
                   smr_ctr.phy_bands[b].valid_cnt,
                   BLOCKS_PER_BAND));
    }
}


// not efficient yet
static block_t get_free_smr_block(void){
    // cur_paddr always points to free block
    if( (smr_ctr.cur_paddr % BLOCKS_PER_BAND == 0) &&
        (smr_ctr.smr_reverse_table[smr_ctr.cur_paddr].phy_status != FREE_PHY_BLOCK) ){
        // reset cur_paddr in GC
        garbage_collecting();
    }

    return smr_ctr.cur_paddr++;
}

static block_t get_fully_invalid_band(void){

    if(get_vect_size(smr_ctr.fully_invalid_bands) > 0){
        block_t b = smr_ctr.fully_invalid_bands->buckets[0]->blks[0];
        remove_from_sorted_vector(smr_ctr.fully_invalid_bands,b);
        return b;
    }
    DS_PRINTF((PRINT_PREFIX"impossible ...\n"));
    return 0;
}
//
static block_t do_gc_in_band(int b){
    block_t i = 0;
    block_t paddr = b * BLOCKS_PER_BAND;
    int cnt = 0;

    int n = smr_ctr.phy_bands[b].valid_cnt;


    // read all the valid blocks in band b into buffer
    for(i = 0; i < BLOCKS_PER_BAND; i++){
        if(smr_ctr.smr_reverse_table[paddr].phy_status == VALID_PHY_BLOCK){
            block_t blk_no = smr_ctr.smr_reverse_table[paddr].blk_no;
            read_hdd_blocks(band_buf+cnt*EXT2_BLOCK_SIZE,paddr,1);
            setup_global_mapping_entry(blk_no,
                                b*BLOCKS_PER_BAND+cnt,
                                IN_SMR_DISK);
            setup_smr_reverse_mapping(b*BLOCKS_PER_BAND+cnt,blk_no,VALID_PHY_BLOCK);
            cnt++;
        }
        paddr++;
    }
    // reset the band state
    smr_ctr.phy_bands[b].valid_cnt = n;
    smr_ctr.phy_bands[b].invalid_cnt = 0;
    smr_ctr.phy_bands[b].free_cnt = BLOCKS_PER_BAND - n;
    for(i = n; i < BLOCKS_PER_BAND; i++){
        smr_ctr.smr_reverse_table[b * BLOCKS_PER_BAND+i].phy_status = FREE_PHY_BLOCK;
        smr_ctr.smr_reverse_table[b * BLOCKS_PER_BAND+i].blk_no = 0;
    }
    // write all the valid blocks back to band b
    write_hdd_blocks(band_buf,b*BLOCKS_PER_BAND,n);
    // reset cur_paddr
    return b * BLOCKS_PER_BAND + n;
}

// not efficient yet, only focus on write amplification now
static void garbage_collecting(void){
    if(get_vect_size(smr_ctr.fully_invalid_bands) > 0){
        block_t b = get_fully_invalid_band();
        free_band(b);
        //reset cur_paddr
        smr_ctr.cur_paddr = b*BLOCKS_PER_BAND;
    }else{
        //
        int max = 0;
        block_t pos = 0;
        block_t i = 0;
        // inefficient
        // find the band with most invalid blocks
        for(i = 0; i < TOTAL_SMR_BANDS_CNT; i++){
            if(smr_ctr.phy_bands[i].free_cnt > 0){
                check_band_state(i);
                DS_PRINTF((PRINT_PREFIX"phy_bands[%d].free_cnt(%d),valid_cnt(%d),invalid_cnt(%d)\n",
                           (int)i,
                           smr_ctr.phy_bands[i].free_cnt,
                           smr_ctr.phy_bands[i].valid_cnt,
                           smr_ctr.phy_bands[i].invalid_cnt));
            }
            if(smr_ctr.phy_bands[i].invalid_cnt > max){
                pos = i;
                max = smr_ctr.phy_bands[i].invalid_cnt;
            }
        }
        // reset cur_paddr
        smr_ctr.cur_paddr = do_gc_in_band(pos);
    }
}

#if 0
reverse_mapping_entry * get_reverse_mapping_table(void){
    return smr_ctr.smr_reverse_table;
}
#endif

mapping_entry * get_mapping_table(void){
    return smr_ctr.mapping_table;
}

int in_smr_disk(block_t blk_no){
    return (smr_ctr.mapping_table[blk_no].mapping_flag == IN_SMR_DISK);
}

int in_hdd_buf(block_t blk_no){
    return (smr_ctr.mapping_table[blk_no].mapping_flag == IN_HDD_BUF);
}


void invalidate_mapping_entry(block_t blk_no){
    if(in_smr_disk(blk_no)){
        block_t paddr = smr_ctr.mapping_table[blk_no].paddr;
        block_t b = paddr / BLOCKS_PER_BAND;

        smr_ctr.phy_bands[b].invalid_cnt++;
        smr_ctr.phy_bands[b].valid_cnt--;
        setup_smr_reverse_mapping(paddr,0,INVALID_PHY_BLOCK);
        setup_global_mapping_entry(blk_no,0,INVALID_MAPPING);
        if(smr_ctr.phy_bands[b].invalid_cnt == BLOCKS_PER_BAND){
			//DS_PRINTF((PRINT_PREFIX".............band %u is fully invalid.........\n",(unsigned int) b));
            insert_into_sorted_vector(smr_ctr.fully_invalid_bands,b);
        }
    }
}


/**
 * @brief setup_mapping_entry
 *          setup the mapping table entry and the reverse mapping table entry
 * (1)when blk_no is mapped to NVM, @paddr is the inner address of NVM
 * (2)when blk_no is mapped to HDD_BUF, @paddr is the inner address of HDD BUFFER
 * (3)when blk_no is mapped to SMR DISK,@paddr is the inner address of SMR DISK
 * @param blk_no        logical block number
 * @param paddr         the corresponding inner physical address
 * @param flag          information about the block
 */
void setup_global_mapping_entry(block_t blk_no, unsigned int paddr, 
			unsigned int mapping_flag){
    //
    smr_ctr.mapping_table[blk_no].paddr = paddr;
    smr_ctr.mapping_table[blk_no].mapping_flag = mapping_flag;
	//smr_ctr.mapping_table[blk_no].meta_flag = meta_flag;
}

void setup_meta_flag(block_t blk_no, unsigned int meta_flag){
	smr_ctr.mapping_table[blk_no].meta_flag = meta_flag;
}

unsigned int get_meta_flag(block_t blk_no){
	return smr_ctr.mapping_table[blk_no].meta_flag;
}

static void init_global_mapping_table(void){
    block_t i;
    for(i = 0; i < (TOTAL_SMR_BLOCKS_CNT); i++){
        // no mapping entry [vir,phy] at first
        //smr_ctr.mapping_table[i].meta_flag = 0;
        smr_ctr.mapping_table[i].mapping_flag = INVALID_MAPPING;
        smr_ctr.mapping_table[i].paddr = 0;
    }
}

static void init_smr_reverse_table(void){
    block_t i;
    for(i = 0; i < TOTAL_SMR_BANDS_CNT; i++){
        free_band(i);
    }
}

/**
 * @brief ds_init_io
 * (1) create mapping table and reverse mapping talbe
 * (2) cretae NVM buffer
 * (3) load kernel part of file system META DATA into NVM by preload_medadata()
 * (4) in fact, we only have to load META DATA only once in real device.
 *     So we could ignore the overhead. In other words, we enable COUNTER after
 *     calling preload_medadata();
 *
 * @return
 */
int init_smr_disk(void){
    init_hdd_io();
    // use hash set as a vector, the parameter should be 1,
    smr_ctr.fully_invalid_bands = create_sorted_vector();
    //
    init_global_mapping_table();
	init_smr_reverse_table();

#if 1	
    init_meta_ctr();
#endif
    init_hdd_buf();
    //
    set_counter_state(&smr_ctr.smr_cnt,DS_ENABLE);
	set_counter_state(&smr_ctr.hdd_cnt,DS_ENABLE);

	DS_PRINTF((PRINT_PREFIX"int init_smr_disk(void) \n"));
    return 0;
}

void reset_all_counters(void){
	ds_counter * hdd_cnt_ptr = get_hdd_rw_counter();
	ds_counter * buf_cnt_ptr = get_buf_rw_counter();
	hdd_buf_ctr_data * buf_ctr = get_hdd_buf_ctr_data();

	hdd_cnt_ptr->read_count = 0;
	hdd_cnt_ptr->write_count = 0;

	buf_cnt_ptr->read_count = 0;
	buf_cnt_ptr->write_count = 0;

	smr_ctr.smr_cnt.write_count = 0;
	smr_ctr.smr_cnt.read_count = 0;

	buf_ctr->hit_cnt = 0;
}

static void print_rw_info(void){
	ds_counter * hdd_cnt_ptr = get_hdd_rw_counter();
	ds_counter * buf_cnt_ptr = get_buf_rw_counter();
	//ds_counter * nvm_cnt_ptr = get_nvm_rw_counter();
	DS_PRINTF((PRINT_PREFIX"SMR: write(%lu),read(%lu)\n",
		smr_ctr.smr_cnt.write_count,smr_ctr.smr_cnt.read_count));
#if 0	
	DS_PRINTF((PRINT_PREFIX"NVM: write(%lu),read(%lu)\n",
		nvm_cnt_ptr->write_count,nvm_cnt_ptr->read_count));
#endif
	DS_PRINTF((PRINT_PREFIX"HDD: write(%lu),read(%lu)\n",
		hdd_cnt_ptr->write_count,hdd_cnt_ptr->read_count));
	DS_PRINTF((PRINT_PREFIX"BUF: write(%lu),read(%lu)\n",
		buf_cnt_ptr->write_count,buf_cnt_ptr->read_count));
	DS_PRINTF((PRINT_PREFIX"Amplified write_read(%lu) : SMR write(%lu)\n",
					hdd_cnt_ptr->read_count + hdd_cnt_ptr->write_count 
								+ buf_cnt_ptr->read_count + buf_cnt_ptr->write_count 
									- smr_ctr.smr_cnt.read_count,
					smr_ctr.smr_cnt.write_count));
	DS_PRINTF((PRINT_PREFIX"BUF hit count = %lu",
		(unsigned long) get_hdd_buf_ctr_data()->hit_cnt));
}

/**
 * @brief ds_release_io
 *      release mapping table, reverse mapping table and NVM.
 * @return
 */
int release_smr_disk(void){
	print_rw_info();


	
    release_hdd_io();
    release_sorted_vector(smr_ctr.fully_invalid_bands);
    release_hdd_buf();
#if 1	
	release_meta_ctr();
#endif
	DS_PRINTF((PRINT_PREFIX"int release_smr_disk(void) \n"));
    return 0;
}



int append_smr_blocks(void *buf, block_t blk_no, block_t num_in_blocks){
    block_t i;

    for(i = 0; i < num_in_blocks; i++){
        block_t paddr = get_free_smr_block();
        setup_global_mapping_entry(i+blk_no,paddr,IN_SMR_DISK);
        setup_smr_reverse_mapping(paddr,i+blk_no,VALID_PHY_BLOCK);
        smr_ctr.phy_bands[paddr/BLOCKS_PER_BAND].free_cnt--;
        smr_ctr.phy_bands[paddr/BLOCKS_PER_BAND].valid_cnt++;
        // to be optimized
#if 1
        write_hdd_blocks(buf+i*EXT2_BLOCK_SIZE,paddr,1);
		
#endif
    }
    return 0;
}


int write_smr_blocks(void *buf, block_t blk_no, block_t num_in_blocks){
    block_t i;
#if 0	
	DS_PRINTF((PRINT_PREFIX"write blocks [0x%lx - 0x%lx] \n",(unsigned long)blk_no, 
								(unsigned long)(blk_no + num_in_blocks - 1)));
#endif
    for(i = 0; i < num_in_blocks; i++){
		if(is_ext2_awared()){
			detect_ext2_fs(buf,blk_no);		
		}
        if(in_hdd_buf(blk_no)){//  2nd update, ....
        	//DS_PRINTF((PRINT_PREFIX"in BUF..........\n"));
            write_block_into_hdd_buf(blk_no,buf);
        }else if(in_smr_disk(blk_no)){// update
        	//DS_PRINTF((PRINT_PREFIX"in SMR DISK..........\n"));
            invalidate_mapping_entry(blk_no);
            write_block_into_hdd_buf(blk_no,buf);
        }else{
        	//DS_PRINTF((PRINT_PREFIX"APPEND WRITE..........\n"));
            append_smr_blocks(buf,blk_no,1);
        }
        buf += EXT2_BLOCK_SIZE;
        blk_no++;
    }
	//smr_ctr.smr_cnt.write_count += num_in_blocks;
    return 0;
}

int read_smr_blocks(void *buf, block_t blk_no, block_t num_in_blocks){
    block_t i ;
	unsigned long no_map = 0;
    for(i = 0; i < num_in_blocks; i++){
		//DS_PRINTF((PRINT_PREFIX"read_smr_blocks() %x..........\n",(unsigned int) blk_no));
        if(in_hdd_buf(blk_no)){
        	//DS_PRINTF((PRINT_PREFIX"IN BUF()\n"));
            read_block_from_hdd_buf(blk_no,buf);
        }else if(in_smr_disk(blk_no)){
        	//DS_PRINTF((PRINT_PREFIX"IN SMR DISK()\n"));
            read_hdd_blocks(buf, smr_ctr.mapping_table[blk_no].paddr,1);
        }else{ // no mapping
        	/**
	        		It seems that 'mkfs' will check whether there is already an EXT file system.
        		 */
            //DS_PRINTF((PRINT_PREFIX"no mapping entry for %llu \n",blk_no));
			//read_hdd_blocks(buf, blk_no,1);
			no_map++;
        }
        buf += EXT2_BLOCK_SIZE;
        blk_no++;
    }
	//smr_ctr.smr_cnt.read_count += num_in_blocks;
	// for consistency, in fact no acutal read when NO MAPPING
	smr_ctr.hdd_cnt.read_count += no_map;
    return i;
}

