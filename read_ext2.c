
#include "read_ext2.h"

#include "hdd_io.h"
#include "hash_set.h"

#include "ext2_meta.h"
#include "ds_utils.h"
#include "smr.h"


///////////////////////////////Macros & Structs////////////////////////////////////////

#define		READ_BLOCKS					read_smr_blocks
#define		MAX_INDENT_SIZE				63
#define		INDENT_STEP					3
#define		INDENT_CHAR					'|'
///////////////////////////////Static Function Declarations////////////////////////////
static int read_direct_blocks(char * fdata,struct ext2_inode * ip,int from,int n);
static int read_indirect1_blocks(char * fdata,block_t blk_no,int from,int n);
static int read_indirect2_blocks(char * fdata,block_t blk_no,int from,int n);
static int read_indirect3_blocks(char * fdata,block_t blk_no,int from,int n);

///////////////////////////////Static Varialbes////////////////////////////////////////

static char indent_ws[MAX_INDENT_SIZE+1];
///////////////////////////////Global Varialbes////////////////////////////////////////



///////////////////////////////Definitions of functions////////////////////////////////

void print_ext2_group_desc(struct ext2_group_desc * gdp){

    PRT_INT_FIELD(gdp,bg_block_bitmap);
    PRT_INT_FIELD(gdp,bg_inode_bitmap);
    PRT_INT_FIELD(gdp,bg_inode_table);
    PRT_INT_FIELD(gdp,bg_free_blocks_count);
    PRT_INT_FIELD(gdp,bg_free_inodes_count);
    PRT_INT_FIELD(gdp,bg_used_dirs_count);
}

void print_ext2_dir_entry_2(struct ext2_dir_entry_2 * dep){

    PRT_INT_FIELD(dep,inode);
    PRT_INT_FIELD(dep,rec_len);
    PRT_INT_FIELD(dep,name_len);
    PRT_INT_FIELD(dep,file_type);
    PRT_STR_FIELD(dep,name);
}

void print_ext2_inode(struct ext2_inode * ip){
    int i;

    PRT_INT_FIELD(ip,i_mode);
    PRT_INT_FIELD(ip,i_uid);
    PRT_INT_FIELD(ip,i_size);
    PRT_INT_FIELD(ip,i_atime);
    PRT_INT_FIELD(ip,i_ctime);
    PRT_INT_FIELD(ip,i_mtime);
    PRT_INT_FIELD(ip,i_dtime);
    PRT_INT_FIELD(ip,i_gid);
    PRT_INT_FIELD(ip,i_links_count);
    PRT_INT_FIELD(ip,i_blocks);
    PRT_INT_FIELD(ip,i_flags);
    for(i = 0; i < EXT2_N_BLOCKS; i++){
        PRT_INT_FIELD(ip,i_block[i]);
    }

}

void print_ext2_super_block(struct ext2_super_block * sbp){
    //DS_PRINTF((PRINT_PREFIX"s_inodes_count = %x \n",sbp->s_inodes_count));
    int i = 0;

    PRT_INT_FIELD(sbp,s_inodes_count);
    PRT_INT_FIELD(sbp,s_blocks_count);
    PRT_INT_FIELD(sbp,s_r_blocks_count);
    PRT_INT_FIELD(sbp,s_free_blocks_count);

    PRT_INT_FIELD(sbp,s_free_inodes_count);
    PRT_INT_FIELD(sbp,s_first_data_block);
    PRT_INT_FIELD(sbp,s_log_block_size);


    PRT_INT_FIELD(sbp,s_blocks_per_group);

    PRT_INT_FIELD(sbp,s_inodes_per_group);
    PRT_INT_FIELD(sbp,s_magic);
    PRT_INT_FIELD(sbp,s_first_ino);
    PRT_INT_FIELD(sbp,s_inode_size);

    PRT_INT_FIELD(sbp,s_block_group_nr);
    PRT_STR_FIELD(sbp,s_last_mounted);
    PRT_STR_FIELD(sbp,s_volume_name);

    DS_PRINTF((PRINT_PREFIX"uuid[16]:"));
    for(i = 0; i < 16; i++){
        DS_PRINTF((PRINT_PREFIX"%x ",sbp->s_uuid[i]));
    }
    DS_PRINTF((PRINT_PREFIX"\n"));
}

/**
 * @brief read_direct_blocks
 *        read the direct blocks [from, from+n) of a file @ip
 * @param fdata         buffer for reading file
 * @param ip            inode
 * @param from          0 <= from < EXT2_NDIR_BLOCKS
 * @param n             from + n <= EXT2_NDIR_BLOCKS
 * @return
 */
