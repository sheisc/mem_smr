

#include "hdd_io.h"
#include "hash_set.h"
#include "ext2_meta.h"
#include "read_ext2.h"
#include "smr.h"
#include "ds_utils.h"
#include "hdd_buf.h"
///////////////////////////////////////////////////////////////////////////


#define		EXT2_MAGIC_NUMBER 		0xEF53
typedef void (* DETECTOR)(char * buf,block_t blk_no);
// (SB_IDX,META_SUPER_BLOCK)
typedef struct{
	meta_vect_index idx;
	unsigned int meta_flag;
	char * name;
}idx_2_flag_entry;

// for  void mark_meta_blks(int action)
#define		CLEAN_META_FLAG			1
#define		SET_META_FLAG			2

#define		GET_BIT(val, n)		((val)&(1<<(n))) 
///////////////////////////////////////////////////////////////////////////

static void  detect_SB0_if_NO_SB0(char * buf,block_t blk_no);
static void  detect_SBs_if_SB0(char * buf,block_t blk_no);
static void  detect_BG_DESCs_if_SBs(char * buf,block_t blk_no);
static void  detect_BLK_BMP_if_BG_DESCs(char * buf,block_t blk_no);
static void  detect_INO_BMP_if_BLK_BMP(char * buf,block_t blk_no);
static void  detect_INO_TABLE_if_INO_BMP(char * buf,block_t blk_no);


static void do_group_descriptor(struct ext2_group_desc * gdp);

static int is_legal_super_block(struct ext2_super_block * sbp);
static int get_number_of_BG_DESCs(block_t blk_no);

static int  mark_one_blk_bmp(int * new_ptr,block_t from);
static void mark_meta_blks(int action);

///////////////////////////////////////////////////////////////////////////


static ext2_meta_ctr meta_ctr = {
	NO_SB0
};


static DETECTOR dectors[] = {
	detect_SB0_if_NO_SB0,
	detect_SBs_if_SB0,
	detect_BG_DESCs_if_SBs,
	detect_BLK_BMP_if_BG_DESCs,
	detect_INO_BMP_if_BLK_BMP,
	detect_INO_TABLE_if_INO_BMP
};

static idx_2_flag_entry idx_2_flag[] = {
	{SB_IDX,META_SUPER_BLOCK,"SUPER BLOCK"},
	{BG_DESC_IDX,META_BG_DESCRIPTORS,"BLOCK GROUP DESCRIPTORS"},
	{INO_TABLE_IDX,META_INODE_TABLE,"INODE TABLE"},
	{INO_BMP_IDX,META_INODE_BITMAP,"INODE BITMAP"},
	{BLK_BMP_IDX,META_BLOCK_BITMAP,"BLOCK BITMAP"},
};


///////////////////////////////////////////////////////////////////////////

static int all_elements_mapped(sorted_vector * vect){
	int len;
	int i;
	block_t blk_no;
	
	len = get_vect_size(vect);
	for(i = 0; i < len; i++){
		blk_no = get_vect_element(vect,i);
		if(!has_mapping(blk_no)){
			return 0;
		}
	}
	return 1;
}


