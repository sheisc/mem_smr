

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/timer.h>
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/hdreg.h>	/* HDIO_GETGEO */
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	/* invalidate_bdev */
#include <linux/bio.h>

#include <linux/uaccess.h>
#include "hdd_buf.h"
#include "ds_types.h"
#include "ds_utils.h"
#include "hdd_io.h"
#include "ds_counter.h"
#include "smr.h"
#include "ext2_meta.h"


//#include "read_ext2.h"

//

MODULE_LICENSE("Dual BSD/GPL");
// set this value be equal with EXT2_BLOCK_SIZE
#define		HARD_DISK_SECT_SIZE		EXT2_BLOCK_SIZE


// whether to save SMR DISK or not
static int save = 0;
module_param(save, int, 0);

// whether to reset all counters
static int reset = 0;
module_param(reset, int, 0);

// whether to enable ext2 file system aware
static int aware = 1;
module_param(aware, int, 0);

// whether to favor META data in HDD BUF
static int favor = 1;
module_param(favor, int, 0);



static int mdisk_major = 0;
//module_param(mdisk_major, int, 0);
static int hardsect_size = HARD_DISK_SECT_SIZE;
//module_param(hardsect_size, int, 0);
// 1GB memory disk for test
static int nsectors = TOTAL_SMR_BLOCKS_CNT;

//module_param(nsectors, int, 0);

/*
 * The different "request modes" we can use.
 */
enum {
	RM_SIMPLE  = 0,	/* The extra-simple request function */
#if 0		
	RM_FULL    = 1,	/* The full-blown version */
	RM_NOQUEUE = 2,	/* Use make_request */
#endif	
};
static int request_mode = RM_SIMPLE;
//module_param(request_mode, int, 0);

#if 1
/*
 * Minor number and partition management.
 */
#define SBULL_MINORS	1 
#endif
/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SHIFT	9
#define KERNEL_SECTOR_SIZE	(1<<KERNEL_SECTOR_SHIFT)

#define	KERN_SECTS_PER_HARD_SECT		(HARD_DISK_SECT_SIZE/KERNEL_SECTOR_SIZE)
// for init_system()	
#define	RW_BUF_SIZE		4096

/*
 * The internal representation of our device.
 */
struct mem_disk_dev {
        int size;                       /* Device size in sectors */
        u8 *data;                       /* The data array */
        short users;                    /* How many users */
        spinlock_t lock;                /* For mutual exclusion */
        struct request_queue *queue;    /* The device request queue */
        struct gendisk *gd;             /* The gendisk structure */
};

/////////////////////////////// static  variables //////////////////////////////////////////////////

static struct mem_disk_dev *mdisk_dev = NULL;

////////////////////////////////function definitions////////////////////////////////////////////////

int is_meta_favored(void){
	return favor;
}

int is_counter_reset(void){
	return reset;
}

int is_ext2_awared(void){
	return aware;
}

static int check_block_size(struct mem_disk_dev *dev,unsigned long sector,unsigned long nsect){
	unsigned long offset = sector*KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect*KERNEL_SECTOR_SIZE;
	if(sector % KERN_SECTS_PER_HARD_SECT != 0){
		printk (KERN_NOTICE "%lu not multiple of %d\n", sector,KERN_SECTS_PER_HARD_SECT);
		return -1;
	}
	if(nsect % KERN_SECTS_PER_HARD_SECT != 0){
		printk (KERN_NOTICE "%lu not multiple of %d\n", nsect,KERN_SECTS_PER_HARD_SECT);
		return -1;
	}
	if ((offset + nbytes) > dev->size) {
		printk (KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, nbytes);
		return -1;
	}
	return 0;
}

ds_counter * get_hdd_rw_counter(void){
	smr_ctr_data * ctr = get_smr_ctrl_data();
    return &ctr->hdd_cnt;
}

/**
 * @brief ds_read_blocks
 *        read @count blocks at offset @offset from RAW device/file
 *        both @count and @offset are in blocks
 * @param buf       buffer
 * @param offset_in_blocks    in blocks
 * @param num_in_blocks     in blocks
 * @return
 */