static int read_direct_blocks(char * fdata,struct ext2_inode * ip,int from,int n){

    int i = 0;

    for(i = from; i < n+from ; i++){
        READ_BLOCKS(fdata,ip->i_block[i],1);
        fdata += EXT2_BLOCK_SIZE;
    }
    return n;

}
/**
 * @brief read_indirect1_blocks
 *      read the designated part of indirect blocks.
 *       [from,from+n) is subset of [0,INDIRECT_BLKS)
 * @param fdata
 * @param blk_no
 * @param from      0 <= from < INDIRECT_BLKS
 * @param n         from +n <= INDIRECT_BLKS
 * @return
 */
static int read_indirect1_blocks(char * fdata,block_t blk_no, int from,int n){
    //char buf[EXT2_BLOCK_SIZE];
    char * buf;
    int i = 0;
    block_t * ptr;
	
	buf = ALLOC_MEM(EXT2_BLOCK_SIZE);
	
    READ_BLOCKS(buf,blk_no,1);
    ptr = ((block_t *) buf)+from;

    for(i = 0; i < n; i++){
        READ_BLOCKS(fdata,*ptr,1);
        fdata += EXT2_BLOCK_SIZE;
        ptr++;
    }

	FREE_MEM(buf);
    return n;
}
/**
 * @brief read_indirect2_blocks
 *      read the designatged part of double indirect blocks
 *          [from,from+n) is subset of [0,DOUBLE_IND_BLKS)
 * @param fdata
 * @param blk_no
 * @param from
 * @param n
 * @return
 */
static int read_indirect2_blocks(char * fdata,block_t blk_no,int from,int n){
    //char buf[EXT2_BLOCK_SIZE];
    char * buf;
    //int i;
    block_t * ptr;
    int index = from / INDIRECT_BLKS;
    int offset = from % INDIRECT_BLKS;
    int saved_n = n;
    int count;

	buf = ALLOC_MEM(EXT2_BLOCK_SIZE);
    READ_BLOCKS(buf,blk_no,1);
    ptr = ((block_t *) buf) + index;
    // parameter 'from' of read_indirect1_blocks() may be not 0
    count = INDIRECT_BLKS-offset;
    if(n <= count){
        read_indirect1_blocks(fdata,*ptr,offset,n);
        return saved_n;
    }else{
        read_indirect1_blocks(fdata,*ptr,offset,count);
        fdata += count *EXT2_BLOCK_SIZE;
        n -= count;
        ptr++;
    }
    // now parameter 'from' of read_indirect1_blocks() is 0
    while(n > 0){
        if(n <= INDIRECT_BLKS){
            read_indirect1_blocks(fdata,*ptr,0,n);
            return saved_n;
        }else{
            read_indirect1_blocks(fdata,*ptr,0,INDIRECT_BLKS);
            fdata += INDIRECT_BLKS*EXT2_BLOCK_SIZE;
            ptr++;
            n -= INDIRECT_BLKS;
        }

    }
	FREE_MEM(buf);
    return saved_n;
}
/**
 * @brief read_indirect3_blocks
 *      read the designatged part of triple indirect blocks
 *      [from,from+n) is subset of [0,TRIPLE_IND_BLKS)
 * @param fdata
 * @param blk_no
 * @param from
 * @param n
 * @return
 */
static int read_indirect3_blocks(char * fdata,block_t blk_no,int from,int n){
    //char buf[EXT2_BLOCK_SIZE];
    char * buf;
    //int i;
    block_t * ptr;
    int index = from / DOUBLE_IND_BLKS;
    int offset = from % DOUBLE_IND_BLKS;
    int saved_n = n;
    int count;

	buf = ALLOC_MEM(EXT2_BLOCK_SIZE);
    READ_BLOCKS(buf,blk_no,1);
    ptr = ((block_t *) buf) + index;
    // parameter 'from' of read_indirect2_blocks() may be not 0
    count = DOUBLE_IND_BLKS-offset;
    if(n <= count){
        read_indirect2_blocks(fdata,*ptr,offset,n);
        return saved_n;
    }else{
        read_indirect2_blocks(fdata,*ptr,offset,count);
        fdata += count *EXT2_BLOCK_SIZE;
        n -= count;
        ptr++;
    }
    // now parameter 'from' of read_indirect2_blocks() is 0
    while(n > 0){
        if(n <= DOUBLE_IND_BLKS){
            read_indirect2_blocks(fdata,*ptr,0,n);
            return saved_n;
        }else{
            read_indirect2_blocks(fdata,*ptr,0,DOUBLE_IND_BLKS);
            fdata += DOUBLE_IND_BLKS*EXT2_BLOCK_SIZE;
            ptr++;
            n -= DOUBLE_IND_BLKS;
        }
    }
	FREE_MEM(buf);
    return saved_n;
}



