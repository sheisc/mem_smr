# MEM_SMR
MEM_SMR: A Linux Kernel module for simulating Shingled Magnetic Recording, with EXT2-file-system-aware ability

## How to build 

```sh
iron@CSE:github$ pwd
/home/iron/github

iron@CSE:github$ git clone https://github.com/sheisc/mem_smr.git

iron@CSE:github$ cd mem_smr

iron@CSE:mem_smr$ make
```

## How to use

```sh
iron@CSE:mem_smr$ sudo ./load.sh 
load mem_smr kernel module, please wait ........
make ext2 file system on /dev/mem_disk
mke2fs 1.44.1 (24-Mar-2018)

Warning: the fs_type ext2 is not defined in mke2fs.conf

Creating filesystem with 262144 4k blocks and 65536 inodes
Filesystem UUID: 2f7b5169-20a0-4b23-9f84-35644107dc34
Superblock backups stored on blocks: 
	32768, 98304, 163840, 229376

Allocating group tables: done                            
Writing inode tables: done                            
Writing superblocks and filesystem accounting information: done
```

We have mounted /dev/mem_disk to ~/github/mem_smr/mnt.

## How to unmount

```sh
iron@CSE:mem_smr$ sudo ./unload.sh 
umount ./mnt, please wait ........
remove kernel module, please wait ........
```

## Please refer to document.pdf for more details