ds_size_t read_hdd_blocks(void * buf, ds_offset_t offset_in_blocks, ds_size_t num_in_blocks){
	unsigned long offset = offset_in_blocks*HARD_DISK_SECT_SIZE;
	unsigned long nbytes = num_in_blocks*HARD_DISK_SECT_SIZE;
	ds_counter * hdd_cnt = get_hdd_rw_counter();
	if(is_counter_enabled(hdd_cnt)){
        hdd_cnt->read_count += num_in_blocks;
    }
	ds_memcpy(buf, mdisk_dev->data + offset, nbytes);
    return 0;
}
/**
 * @brief ds_write_blocks
 *        write @count blocks into RAW device/file starting at @offset
 *        both @count and @offset are in blocks
 * @param buf
 * @param offset_in_blocks        int blocks
 * @param num_in_blocks         in blocks
 * @return
 */
ds_size_t write_hdd_blocks(const void *buf, ds_offset_t offset_in_blocks, ds_size_t num_in_blocks){
	unsigned long offset = offset_in_blocks*HARD_DISK_SECT_SIZE;
	unsigned long nbytes = num_in_blocks*HARD_DISK_SECT_SIZE;
	ds_counter * hdd_cnt = get_hdd_rw_counter();
    if(is_counter_enabled(hdd_cnt)){
        hdd_cnt->write_count += num_in_blocks;
    }
	ds_memcpy(mdisk_dev->data + offset, buf, nbytes);
	return 0;
}





/*
 * Handle an I/O request.
 */
static void do_data_transfer(struct mem_disk_dev *dev, unsigned long sector,
		unsigned long nsect, char *buffer, int write)
{

	ds_counter * cnt = get_smr_rw_counter();
	if(check_block_size(dev,sector,nsect) < 0){
		return;
	}


	if (write){
		write_smr_blocks(buffer,sector/KERN_SECTS_PER_HARD_SECT,
								nsect/KERN_SECTS_PER_HARD_SECT);
		//memcpy(dev->data + offset, buffer, nbytes);
		cnt->write_count += (nsect/KERN_SECTS_PER_HARD_SECT);
	}else{
		read_smr_blocks(buffer,sector/KERN_SECTS_PER_HARD_SECT,
									nsect/KERN_SECTS_PER_HARD_SECT);
		//memcpy(buffer, dev->data + offset, nbytes);
		cnt->read_count += (nsect/KERN_SECTS_PER_HARD_SECT);
	}
}

/*
 * The simple form of the request function.
 */
static void do_request(struct request_queue *q)
{
	struct request *req;
	int ret;

	req = blk_fetch_request(q);
	while (req) {
		struct mem_disk_dev *dev = req->rq_disk->private_data;	
#if 0	// To make /usr/src/linux-headers-4.15.0-177-generic happy		
		if (req->cmd_type != REQ_TYPE_FS) {
			printk (KERN_NOTICE "Skip non-fs request\n");
			ret = -EIO;
			goto done;
		}
#endif
		do_data_transfer(dev, blk_rq_pos(req), blk_rq_cur_sectors(req),
				bio_data(req->bio), rq_data_dir(req));
		ret = 0;
#if 0	// To make /usr/src/linux-headers-4.15.0-177-generic happy		
	done:
#endif	
		if(!__blk_end_request_cur(req, ret)){
			req = blk_fetch_request(q);
		}
	}
}

/*
 * Open and close.
 */

static int mem_disk_open(struct block_device *bdev, fmode_t mode)
{
	struct mem_disk_dev *dev = bdev->bd_disk->private_data;

	spin_lock(&dev->lock);
	dev->users++;
	spin_unlock(&dev->lock);
	return 0;
}

static void mem_disk_release(struct gendisk *disk, fmode_t mode)
{
	struct mem_disk_dev *dev = disk->private_data;

	spin_lock(&dev->lock);
	dev->users--;
	spin_unlock(&dev->lock);

}