/**
 * @brief read_file_data
 *  this function allocate 'enough' memory for the whole file @ino,
 *  then read all the file data into memory.
 *  If the file is too large, this function is not suitable.
 *  In this case,we had better use read_ext2_file_blocks() to read only part of the file
 *  each time.
 * @param ino   the inode number of a file
 * @return
 */
void * read_whole_ext2_file(struct ext2_inode * ip){
	
	
	char * fdata;
	char * cur_buf;
	int blocks;


    fdata = ALLOC_MEM(ALIGNED_BYTES(ip->i_size));
    cur_buf = fdata;
    blocks = BYTES_ALIGN_TO_BLOCKS(ip->i_size);

    //DS_PRINTF((PRINT_PREFIX"size = %x \n",ALIGNED_BYTES(ip->i_size)));
    // direct blocks

    if(blocks <= EXT2_NDIR_BLOCKS){
        read_direct_blocks(cur_buf,ip,0,blocks);
		goto ret_val;
    }else{
        read_direct_blocks(cur_buf,ip,0,EXT2_NDIR_BLOCKS);
        cur_buf += EXT2_NDIR_BLOCKS * EXT2_BLOCK_SIZE;
        blocks -= EXT2_NDIR_BLOCKS;
    }
    // indirect blocks
    if(blocks <= INDIRECT_BLKS){
        read_indirect1_blocks(cur_buf,ip->i_block[EXT2_IND_BLOCK],0,blocks);
        goto ret_val;
    }else{
        read_indirect1_blocks(cur_buf,ip->i_block[EXT2_IND_BLOCK],0,INDIRECT_BLKS);
        cur_buf += INDIRECT_BLKS * EXT2_BLOCK_SIZE;
        blocks -= INDIRECT_BLKS;
    }
    // double indirect blocks
    if(blocks <= DOUBLE_IND_BLKS){
        read_indirect2_blocks(cur_buf,ip->i_block[EXT2_DIND_BLOCK],0,blocks);
        goto ret_val;
    }else{
        read_indirect2_blocks(cur_buf,ip->i_block[EXT2_DIND_BLOCK],0,DOUBLE_IND_BLKS);
        cur_buf += DOUBLE_IND_BLKS * EXT2_BLOCK_SIZE;
        blocks -= DOUBLE_IND_BLKS;
    }
    // triple indirect blocks
    if(blocks > TRIPLE_IND_BLKS){
        DS_PRINTF((PRINT_PREFIX"too large blocks \n"));
    }else{
        read_indirect3_blocks(cur_buf,ip->i_block[EXT2_TIND_BLOCK],0,blocks);
    }
ret_val:
    return fdata;
}

void free_ext2_file_in_mem(void *fdata){
    FREE_MEM(fdata);
}

/**
 * @brief read_ext2_file_blocks
 *          read [from,from+n) blocks from file @ino into @buf.
 *          the meta data of file system has already been loaded into NVM.
 * @param ino
 * @param buf
 * @param from      starting at block @from
 * @param n         n blocks
 * @return
 */
int read_ext2_file_blocks(__u32 ino, char * buf, int from,int n){

    static int pos[4] = {
                            EXT2_NDIR_BLOCKS,
                            EXT2_NDIR_BLOCKS+INDIRECT_BLKS,
                            EXT2_NDIR_BLOCKS+INDIRECT_BLKS+DOUBLE_IND_BLKS,
                            EXT2_NDIR_BLOCKS+INDIRECT_BLKS+DOUBLE_IND_BLKS+TRIPLE_IND_BLKS
                        };
    typedef int (* READ_FUNC_PTR)(char * fdata,block_t blk_no,int from,int n);
    static READ_FUNC_PTR read_func[4] = {
        NULL,
        read_indirect1_blocks,
        read_indirect2_blocks,
        read_indirect3_blocks
    };
    int blocks[4] = {0};
    struct ext2_inode * ip = ALLOC_MEM(sizeof(struct ext2_inode));
    int start = -1, end = -1;
    int i = 0;
    int saved_from = from, saved_n = n;

	read_ext2_inode(ip,ino);
	
    for(i = 0; i < 4; i++){
        // [from,from+n)
        if((from < pos[i]) && (start == -1)){
            start = i;
        }
        if((from+n <= pos[i]) && (end == -1)){
            end = i;
        }
    }

    for(i = start; i <= end; i++){
        if(n > pos[i]-from){
            blocks[i] = pos[i]-from;
            from = pos[i];
            n -= blocks[i];
        }else{
            blocks[i] = n;
        }
    }
#if 0
    for(i = 0; i < 4; i++){
        DS_PRINTF((PRINT_PREFIX"blocks[%d] = %d \n",i,blocks[i]));
    }
#endif
    from = saved_from;
    // read the direct blocks if necessary
    if(blocks[0] != 0){
        //DS_PRINTF((PRINT_PREFIX"from = %d, blocks[0]= %d\n",from,blocks[0]));
        read_direct_blocks(buf,ip,from,blocks[0]);
        buf += EXT2_BLOCK_SIZE*blocks[0];
    }
    // read indirect / double indirect /triple indirect blocks if applicable
    for(i = 1; i < 4; i++){
        if(blocks[i] != 0){
            //DS_PRINTF((PRINT_PREFIX"from: %d \n",from-pos[i-1]));
            if(blocks[i-1] != 0){
                from = 0;
            }else{
                from = saved_from - pos[i-1];
            }
            (read_func[i])(buf,ip->i_block[EXT2_IND_BLOCK+i-1],from,blocks[i]);
            buf += EXT2_BLOCK_SIZE*blocks[i];
        }
    }
	FREE_MEM(ip);
    return saved_n;
}