// try to detect SB0
static void  detect_SB0_if_NO_SB0(char * buf,block_t blk_no){	
	static int bg_no[3] = {3,5,7};	
	struct ext2_super_block * sbp;
    int tmp[3] = {3,5,7};
	int i;
	block_t target;
	
	if(blk_no == SB0_BLOCK_NO){
		sbp = (struct ext2_super_block *)(buf + FIRST_SUPER_BLOCK_OFFSET);
		if(is_legal_super_block(sbp)){
			int gd_cnt;
			int overflow;
			meta_ctr.sb0 = *sbp;
			meta_ctr.status = SB0_DETECTED;
			DS_PRINTF((PRINT_PREFIX"meta_ctr.status = SB0_DETECTED; \n"));
			//init the vectors
			for(i = SB_IDX; i <= TMP_IDX; i++){
				remove_all_elements(meta_ctr.meta_blks[i]);
			}
			//
			insert_into_sorted_vector(meta_ctr.meta_blks[SB_IDX],SB0_BLOCK_NO);
			gd_cnt = (sbp->s_blocks_count-sbp->s_first_data_block - 1) / sbp->s_blocks_per_group + 1;
			DS_PRINTF((PRINT_PREFIX"gd_cnt = %d \n", gd_cnt));
			if(gd_cnt > 1){					
				// block group #1				
				target = sbp->s_first_data_block + 1*sbp->s_blocks_per_group;			
				insert_into_sorted_vector(meta_ctr.meta_blks[TMP_IDX],target);
				insert_into_sorted_vector(meta_ctr.meta_blks[SB_IDX],target);
				
				// block group #3, #5, #7 , ....
				overflow = 0;
				while(!overflow){					
					// {3,5,7,  9,25,49,    ...}
					for(i = 0; i < 3; i++){
						if(tmp[i] >= gd_cnt){
					    	overflow = 1;
					        break;
						}else{
							target = sbp->s_first_data_block + tmp[i]*sbp->s_blocks_per_group;
							insert_into_sorted_vector(meta_ctr.meta_blks[TMP_IDX],target);
							insert_into_sorted_vector(meta_ctr.meta_blks[SB_IDX],target);
					        tmp[i] *= bg_no[i];
							//DS_PRINTF((PRINT_PREFIX"target = 0x%lx \n", (unsigned long)target));
						}
					}
				}
				
			} 
			//DS_PRINTF((PRINT_PREFIX"SUPER BLOCK: \n"));
			//prt_sorted_vect(meta_ctr.meta_blks[SB_IDX]);
			detect_SBs_if_SB0(buf,blk_no);
		}
	}
}
// try to detect other SBs
static void  detect_SBs_if_SB0(char * buf,block_t blk_no){
	static char data_buf[EXT2_BLOCK_SIZE];
	
	int pos, len, n,gd_cnt,i;
	// 
	if(all_elements_mapped(meta_ctr.meta_blks[TMP_IDX])){
		len = get_vect_size(meta_ctr.meta_blks[TMP_IDX]);
		for(i = 0; i < len; i++){
			read_smr_blocks(data_buf,get_vect_element(meta_ctr.meta_blks[TMP_IDX],i),1);
			if(!is_legal_super_block((struct ext2_super_block *) data_buf)){
				// Back to the start point
				meta_ctr.status = NO_SB0;	
				return;
			}
		}			
		remove_all_elements(meta_ctr.meta_blks[TMP_IDX]);
		// all super blocks have been checked, switch to next state		
		meta_ctr.status = SBs_DETECTED;
		DS_PRINTF((PRINT_PREFIX"meta_ctr.status = SBs_DETECTED; \n"));
		gd_cnt = (meta_ctr.sb0.s_blocks_count - meta_ctr.sb0.s_first_data_block - 1)
						/ meta_ctr.sb0.s_blocks_per_group + 1;
		n = (gd_cnt*sizeof(struct ext2_group_desc)+(EXT2_BLOCK_SIZE-1))/EXT2_BLOCK_SIZE;		
		len = get_vect_size(meta_ctr.meta_blks[SB_IDX]);
		// for each super block
		for(pos = 0; pos < len; pos++){
			block_t sb = get_vect_element(meta_ctr.meta_blks[SB_IDX],pos);
			// the following n blocks are BG_DESCs
			for(i = 0; i < n;i++){
				insert_into_sorted_vector(meta_ctr.meta_blks[BG_DESC_IDX],sb+1+i);
				// we only detect the GroupDescs after SB0
				if(sb == SB0_BLOCK_NO){
					insert_into_sorted_vector(meta_ctr.meta_blks[TMP_IDX],sb+1+i);				
				}
			}
		}
		//prt_sorted_vect(meta_ctr.meta_blks[SB_IDX]);
		detect_BG_DESCs_if_SBs(buf,blk_no);
	}	
}
/**
	Calculate the number of block group descriptors in block @blk_no.
	
	 ONLY called in detect_BG_DESCs_if_SBs()
	 Parameter @blk_no MUST BE in meta_ctr.meta_blks[BG_DESC_IDX]
	 If we only detect BG_DESCs after SB0, we don't need this function at all.
	 Buf if we want to detect all the BG_DESCs, this function will be useful.
 */