/*
 * The ioctl() implementation
 */

int mem_disk_ioctl (struct block_device *bdev,
		 fmode_t mode,
                 unsigned int cmd, unsigned long arg)
{
	long size;
	struct hd_geometry geo;
	struct mem_disk_dev *dev = bdev->bd_disk->private_data;

	switch(cmd) {
	    case HDIO_GETGEO:
        /*
		 * Get geometry: since we are a virtual device, we have to make
		 * up something plausible.  So we claim 16 sectors, four heads,
		 * and calculate the corresponding number of cylinders.  We set the
		 * start of data at sector four.
		 */
		size = dev->size*(hardsect_size/KERNEL_SECTOR_SIZE);
		geo.cylinders = (size & ~0x3f) >> 6;
		geo.heads = 4;
		geo.sectors = 16;
		geo.start = 4;
		if (copy_to_user((void __user *) arg, &geo, sizeof(geo)))
			return -EFAULT;
		return 0;
	}

	return -ENOTTY; /* unknown command */
}



/*
 * The device operations structure.
 */
static struct block_device_operations mem_disk_ops = {
	.owner           = THIS_MODULE,
	.open 	         = mem_disk_open,
	.release 	 = mem_disk_release,
	.ioctl	         = mem_disk_ioctl
};


/*
 * Set up our internal device.
 */
static void setup_device(struct mem_disk_dev *dev){
	/*
	 * Get some memory.
	 */
	memset (dev, 0, sizeof (struct mem_disk_dev));
	dev->size = nsectors*hardsect_size;
	dev->data = vmalloc(dev->size);
	if (dev->data == NULL) {
		printk (KERN_NOTICE "------------------vmalloc failure.--------------\n");
		return;
	}
	spin_lock_init(&dev->lock);

	/*
	 * The I/O queue, depending on whether we are using our own
	 * make_request function or not.
	 */
	switch (request_mode) {

	    default:
		printk(KERN_NOTICE "Bad request mode %d, using simple\n", request_mode);
        	/* fall into.. */
	
	    case RM_SIMPLE:
		dev->queue = blk_init_queue(do_request, &dev->lock);
		if (dev->queue == NULL)
			goto out_vfree;
		break;
	}
	blk_queue_logical_block_size(dev->queue, hardsect_size);
	dev->queue->queuedata = dev;
	/*
	 * And the gendisk structure.
	 */
	dev->gd = alloc_disk(SBULL_MINORS);
	if (! dev->gd) {
		printk (KERN_NOTICE "alloc_disk failure\n");
		goto out_vfree;
	}
	dev->gd->major = mdisk_major;
	dev->gd->first_minor = 0;	
	dev->gd->fops = &mem_disk_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;

	snprintf (dev->gd->disk_name, 32, "mem_disk");
	set_capacity(dev->gd, nsectors*(hardsect_size/KERNEL_SECTOR_SIZE));
	add_disk(dev->gd);
	return;

  out_vfree:
	if (dev->data)
		vfree(dev->data);
}


static int load_smr_disk_data(struct file * disk_fp){   

	ssize_t ret;
	loff_t fsize;
	char * rw_ptr;
	loff_t pos;

	pos = 0;
	fsize = disk_fp->f_inode->i_size;	
	rw_ptr = mdisk_dev->data;

	if(fsize != TOTAL_SMR_BLOCKS_CNT * EXT2_BLOCK_SIZE){
		return -1;
	}	
	// read the whole smr disk 
	while(pos < fsize){
		ret = vfs_read(disk_fp,rw_ptr, EXT2_BLOCK_SIZE,&pos);
		rw_ptr += EXT2_BLOCK_SIZE;		
		if(ret < 0){
			DS_PRINTF((PRINT_PREFIX"load_smr_disk_data() error \n"));
			return -1;
		}
	}
	return 0;
}