/**
 * @brief visit_ext2_dir
 *      visit file system sub tree starting at directory @ino
 * @param ino
 *      the inode number of a directory
 * @param depth
 *      used for printing in indenting format.
 */
void visit_ext2_dir(__u32 ino, int depth){
    char * fdata;
    char * cur;
    char * name;//[EXT2_NAME_LEN+1];
    struct ext2_dir_entry_2 * dep;
    struct ext2_inode * ip;
    __u32 size = 0;
	char saved_ch;
	name = ALLOC_MEM(EXT2_NAME_LEN+1);
	ip = ALLOC_MEM(sizeof(struct ext2_inode));
    read_ext2_inode(ip,ino);
	
    if((ip->i_mode & EXT2_S_MASK) != EXT2_S_IFDIR){
		FREE_MEM(name);
		FREE_MEM(ip);
        return;
    }
    fdata = read_whole_ext2_file(ip);
    cur = fdata;
    while(size < ip->i_size){
        dep = (struct ext2_dir_entry_2 *) cur;
        //DS_PRINTF((PRINT_PREFIX"size = %d, ip->size = %d %d\n",size,ip->i_size,dep->rec_len));
        ds_memcpy(name,dep->name,dep->name_len);
        name[dep->name_len] = 0;
        //
        if(depth*INDENT_STEP < MAX_INDENT_SIZE){
			saved_ch = indent_ws[depth*INDENT_STEP];
			indent_ws[depth*INDENT_STEP] = 0;
        }
		
        DS_PRINTF((PRINT_PREFIX"%s%s %d\n",indent_ws,name,dep->inode));
		//
		if(depth*INDENT_STEP < MAX_INDENT_SIZE){
			indent_ws[depth*INDENT_STEP] = saved_ch;
        }
        cur += dep->rec_len;
        size += dep->rec_len;
        // skip "." and ".."
        if (strcmp(".", name) == 0 || strcmp("..", name) == 0){
            continue;
        }
        // 
        if(dep->inode != 0){
        	visit_ext2_dir(dep->inode,depth+1);
        }
    }
	FREE_MEM(name);
	FREE_MEM(ip);
    free_ext2_file_in_mem(fdata);
}

void display_ext2_meta_data(void){
	static struct ext2_super_block sb;
	static struct ext2_group_desc gd;

	int i,cnt;
	int len;
	ext2_meta_ctr * ctr;

	ctr = get_ext2_meta_ctr();
	len = get_vect_size(ctr->meta_blks[SB_IDX]);
	for(i = 0; i < len; i++){
		read_super_block(&sb,i);
		DS_PRINTF((PRINT_PREFIX"EXT2 FS SUPER BLOCK # %d at BLOCK 0x%x\n",
			i, (unsigned int) get_vect_element(ctr->meta_blks[SB_IDX],i)));
		print_ext2_super_block(&sb);
	}
	

	cnt = (sb.s_blocks_count - sb.s_first_data_block - 1)/sb.s_blocks_per_group + 1;
	for(i = 0; i < cnt; i++){
		read_group_desc(&gd,i);
		DS_PRINTF((PRINT_PREFIX"...........EXT2 FS GROUP DESCRIPTOR %d.........\n",i));
		print_ext2_group_desc(&gd);
	}
}
/**
 * @brief traverse_ext2_fs
 *      visit the whole ext2 file system
 */
void traverse_ext2_fs(void){	
	int i,j;
	for(i = 0; i < MAX_INDENT_SIZE; i+=INDENT_STEP){
		indent_ws[i] = INDENT_CHAR;
		for(j = 1; j < INDENT_STEP; j++){
			indent_ws[i+j] = ' ';
		}
	}
	indent_ws[MAX_INDENT_SIZE] = 0;
    DS_PRINTF((PRINT_PREFIX".........traverse_ext2_fs()..........\n/\n"));
    visit_ext2_dir(EXT2_ROOT_INO,1);
	
}