static int get_number_of_BG_DESCs(block_t blk_no){
	int offset,gd_cnt,blks;
	int descs_per_block;
	gd_cnt = (meta_ctr.sb0.s_blocks_count - meta_ctr.sb0.s_first_data_block - 1)
						/ meta_ctr.sb0.s_blocks_per_group + 1;
	blks = (gd_cnt*sizeof(struct ext2_group_desc)+(EXT2_BLOCK_SIZE-1))/EXT2_BLOCK_SIZE;
	descs_per_block = EXT2_BLOCK_SIZE/(sizeof(struct ext2_group_desc));
	if(blks <= 1){
		return gd_cnt;
	}else{
		offset = 1;
		while(!is_existing(meta_ctr.meta_blks[SB_IDX],blk_no-offset)){
			offset++;
		}
		if(offset == blks){// the last one
			return gd_cnt % descs_per_block;
		}else{
			return descs_per_block;
		}
	}
}


/**
	insert the block number of bitmaps and inode tables into respective vectors.
 */
static void do_group_descriptor(struct ext2_group_desc * gdp){
	int blks,i;
	blks = (meta_ctr.sb0.s_inodes_per_group * sizeof(struct ext2_inode) - 1)/EXT2_BLOCK_SIZE+1;
	insert_into_sorted_vector(meta_ctr.meta_blks[BLK_BMP_IDX],gdp->bg_block_bitmap);
	insert_into_sorted_vector(meta_ctr.meta_blks[INO_BMP_IDX],gdp->bg_inode_bitmap);
	for(i = 0; i < blks; i++){
		insert_into_sorted_vector(meta_ctr.meta_blks[INO_TABLE_IDX],gdp->bg_inode_table+i);
	}
}

// try to detect block group descriptors
static void  detect_BG_DESCs_if_SBs(char * buf,block_t blk_no){
	static char data_buf[EXT2_BLOCK_SIZE];
	
	int i,n,k,len;
	struct ext2_group_desc * gdp;		
	//
	if(all_elements_mapped(meta_ctr.meta_blks[TMP_IDX])){
		len = get_vect_size(meta_ctr.meta_blks[TMP_IDX]);
		for(i = 0; i < len; i++){
			block_t blk = get_vect_element(meta_ctr.meta_blks[TMP_IDX],i);
			read_smr_blocks(data_buf,blk,1);
			gdp = (struct ext2_group_desc *) data_buf;
			n = get_number_of_BG_DESCs(blk);
			for(k = 0; k < n; k++){
				do_group_descriptor(gdp);
				gdp++;				
			}
		}			
		remove_all_elements(meta_ctr.meta_blks[TMP_IDX]);
		meta_ctr.status = BG_DESCs_DETECTED;
		DS_PRINTF((PRINT_PREFIX"meta_ctr.status = BG_DESCs_DETECTED; \n"));
		copy_all_elements(meta_ctr.meta_blks[TMP_IDX],meta_ctr.meta_blks[BLK_BMP_IDX]);
		detect_BLK_BMP_if_BG_DESCs(buf,blk_no);
	}	
}

// try to detect block bitmaps
static void  detect_BLK_BMP_if_BG_DESCs(char * buf,block_t blk_no){
	if(all_elements_mapped(meta_ctr.meta_blks[TMP_IDX])){
		// we could do stronger checking here.  Ignore it just for simplicity
		remove_all_elements(meta_ctr.meta_blks[TMP_IDX]);
		meta_ctr.status = BLK_BITMAP_DETECTED;
		DS_PRINTF((PRINT_PREFIX"meta_ctr.status = BLK_BITMAP_DETECTED; \n"));
		copy_all_elements(meta_ctr.meta_blks[TMP_IDX],meta_ctr.meta_blks[INO_BMP_IDX]);
		detect_INO_BMP_if_BLK_BMP(buf,blk_no);
	}
}
// try to detect inode bitmaps
static void  detect_INO_BMP_if_BLK_BMP(char * buf,block_t blk_no){
	if(all_elements_mapped(meta_ctr.meta_blks[TMP_IDX])){
		// we could do stronger checking here.  Ignore it just for simplicity
		remove_all_elements(meta_ctr.meta_blks[TMP_IDX]);
		meta_ctr.status = INO_BITMAP_DETECTED;
		DS_PRINTF((PRINT_PREFIX"meta_ctr.status = INO_BITMAP_DETECTED; \n"));
		copy_all_elements(meta_ctr.meta_blks[TMP_IDX],meta_ctr.meta_blks[INO_TABLE_IDX]);
		detect_INO_TABLE_if_INO_BMP(buf,blk_no);
	}
}
// try to detect inode tables
static void  detect_INO_TABLE_if_INO_BMP(char * buf,block_t blk_no){
	if(all_elements_mapped(meta_ctr.meta_blks[TMP_IDX])){
		// we could do stronger checking here.  Ignore it just for simplicity
		remove_all_elements(meta_ctr.meta_blks[TMP_IDX]);
		meta_ctr.status = EXT2_DETECTED;
		DS_PRINTF((PRINT_PREFIX"meta_ctr.status = EXT2_DETECTED; \n"));
		mark_meta_blks(SET_META_FLAG);
		mark_all_free_blks();
	}
}


