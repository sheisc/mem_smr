#!/bin/bash
module="mem_smr"
device="mem_disk"

echo "umount ./mnt, please wait ........"
sudo umount ./mnt

# invoke rmmod with all arguments we got
echo "remove kernel module, please wait ........"
/sbin/rmmod $module 

# Remove stale nodes
rm -f /dev/${device}





