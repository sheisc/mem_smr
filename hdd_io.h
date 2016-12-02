#ifndef DS_IO_H
#define DS_IO_H
#include "ds_counter.h"
#include "ds_types.h"
/////////////////////////////////////////////////////////////////////////////////////




///////////////////////////////////////////////////////////////////////////////////

int init_hdd_io(void);
int release_hdd_io(void);
ds_counter * get_hdd_rw_counter(void);
ds_size_t read_hdd_blocks(void * buf, ds_offset_t offset_in_blocks, ds_size_t num_in_blocks);
ds_size_t write_hdd_blocks(const void *buf, ds_offset_t offset_in_blocks, ds_size_t num_in_blocks);



#endif // DS_IO_H