int read_super_block(struct ext2_super_block * sbp, int pos){
	static char sb_buf[EXT2_BLOCK_SIZE];
	int len;
	block_t blk_no;
	len = get_vect_size(meta_ctr.meta_blks[SB_IDX]);
	if(meta_ctr.status != EXT2_DETECTED){
		return -1;
	}else if(pos < 0 || pos >= len){
		return -1;		
	}else{
		blk_no = get_vect_element(meta_ctr.meta_blks[SB_IDX],pos);
		read_smr_blocks(sb_buf,blk_no,1);
		if(blk_no == SB0_BLOCK_NO){
			*sbp = *((struct ext2_super_block *)(sb_buf + FIRST_SUPER_BLOCK_OFFSET));			
		}else{
			*sbp = *((struct ext2_super_block *)(sb_buf));
		}
		return 0;
	}
	
}
// bg_no is in [0 ,....)
int read_group_desc(struct ext2_group_desc * gdp,int bg_no){
	static char gd_buf[EXT2_BLOCK_SIZE];

	unsigned int blk_offset;
	unsigned int descs_per_block;
	struct ext2_group_desc * src;

	if(meta_ctr.status != EXT2_DETECTED){
		DS_PRINTF((PRINT_PREFIX"(meta_ctr.status != EXT2_DETECTED)"));
		return -1;
	}
	blk_offset = ((bg_no+1)*sizeof(struct ext2_group_desc)+(EXT2_BLOCK_SIZE-1))/EXT2_BLOCK_SIZE;
	descs_per_block = EXT2_BLOCK_SIZE/(sizeof(struct ext2_group_desc));	
	read_smr_blocks(gd_buf,SB0_BLOCK_NO+blk_offset,1);
	src = (struct ext2_group_desc * ) gd_buf;
	src += (bg_no % descs_per_block);
	*gdp = *src;
	return 0;
}

// ino is in [1, ...)
int read_ext2_inode(struct ext2_inode * ip,__u32 ino){
	static struct ext2_group_desc grp_desc;
	static char inode_buf[EXT2_BLOCK_SIZE];

	unsigned int bg_no;
	unsigned int pos;
	block_t blk_no;
	unsigned int inodes_per_block;
	struct ext2_inode * sip;
	
	//DS_PRINTF((PRINT_PREFIX"read_ext2_inode( %u) \n",ino));
	if(meta_ctr.status != EXT2_DETECTED){
		DS_PRINTF((PRINT_PREFIX"(meta_ctr.status != EXT2_DETECTED) \n"));
		return -1;
	}
	inodes_per_block = EXT2_BLOCK_SIZE/(sizeof(struct ext2_inode));
	//DS_PRINTF((PRINT_PREFIX"inodes per block is %u \n",inodes_per_block));
    bg_no = (ino-1) / meta_ctr.sb0.s_inodes_per_group;	
	read_group_desc(&grp_desc,bg_no);
	// pos is in [0 ,...)
    pos = (ino-1) %  meta_ctr.sb0.s_inodes_per_group;

	blk_no = grp_desc.bg_inode_table + (pos * sizeof(struct ext2_inode))/EXT2_BLOCK_SIZE;
	read_smr_blocks(inode_buf,blk_no,1);
	sip = (struct ext2_inode *)inode_buf;
	sip += (pos % inodes_per_block);
	*ip = *sip;
	return 0;	
}


