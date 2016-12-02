#include "hash_set.h"
#include "ds_utils.h"

enum BLOCK_ACTION{
    SEARCH_BLK = 1,
    INSERT_BLK,
    REMOVE_BLK,
};




void prt_blks(blocks_bucket * bk){
    int i;
    for(i = 0; i < bk->blks_cnt; i++){
        DS_PRINTF((PRINT_PREFIX"%llu ",bk->blks[i]));
    }
    DS_PRINTF((PRINT_PREFIX"\n"));
}


/**
 * @brief do_blk_no
 * @param bk
 * @param blk_no
 * @param action
 * @return
 * (1)  action is REMOVE_BLK
 *      return HSET_ACTION_FAIL, when no such blk_no
 *      return the old position when removing the blk_no
 * (2)  action is INSERT_BLK
 *      return HSET_ACTION_FAIL, when alread existing
 *      return the position, when inserting one
 * (3)  action is SEARCH_BLK
 *      return HSET_ACTION_FAIL, when no existing
 *      return the position, when existing
 */
static int do_blk_no(blocks_bucket * bk, block_t blk_no, enum BLOCK_ACTION action){
    int i,left,right;
	int retval;

	retval = HSET_ACTION_FAIL;
    if(bk->blks_cnt == 0){
        if(action == SEARCH_BLK || action == REMOVE_BLK){
            return HSET_ACTION_FAIL;
        }
        bk->blks[0] = blk_no;
		retval = 0;
    }else{
        left = 0; right = bk->blks_cnt-1;
        while(left <= right){
            i = (left+right)/2;
            if(blk_no < bk->blks[i]){
                right = i-1;
            }else if(blk_no > bk->blks[i]){
                left = i+1;
            }else{
                if(action == SEARCH_BLK){					
                    return i;
                }else if(action == REMOVE_BLK){
                	retval = i;
                    while(i < bk->blks_cnt-1){
                        bk->blks[i] = bk->blks[i+1];
                        i++;
                    }
                    bk->blks_cnt--;
                    return retval;
                }else{  // INSERT_BLK :  already existing
                    return HSET_ACTION_FAIL;
                }
            }
        }
        if(action == SEARCH_BLK || action == REMOVE_BLK){
            return HSET_ACTION_FAIL;
        }
        // action is INSERT_BLK
        if(left >= bk->blks_cnt){
            bk->blks[bk->blks_cnt] = blk_no;
			retval = bk->blks_cnt;
        }else{
            int j;
            for(j = bk->blks_cnt-1; j >= left; j--){
                bk->blks[j+1] = bk->blks[j];
            }
            bk->blks[left] = blk_no;
			retval = left;
        }
    }
    bk->blks_cnt++;
    return retval;
}
/**
 * @brief is_existing
 * @param hset
 * @param blk_no
 * @return
 *          1,  there is already such value in hash_set
 *          0,  there is not such value
 */
int is_existing(sorted_vector * hset,block_t blk_no){
    blocks_bucket * bk;

    bk = hset->buckets[blk_no % hset->buckets_cnt];
    return do_blk_no(bk,blk_no,SEARCH_BLK) != HSET_ACTION_FAIL;
}
/**
 * @brief 	search_in_sorted_vector
 * @param hset
 * @param blk_no
 * @return
 *          position,  there is already such value in hash_set
 *          HSET_ACTION_FAIL,  there is not such value
 */
int search_in_sorted_vector(sorted_vector * hset,block_t blk_no){
	blocks_bucket * bk;

    bk = hset->buckets[blk_no % hset->buckets_cnt];
	return do_blk_no(bk,blk_no,SEARCH_BLK);
}
/**
 * @brief insert_into_sorted_vector
 * (1)  do nothing when @blk_no exists in hashset
 * (2)  insert blk_no into blocks_bucket when not existing
 * @param hset
 * @param blk_no
 * @return
 *      HSET_ACTION_FAIL, when there is already a same value in hash_set. No insert action is done.
 *      position, when there is no such value, insert blk_no into the hash_set
 */
int insert_into_sorted_vector(sorted_vector * hset,block_t blk_no){
    blocks_bucket * bk;
    int ret;

    bk = hset->buckets[blk_no % hset->buckets_cnt];
    // test whether it is full
    if(bk->blks_cnt == bk->capacity){
        // double the capacity of a bucket
        int i;
        block_t * blks = ALLOC_MEM(sizeof(block_t) * bk->capacity * 2);
        for(i = 0; i < bk->capacity; i++){
            blks[i] = bk->blks[i];
        }
        FREE_MEM(bk->blks);
        bk->blks = blks;
        bk->capacity *= 2;
    }
    // now the bucket is large enough
    ret = do_blk_no(bk,blk_no,INSERT_BLK);
    if(ret != HSET_ACTION_FAIL){
        hset->blocks_cnt++;
    }
    return ret;
}


int remove_from_sorted_vector(sorted_vector * hset, block_t blk_no){

    blocks_bucket * bk;
    int ret;

    bk = hset->buckets[blk_no % hset->buckets_cnt];
    ret = do_blk_no(bk,blk_no,REMOVE_BLK);
    if(ret != HSET_ACTION_FAIL){
        hset->blocks_cnt--;
    }
    return ret;
}

