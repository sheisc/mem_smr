#!/bin/bash


module="mem_smr"
device="mem_disk"
mode="664"



# It should be consistent with Macro SMR_DISK_FILE_PATH in smr.h
SMR_DISK_FILE_PATH=./image/smr_disk.img
SMR_CTRL_FILE_PATH=./image/smr_disk_ctrl.img
EXT2_BLOCK_SIZE=4096
TOTAL_SMR_BLOCKS_CNT=$((256*1024))
#
#LOOP_DEVICE=/dev/loop0

# Group: since distributions do it differently, look for wheel or use staff
if grep '^staff:' /etc/group > /dev/null; then
    group="staff"
else
    group="wheel"
fi

###################################load ext2 kernel module###################################
#loaded=`lsmod|grep ext2`

# not loaded yet
#if [ x"$loaded" = x ]; then
#	echo "load ext2 module, please wait ...."
#	sudo insmod ./ext2/ext2.ko
#fi



###################################create disk image and file system on it###################
#if [ ! -f $SMR_DISK_FILE_PATH ]; then
#	rm -f $SMR_CTRL_FILE_PATH
#	echo "creating disk image, please wait...."
#	dd if=/dev/zero of=$SMR_DISK_FILE_PATH bs=$EXT2_BLOCK_SIZE count=$TOTAL_SMR_BLOCKS_CNT
#	losetup $LOOP_DEVICE $SMR_DISK_FILE_PATH
#	mkfs -T myext2 -I 128 $LOOP_DEVICE
#	echo "losetup -d $LOOP_DEVICE"
#	losetup -d $LOOP_DEVICE
#fi
#############################################################################################

##################################load mem_smr kernel module ##################################
echo "load $module kernel module, please wait ........"
#/sbin/insmod -f ./$module.ko $* 
/sbin/insmod ./$module.ko $* 

major=`cat /proc/devices | awk "\\$2==\"$device\" {print \\$1}"`

# Remove stale nodes and replace them, then give gid and perms

rm -f /dev/${device}

mknod /dev/${device} b $major 0


##################################mount ext2 file system#### ##################################
chgrp $group /dev/${device}
chmod $mode  /dev/${device}

IMAGE_DIR=./image

# create the directory if necessary
if [ ! -d $MNT_DIR ]; then
	mkdir -p $IMAGE_DIR
fi

if [ ! -f $SMR_DISK_FILE_PATH ]; then
	echo "make ext2 file system on /dev/${device}"
	sudo mkfs -T ext2 -I 128 /dev/${device}
fi
#
##############################################################################
# set the directory prefix

MNT_DIR=./mnt

# create the directory if necessary
if [ ! -d $MNT_DIR ]; then
	mkdir -p $MNT_DIR
fi


sudo mount -t ext2 /dev/${device} $MNT_DIR
sudo chmod a+w $MNT_DIR