static int is_legal_super_block(struct ext2_super_block * sbp){
	static unsigned int blk_size[3] = {
		1024,2048,4096
	};

	unsigned int index = 0;
	
	if(sbp->s_magic != EXT2_MAGIC_NUMBER){
		DS_PRINTF((PRINT_PREFIX"illegal magic number 0x%x\n",(unsigned int)sbp->s_magic));
		return 0;
	}
	if(sbp->s_blocks_count < sbp->s_first_data_block){
		DS_PRINTF((PRINT_PREFIX"illegal: s_blocks_count < s_first_data_block \n"));
		return 0;
	}
	index = sbp->s_log_block_size ;
	if(index > 2){// 0,1,2 for 1K,2K,4K respectively, 
		DS_PRINTF((PRINT_PREFIX"illegal s_log_block_size \n"));
		return 0;
	}
	if(index < 2){
		DS_PRINTF((PRINT_PREFIX"We only consider 4K in this version for simplicity \n"));
		return 0;
	}
	if(sbp->s_blocks_per_group != blk_size[index]*8){
		DS_PRINTF((PRINT_PREFIX"illegal s_blocks_per_group \n"));
		return 0;
	}

	
	if(sbp->s_blocks_count > (SMR_DISK_SIZE/blk_size[index])){
		DS_PRINTF((PRINT_PREFIX"illegal s_blocks_count \n"));
		return 0;
	}
	return 1;
}


static void mark_meta_blks(int action){
    unsigned int i = 0;
	int pos, len;
	meta_vect_index idx;
	block_t blk_no;
	mapping_entry * mtable;

	mtable = get_mapping_table();
	
	for(i = 0; i < sizeof(idx_2_flag)/sizeof(idx_2_flag[0]); i++){
		idx = idx_2_flag[i].idx;
		len = get_vect_size(meta_ctr.meta_blks[idx]);
		for(pos = 0; pos < len; pos++){
			blk_no = get_vect_element(meta_ctr.meta_blks[idx],pos);
			if(action == CLEAN_META_FLAG){
				mtable[blk_no].meta_flag = 0;
			}else{
				mtable[blk_no].meta_flag = idx_2_flag[i].meta_flag;
			}
		}
	}	
}



// Need to be OPTIMIZED
static int mark_one_blk_bmp(int * new_ptr, block_t from){
	block_t blk_no;
    unsigned int i,j;
	int count;
	//(void)old_ptr;
	
	count = 0;
	blk_no = from;
	if(meta_ctr.status == EXT2_DETECTED){
        for(i = 0; i < EXT2_BLOCK_SIZE/sizeof(*new_ptr); i++){
			if(new_ptr[i] == -1){
				blk_no += sizeof(new_ptr[i])*8;
			}else{
				for(j = 0; j < sizeof(new_ptr[i])*8; j++){
					if(GET_BIT(new_ptr[i],j) == 0){// mark it as free block                                  	
						if(in_hdd_buf(blk_no)){
							reclaim_free_hdd_buf(blk_no);
						}else if(in_smr_disk(blk_no)){
							invalidate_mapping_entry(blk_no);
						}else{
							// nothing to do, no mapping 					
						}
						count++;
					}
					blk_no++;
				}
			}
        }		
	}
	DS_PRINTF((PRINT_PREFIX"mark_one_blk_bmp(from = 0x%x): count = 0x%x \n",
						(unsigned int)from,count));
	return count;
}

int mark_all_free_blks(void){
	//unsigned int i;
	int len,pos;
	block_t blk_no,from;
	static int blk_bmp[EXT2_BLOCK_SIZE/sizeof(int)];	
	int count;
	int sum = 0;
	


	len = get_vect_size(meta_ctr.meta_blks[BLK_BMP_IDX]);
	for(pos = 0; pos < len; pos++){
		from = pos * (meta_ctr.sb0.s_blocks_per_group) + meta_ctr.sb0.s_first_data_block;
		blk_no = get_vect_element(meta_ctr.meta_blks[BLK_BMP_IDX],pos);
		read_smr_blocks(blk_bmp,blk_no,1);
		//DS_PRINTF((PRINT_PREFIX"mark_one_blk_bmp(0x%x) \n",(unsigned int) from));
		count = mark_one_blk_bmp(blk_bmp,from);
		sum += count;
		DS_PRINTF((PRINT_PREFIX"free count = 0x%x , BG#0x%x, at block 0x%x\n",
							count,pos,(unsigned int)blk_no));
	}	
	DS_PRINTF((PRINT_PREFIX"mark_all_free_blks(void): total free : 0x%x\n",(unsigned int)sum));
	return sum;
}