static int save_smr_disk_data(struct file * disk_fp){
	ssize_t ret;
	loff_t fsize;
	char * rw_ptr;
	loff_t pos;

	pos = 0;
	fsize = TOTAL_SMR_BLOCKS_CNT * EXT2_BLOCK_SIZE;	
	rw_ptr = mdisk_dev->data;

	// read the whole smr disk 
	while(pos < fsize){
		ret = vfs_write(disk_fp,rw_ptr, EXT2_BLOCK_SIZE,&pos);
		rw_ptr += EXT2_BLOCK_SIZE;		
		if(ret < 0){
			DS_PRINTF((PRINT_PREFIX"save_smr_disk_data() error \n"));
			return -1;
		}
	}
	return 0;	
}


static sorted_vector * load_sorted_vector(struct file * ctrl_fp,loff_t * pos_ptr, unsigned int len){
	sorted_vector * vect = create_sorted_vector();
	
	if(len > 0){
		unsigned int i = 0;
		block_t * blks;
		blks = ALLOC_MEM(sizeof(vect->buckets[0]->blks[0]) * len);
		if(vfs_read(ctrl_fp,(char *)blks,sizeof(vect->buckets[0]->blks[0]) * len, pos_ptr) < 0){
			DS_PRINTF((PRINT_PREFIX"load_sorted_vector() error \n"));
		}
		for(i = 0; i < len; i++){
			insert_into_sorted_vector(vect,blks[i]);
		}
		FREE_MEM(blks);		
	}
	return vect;
}

static int save_sorted_vector(struct file * ctrl_fp,loff_t * pos_ptr, sorted_vector * vect){
	int len = get_vect_size(vect);
	//DS_PRINTF((PRINT_PREFIX"save_sorted_vector: len = %d \n",len));
	//DS_PRINTF((PRINT_PREFIX"sizeof(vect->buckets[0]->blks[0]) * len = %u \n",
	//				(unsigned int)(sizeof(vect->buckets[0]->blks[0]) * len)));
	if(len > 0){
		if(vfs_write(ctrl_fp,(char *)vect->buckets[0]->blks,
						sizeof(vect->buckets[0]->blks[0]) * len, pos_ptr) < 0){
			DS_PRINTF((PRINT_PREFIX"load_sorted_vector() error \n"));
			return -1;
		}
	}
	return 0;
}


static int save_smr_ctrl(struct file * ctrl_fp,loff_t * pos_ptr){
	smr_ctr_data * s_ctr = get_smr_ctrl_data();
	hdd_buf_ctr_data * b_ctr = get_hdd_buf_ctr_data();	
	ext2_meta_ctr * m_ctr = get_ext2_meta_ctr();
	int i;
	// 
	sorted_vector * vect;

	//DS_PRINTF((PRINT_PREFIX"..............save_smr_ctrl()....................\n"));
	// write struct smr_ctr_data object
	vect = s_ctr->fully_invalid_bands;
	s_ctr->vec_len = get_vect_size(vect);
	if(vfs_write(ctrl_fp,(char *)s_ctr,sizeof(*s_ctr),pos_ptr) < 0){
		return -1;
	}
	
	// write elements for fully_invalid_bands
	if(save_sorted_vector(ctrl_fp,pos_ptr,vect) < 0){
		return -1;
	}	
	// write struct hdd_buf_ctr_data object
	vect = b_ctr->free_hdd_bufs;
	b_ctr->vect_len = get_vect_size(vect);
	if( vfs_write(ctrl_fp,(char *)b_ctr,sizeof(*b_ctr),pos_ptr) < 0){
		return -1;
	}
	if(save_sorted_vector( ctrl_fp,pos_ptr,vect) < 0){
		return -1;
	}
	// write ext2_meta_ctr object;
	for(i = SB_IDX; i <= TMP_IDX; i++){
		m_ctr->len[i] = get_vect_size(m_ctr->meta_blks[i]);
		//DS_PRINTF((PRINT_PREFIX"m_ctr->len[%d] = %d \n",i,m_ctr->len[i]));
	}
	if( vfs_write(ctrl_fp,(char *)m_ctr,sizeof(*m_ctr),pos_ptr) < 0){
		return -1;
	}
	for(i = SB_IDX; i <= TMP_IDX; i++){
		if(save_sorted_vector( ctrl_fp,pos_ptr,m_ctr->meta_blks[i]) < 0){
			return -1;
		}
	}
	return 0;	

}


