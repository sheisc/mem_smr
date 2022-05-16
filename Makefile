# Comment/uncomment the following line to disable/enable debugging
#DEBUG = y


# Add your debugging flag (or not) to EXTRA_CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSBULL_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)
EXTRA_CFLAGS += -I..

ifneq ($(KERNELRELEASE),)
# call from kernel build system
#mem_smr-objs := mem_disk.o ds_counter.o hash_set.o nvm_io.o ds_utils.o hdd_buf.o read_ext2.o stl.o hdd_io.o
mem_smr-objs := mem_disk.o hdd_io.o ds_counter.o ds_utils.o hash_set.o hdd_buf.o stl.o read_ext2.o ext2_meta.o
obj-m	:= mem_smr.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif


clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.symvers modules.order mnt .cache.mk *.o.ur-safe
depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif
