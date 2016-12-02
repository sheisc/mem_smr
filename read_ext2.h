#ifndef READ_EXT2_H
#define READ_EXT2_H

#include "ds_types.h"

// The available options are 1024, 2048 and 4096.
#define EXT2_BLOCK_SIZE     4096

// number of direct blocks in ext2_inode.i_block[EXT2_N_BLOCKS]
#define	EXT2_NDIR_BLOCKS		12
// offset of indirect block
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
// offset of double indirect block
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
// offset of triple indirect block
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
// number of blocks,  15
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)
// max length of ext2 file name
#define EXT2_NAME_LEN   255


#define EXT2_S_MASK     0xF000
//socket
#define EXT2_S_IFSOCK	0xC000
//symbolic link
#define EXT2_S_IFLNK	0xA000
//regular file
#define EXT2_S_IFREG	0x8000
//block device
#define EXT2_S_IFBLK	0x6000
//directory
#define EXT2_S_IFDIR	0x4000
//character device
#define EXT2_S_IFCHR	0x2000
//fifo
#define EXT2_S_IFIFO	0x1000

// super block is at offset 0x400 bytes
#define     FIRST_SUPER_BLOCK_OFFSET      0x400
// the block number of super block 0, only for 4K block
#define     SB0_BLOCK_NO      0
// bytes rounded to blocks
#define     BYTES_ALIGN_TO_BLOCKS(bytes)   (((bytes)+EXT2_BLOCK_SIZE-1)/EXT2_BLOCK_SIZE)
// convert blocks to bytes
#define     BLOCKS_2_BYTES(blocks)  ((blocks)*EXT2_BLOCK_SIZE)
// the rounded bytes
#define     ALIGNED_BYTES(bytes)    BLOCKS_2_BYTES(BYTES_ALIGN_TO_BLOCKS(bytes))

// block numbers per block
#define     BLK_NUM_PER_BLK        (EXT2_BLOCK_SIZE/(sizeof(__u32)))
// the number of data blocks via indirect indexing
#define     INDIRECT_BLKS           (BLK_NUM_PER_BLK)
// the number of data blocks via double indirect indexing
#define     DOUBLE_IND_BLKS         (BLK_NUM_PER_BLK * BLK_NUM_PER_BLK)
// the number of data blocks via triple indirect indexing
#define     TRIPLE_IND_BLKS         (BLK_NUM_PER_BLK * BLK_NUM_PER_BLK * BLK_NUM_PER_BLK)
// the inode number of ext2 root directory
#define     EXT2_ROOT_INO           2



// sizeof(ext2_super_block) is 1024 bytes
struct ext2_super_block {
    /*00*/
    __u32 s_inodes_count;      /* total number of inodes */
    __u32 s_blocks_count;      /* total number of blocks  */
    __u32 s_r_blocks_count;    /* the number of reserved blocks */
    __u32 s_free_blocks_count; /* the number of free blocks  */
    /*10*/
    __u32 s_free_inodes_count; /* the number of free inodes */
    __u32 s_first_data_block;  /* the block number of first data block managed by group descriptors */
    // 1 << (s_log_block_size + 10)
    __u32 s_log_block_size;    /* block size:   0 ,1,2 for {1K,2K,4K} repectively */
    __s32 s_log_frag_size;     /*  */
    /*20*/
    __u32 s_blocks_per_group;  /* the number of blocks per block group */
    __u32 s_frags_per_group;   /*  */
    __u32 s_inodes_per_group;  /* the number of inodes per block group */
    __u32 s_mtime;             /* Mount time */
    /*30*/
    __u32 s_wtime;             /* Write time */
    __u16 s_mnt_count;         /* Mount count */
    __s16 s_max_mnt_count;     /* Maximal mount count */
    __u16 s_magic;             /* Magic Number of ext2 file system */
    __u16 s_state;             /* File system state */
    __u16 s_errors;            /* Behaviour when detecting errors */
    __u16 s_minor_rev_level;   /* minor revision level */
    /*40*/
    __u32 s_lastcheck;         /* time of last check */
    __u32 s_checkinterval;     /* max. time between checks */
    __u32 s_creator_os;        /*  */
    __u32 s_rev_level;         /* Revision level */
    /*50*/
    __u16 s_def_resuid;        /* Default uid for reserved blocks */
    __u16 s_def_resgid;        /* Default gid for reserved blocks */
    __u32 s_first_ino;         /* First non-reserved inode */
    __u16 s_inode_size;        /* size of inode structure */
    __u16 s_block_group_nr;    /* block group # of this superblock */
    __u32 s_feature_compat;    /* compatible feature set */
    /*60*/
    __u32 s_feature_incompat;  /* incompatible feature set */
    __u32 s_feature_ro_compat; /* readonly-compatible feature set */
    /*68*/
    __u8  s_uuid[16];          /* 128-bit uuid for volume */
    /*78*/
    char  s_volume_name[16];   /* volume name */
    /*88*/
    char  s_last_mounted[64];  /* directory where last mounted */
    /*C8*/
    __u32 s_algorithm_usage_bitmap; /*  */
    __u8  s_prealloc_blocks;        /*  */
    __u8  s_prealloc_dir_blocks;    /*  */
    __u16 s_padding1;               /*  */
    /*D0*/
    __u8  s_journal_uuid[16]; /* uuid of journal superblock */
    /*E0*/
    __u32 s_journal_inum;     /* the inode number of journal file, a special device file in ext3 */
    __u32 s_journal_dev;      /* the device number of journal file */
    __u32 s_last_orphan;      /* start of list of inodes to delete */
    /*EC*/
    //__u32 s_reserved[197];    /*  */
    // introduced in ext4
    __u32	s_hash_seed[4];		/* HTREE hash seed */
    __u8	s_def_hash_version;	/* Default hash version to use */
    __u8	s_reserved_char_pad;
    __u16	s_reserved_word_pad;
    __u32	s_default_mount_opts;
    __u32	s_first_meta_bg; 	/* First metablock block group */
    __u32	s_reserved[190];	/* Padding to the end of the block */
};
// sizeof(ext2_inode) is 128 bytes
struct ext2_inode {
    __u16 i_mode;    /* File mode */
    __u16 i_uid;     /* Low 16 bits of Owner Uid */
    __u32 i_size;    /* the size of file, in bytes */
    __u32 i_atime;   /* Access time */
    __u32 i_ctime;   /* Creation time */
    __u32 i_mtime;   /* Modification time */
    __u32 i_dtime;   /* Deletion Time */
    __u16 i_gid;     /* Low 16 bits of Group Id */
    __u16 i_links_count;          /* Links count */
    __u32 i_blocks;               /* the number of blocks */
    __u32 i_flags;                /* File flags */
    __u32 l_i_reserved1;          /*  */
    __u32 i_block[EXT2_N_BLOCKS]; /* index table */
    __u32 i_generation;           /*  */
    __u32 i_file_acl;             /*  */
    __u32 i_dir_acl;              /*  */
    __u32 i_faddr;                /*  */
    __u8  l_i_frag;               /*  */
    __u8  l_i_fsize;              /*  */
    __u16 i_pad1;                 /*  */
    __u16 l_i_uid_high;           /*  */
    __u16 l_i_gid_high;           /*  */
    __u32 l_i_reserved2;          /*  */
};

#if 0
/*
 * Structure of a directory entry
 */
struct ext2_dir_entry {
    __u32	inode;			/* Inode number */
    __u16	rec_len;		/* Directory entry length */
    __u16	name_len;		/* Name length */
    char	name[];			/* File name, up to EXT2_NAME_LEN */
};

/*
 * The new version of the directory entry.  Since EXT2 structures are
 * stored in intel byte order, and the name_len field could never be
 * bigger than 255 chars, it's safe to reclaim the extra byte for the
 * file_type field.
 */
struct ext2_dir_entry_2 {
    __u32	inode;			/* Inode number */
    __u16	rec_len;		/* Directory entry length */
    __u8	name_len;		/* Name length */
    __u8	file_type;
    char	name[];			/* File name, up to EXT2_NAME_LEN */
};
#endif

struct ext2_dir_entry_2 {
    __u32 inode;    /* Inode number */
    __u16 rec_len;  /* Directory entry length */
    __u8  name_len; /* Name length */
    __u8  file_type;
    char  name[EXT2_NAME_LEN]; /* File name */
};

// sizeof(struct ext2_group_desc) is 32 bytes
struct ext2_group_desc{
    __u32 bg_block_bitmap;      /* the block number of block bitmap */
    __u32 bg_inode_bitmap;      /* the block number of inode bitmap */
    __u32 bg_inode_table;       /* the block number of inodes table */
    __u16 bg_free_blocks_count; /* the number of free blocks in block group */
    __u16 bg_free_inodes_count; /* the number of free inodes in block group */
    __u16 bg_used_dirs_count;   /* the number of used directories*/
    __u16 bg_pad;               /*  */
    __u32 bg_reserved[3];       /*  */
};
////////////////////////////////////////////////////////////////////////////////////////

void print_ext2_super_block(struct ext2_super_block * sbp);
void print_ext2_group_desc(struct ext2_group_desc * gdp);
void print_ext2_dir_entry_2(struct ext2_dir_entry_2 * dep);
void print_ext2_inode(struct ext2_inode * ip);

int read_ext2_file_blocks(__u32 ino, char * buf, int from,int n);
void visit_ext2_dir(__u32 ino, int depth);
void traverse_ext2_fs(void);
void display_ext2_meta_data(void);

#endif // READ_EXT2_H