static int load_smr_ctrl(struct file * ctrl_fp){
	smr_ctr_data * s_ctr = get_smr_ctrl_data();
	hdd_buf_ctr_data * b_ctr = get_hdd_buf_ctr_data();
	ext2_meta_ctr * m_ctr = get_ext2_meta_ctr();
	
	//
	unsigned int len ;
	loff_t pos;
	loff_t * pos_ptr;
	int i;

	//DS_PRINTF((PRINT_PREFIX"..............load_smr_ctrl()...................."));
	pos = 0;
	pos_ptr = &pos;
	// read struct smr_ctr_data object
	if(vfs_read(ctrl_fp,(char *)s_ctr,sizeof(*s_ctr),pos_ptr) < 0){
		return -1;
	}
	// read elements for fully_invalid_bands
	len = s_ctr->vec_len;
	s_ctr->fully_invalid_bands = load_sorted_vector(ctrl_fp,pos_ptr,len);
	// read struct hdd_buf_ctr_data object
	if( vfs_read(ctrl_fp,(char *)b_ctr,sizeof(*b_ctr),pos_ptr) < 0){
		return -1;
	}
	len = b_ctr->vect_len;
	b_ctr->free_hdd_bufs = load_sorted_vector(ctrl_fp,pos_ptr,len);
	
	// read ext2_meta_ctr object
	if( vfs_read(ctrl_fp,(char *)m_ctr,sizeof(*m_ctr),pos_ptr) < 0){
		return -1;
	}
	for(i = SB_IDX; i <= TMP_IDX; i++){
		//DS_PRINTF((PRINT_PREFIX"m_ctr->len[%d] = %d \n",i,m_ctr->len[i]));
		m_ctr->meta_blks[i] = load_sorted_vector(ctrl_fp,pos_ptr,m_ctr->len[i]);
		//prt_sorted_vect(m_ctr->meta_blks[i]);
	}

	
	return 0;
}


static int load_system_data(void){
    struct file * disk_fp;
	struct file * ctrl_fp;
	int ret;
    mm_segment_t fs;

	ret = 0;
	fs =get_fs();
    disk_fp =filp_open(SMR_DISK_FILE_PATH,O_RDONLY,0644);
	ctrl_fp =filp_open(SMR_CTRL_FILE_PATH,O_RDONLY,0644);		

	set_fs(KERNEL_DS);
    if (IS_ERR(disk_fp) || IS_ERR(ctrl_fp)){
        DS_PRINTF((PRINT_PREFIX"load_system_data(): IS_ERR(disk_fp) || IS_ERR(disk_fp)\n"));
		// we should call init_smr_disk() later
        ret = -1;
		goto exit_load;
    } 
    // read disk file
	if(load_smr_disk_data(disk_fp) < 0){
		ret = -1;
		DS_PRINTF((PRINT_PREFIX" load_smr_disk(disk_fp) < 0\n"));
		goto exit_load;
	}
	// read ctrl file
	if(load_smr_ctrl(ctrl_fp) < 0){
		ret = -1;
		DS_PRINTF((PRINT_PREFIX" load_smr_ctrl(ctrl_fp) < 0 \n"));
		goto exit_load;
	}
exit_load:	
	set_fs(fs);
	if(!IS_ERR(disk_fp)){
    	filp_close(disk_fp,NULL);
	}
	if(!IS_ERR(ctrl_fp)){
    	filp_close(ctrl_fp,NULL);
	}
	//DS_PRINTF((PRINT_PREFIX"load_system_data() OK \n"));
	return ret;

}