static hash_set * create_hash_set(int buckets_cnt){
    hash_set * hset;
    int i;

    hset = ALLOC_MEM(sizeof(hash_set));
    hset->buckets_cnt = buckets_cnt;
    hset->buckets = ALLOC_MEM(sizeof(blocks_bucket *)*buckets_cnt);
    hset->blocks_cnt = 0;
    for(i = 0; i < buckets_cnt; i++){
        // allocate and init a bucket
        hset->buckets[i] = ALLOC_MEM(sizeof(blocks_bucket));
        hset->buckets[i]->blks_cnt = 0;
        hset->buckets[i]->capacity = DEF_CAPACITY_OF_BUCKET;
        hset->buckets[i]->blks = ALLOC_MEM(sizeof(block_t)*DEF_CAPACITY_OF_BUCKET);
    }
    return hset;
}
/**
 * @brief release_hash_set
 *        release the memory of hash set
 * @param hset
 */
static void release_hash_set(hash_set * hset){
    int i;
    blocks_bucket * bk;
    for(i = 0; i < hset->buckets_cnt; i++){
        if( (bk = hset->buckets[i]) != NULL){
            FREE_MEM(bk->blks);
            FREE_MEM(bk);
            hset->buckets[i] = NULL;
        }
    }
    FREE_MEM(hset->buckets);
    FREE_MEM(hset);
}
#if 0
static void prt_hash_set_by_bucket(hash_set * hset){
    int i;

    for(i = 0; i < hset->buckets_cnt; i++){

        int j;
        blocks_bucket * bk;

        bk = hset->buckets[i];
        if(bk->blks_cnt > 0){
            for(j = 0; j < bk->blks_cnt; j++){
                DS_PRINTF((PRINT_PREFIX"%llu ",bk->blks[j]));
            }
            DS_PRINTF((PRINT_PREFIX"\n"));
        }
    }
}

static void prt_hash_set_by_blk_no(hash_set * hset){
    blocks_bucket tmp_bk;
    blocks_bucket * bk;
    int i, offset = 0;

    tmp_bk.blks = ALLOC_MEM(sizeof(block_t) * hset->buckets_cnt);
    tmp_bk.blks_cnt = 0;
    tmp_bk.capacity = hset->buckets_cnt;
    tmp_bk.index = 0;

    // insert the first blk_no of each blocks_bucket into tmp_bk
    for(i = 0; i < hset->buckets_cnt; i++){
        bk = hset->buckets[i];
        bk->index = 0;
        if(bk->blks_cnt > 0){
            do_blk_no(&tmp_bk,bk->blks[0],INSERT_BLK);
        }
    }
    while(tmp_bk.blks_cnt >= 1){
        block_t end;

        offset = (tmp_bk.blks[0] % hset->buckets_cnt);
        bk = hset->buckets[offset];
        if(tmp_bk.blks_cnt > 1){
            end = tmp_bk.blks[1];
        }else{
            end = bk->blks[bk->blks_cnt-1];
        }
        for(i = bk->index; bk->blks[i] <= end && i < bk->blks_cnt; i++){
            DS_PRINTF((PRINT_PREFIX"%llu ",bk->blks[i]));
            //bk->index++;
        }
        bk->index = i;
        do_blk_no(&tmp_bk,tmp_bk.blks[0],REMOVE_BLK);
        if(bk->index < bk->blks_cnt){
            do_blk_no(&tmp_bk,bk->blks[bk->index],INSERT_BLK);
        }
    }
    DS_PRINTF((PRINT_PREFIX"\n"));
    FREE_MEM(tmp_bk.blks);
}

void prt_hash_set(hash_set * hset, enum PRT_ORDER order){
    if(order == BY_BUCKET){
        prt_hash_set_by_bucket(hset);
    }else{
        prt_hash_set_by_blk_no(hset);
    }

}
#endif
int get_vect_size(sorted_vector * hset){

    return hset->blocks_cnt;
}

block_t  get_vect_element(sorted_vector * vect, int pos){
	if(pos < 0 || pos >= get_vect_size(vect)){
		DS_PRINTF((PRINT_PREFIX"get_vect_element(): ERROR"));
		return HSET_ACTION_FAIL;
	}else{
		return vect->buckets[0]->blks[pos];
	}
}

void copy_all_elements(sorted_vector * dst,sorted_vector * src){
	int len,i;
	remove_all_elements(dst);
	len = get_vect_size(src);
	for(i = 0; i < len; i++){
		insert_into_sorted_vector(dst,get_vect_element(src,i));
	}
}
void remove_all_elements(sorted_vector * vect){
	vect->blocks_cnt = 0;
	vect->buckets[0]->blks_cnt= 0;
}

sorted_vector * create_sorted_vector(void){
	return create_hash_set(1);
}
void release_sorted_vector(sorted_vector * vect){
	release_hash_set(vect);
}


void prt_sorted_vect(sorted_vector * vect){
	int len;
	int i;
	block_t blk_no;
	
	len = get_vect_size(vect);
	//DS_PRINTF((PRINT_PREFIX"len = %d \n",len));
	for(i = 0; i < len; i++){
		blk_no = get_vect_element(vect,i);
		DS_PRINTF((PRINT_PREFIX"%d = 0x%x \n",i,(unsigned int) blk_no));
	}	
	return ;
}