static int is_compatible_fs(struct ext2_super_block * old_sb, struct ext2_super_block * new_sb){
	if(old_sb->s_blocks_per_group != new_sb->s_blocks_per_group){
		return 0;
	}
	if(old_sb->s_inodes_per_group != new_sb->s_inodes_per_group){
		return 0;
	}
	if(old_sb->s_blocks_count != new_sb->s_blocks_count){
		return 0;
	}
	if(old_sb->s_inodes_count != new_sb->s_inodes_count){
		return 0;
	}
	if(old_sb->s_log_block_size != new_sb->s_log_block_size){
		return 0;
	}
	return 1;
}

void detect_ext2_fs(char * buf,block_t blk_no){
	//int pos;	
	struct ext2_super_block * sbp;
	
	//DS_PRINTF((PRINT_PREFIX"blk_no = %lu \n",(unsigned long)blk_no));
	if(meta_ctr.status == EXT2_DETECTED){
		if(blk_no == SB0_BLOCK_NO){
			DS_PRINTF((PRINT_PREFIX"EXT2_DETECTED: hit super block #0 \n"));
			sbp = (struct ext2_super_block *)(buf + FIRST_SUPER_BLOCK_OFFSET);
			if(!is_legal_super_block(sbp)){
				DS_PRINTF((PRINT_PREFIX"!is_legal_super_block(sbp) \n"));
				meta_ctr.status = NO_SB0;
				mark_meta_blks(CLEAN_META_FLAG);
			}else{
				/**
					we need to check whethe it is a new EXT2 ?  
					Ignore it right now
				 */
				if(is_compatible_fs(&meta_ctr.sb0,sbp)){
					meta_ctr.sb0 = *sbp;					
				}else{
					// redetect the ext2 file system
					DS_PRINTF((PRINT_PREFIX"reformat to a incompatible EXT2 file system \n"));					
					meta_ctr.status = NO_SB0;
					mark_meta_blks(CLEAN_META_FLAG);
				}
			}
		}else{
			// check whether block bitmaps is updated			
			if(has_mapping(blk_no) && (get_meta_flag(blk_no)== META_BLOCK_BITMAP)){
				DS_PRINTF((PRINT_PREFIX"hit block bitmap: 0x%lx \n",(unsigned long)blk_no));
				#if 0
				// blk_no MUST be in meta_ctr.meta_blks[BLK_BMP_IDX], or it is a deadly error
				pos = search_in_sorted_vector(meta_ctr.meta_blks[BLK_BMP_IDX],blk_no);
				if(pos == HSET_ACTION_FAIL){
					DS_PRINTF((PRINT_PREFIX"ERROR: .....pos == HSET_ACTION_FAIL .......\n"));
					return;
				}
				mark_one_blk_bmp((int*) buf,
						pos * (meta_ctr.sb0.s_blocks_per_group) + meta_ctr.sb0.s_first_data_block);
				#endif
			}
		}
	}else{
		(dectors[meta_ctr.status])(buf,blk_no);
	}

}
	
ext2_meta_ctr * get_ext2_meta_ctr(void){
	return &meta_ctr;
}

int is_ext2_fs(void){
	return meta_ctr.status == EXT2_DETECTED;
}


int init_meta_ctr(void){	
	int i;
	for(i = SB_IDX; i <= TMP_IDX; i++){
		meta_ctr.meta_blks[i] = create_sorted_vector();
	}
	meta_ctr.status = NO_SB0;
	return 0;
}


int release_meta_ctr(void){
	int i;
	for(i = SB_IDX; i <= TMP_IDX; i++){
		release_sorted_vector(meta_ctr.meta_blks[i]);
		meta_ctr.meta_blks[i] = NULL;
	}
    return 0;
}


void print_all_meta_blocks(void){
	int i;
	DS_PRINTF((PRINT_PREFIX"print_all_meta_blocks(void)\n"));
	for(i = SB_IDX; i <= BLK_BMP_IDX; i++){		
		if(meta_ctr.meta_blks[i]){
			DS_PRINTF((PRINT_PREFIX"len = %d ,%s:\n", get_vect_size(meta_ctr.meta_blks[i]),idx_2_flag[i].name));
			prt_sorted_vect(meta_ctr.meta_blks[i]);
		}
	}
}