static int save_system_data(void){
    struct file * disk_fp;
	struct file * ctrl_fp;
	int ret = 0;
	loff_t pos;
    mm_segment_t fs;

	fs =get_fs();
    disk_fp =filp_open(SMR_DISK_FILE_PATH,O_RDWR|O_CREAT,0666);
	ctrl_fp =filp_open(SMR_CTRL_FILE_PATH,O_RDWR|O_CREAT,0666);	
    if (IS_ERR(disk_fp) || IS_ERR(ctrl_fp)){
		// may ignore closing one file here
        DS_PRINTF((PRINT_PREFIX"save_system_data(): IS_ERR(disk_fp) || IS_ERR(disk_fp)\n"));
        ret = -1;
		goto exit_load;
    }
	
    
    set_fs(KERNEL_DS);
	if(save_smr_disk_data(disk_fp) < 0){
		ret = -1;
		DS_PRINTF((PRINT_PREFIX" save_smr_disk_data(disk_fp) < 0\n"));
		goto exit_load;
	}
	// another file , reset pos to 0
	pos = 0;
	if(save_smr_ctrl(ctrl_fp,&pos) < 0){
		ret = -1;
		DS_PRINTF((PRINT_PREFIX" save_smr_ctrl(ctrl_fp,&pos) < 0 \n"));
		goto exit_load;
	}
	
exit_load:	
	set_fs(fs);
	if(!IS_ERR(disk_fp)){
    	filp_close(disk_fp,NULL);
	}
	if(!IS_ERR(ctrl_fp)){
    	filp_close(ctrl_fp,NULL);
	}

	//DS_PRINTF((PRINT_PREFIX"save_system_data() OK \n"));
	return ret;	
}

static int __init mem_disk_init(void){
	
	DS_PRINTF((PRINT_PREFIX"--------------------mem_disk_init() begins:------------------------\n"));
	DS_PRINTF((PRINT_PREFIX "aware = %d favor = %d\n",aware,favor));

	/*
	 * Get registered.
	 */
	mdisk_major = register_blkdev(mdisk_major, "mem_disk");
	if (mdisk_major <= 0) {
		printk(KERN_WARNING "mem_disk: unable to get major number\n");
		return -EBUSY;
	}
	/*
	 * Allocate the device array, and initialize each one.
	 */
	mdisk_dev = kmalloc(sizeof (struct mem_disk_dev), GFP_KERNEL);
	if (mdisk_dev == NULL)
		goto out_unregister;
	setup_device(mdisk_dev);
	// try to load saved context . If it fails, we call init_smr_disk() to initialize the system
	if(load_system_data() < 0){
		DS_PRINTF((PRINT_PREFIX"load_system_data() < 0 \n"));
		init_smr_disk();
	}
#if 1
	// If it is an ext2 file system, we display the FS tree for test and fun :)
	if(is_ext2_fs()){
		DS_PRINTF((PRINT_PREFIX"There is an EXT2 file system on disk\n"));
		mark_all_free_blks();
		//display_ext2_meta_data();
		//traverse_ext2_fs();
		//print_all_meta_blocks();
		//
	}
#endif	
	// reset counters
	if(is_counter_reset()){
		DS_PRINTF((PRINT_PREFIX"reset all counters ................\n"));
		reset_all_counters();
	}
	
    DS_PRINTF((PRINT_PREFIX"mem_disk_init() ends. disk_size:%x\n",nsectors*hardsect_size));
	return 0;

  out_unregister:
	unregister_blkdev(mdisk_major, "mem_disk");
	return -ENOMEM;

}

static void mem_disk_exit(void){
	
	struct mem_disk_dev *dev = mdisk_dev;

	// 
	if (dev->gd) {
		del_gendisk(dev->gd);
		put_disk(dev->gd);
	}
	// clean up request queue
	if (dev->queue) {
		blk_cleanup_queue(dev->queue);
	}

	if(save){
		save_system_data();
	}
	// free memory disk
	if (dev->data){
		vfree(dev->data);
	}
	release_smr_disk();
	unregister_blkdev(mdisk_major, "mem_disk");
	kfree(mdisk_dev);

	DS_PRINTF((PRINT_PREFIX "mem_disk_exit() \n"));
}
	
module_init(mem_disk_init);
module_exit(mem_disk_exit);
