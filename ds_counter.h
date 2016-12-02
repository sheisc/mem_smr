#ifndef COUNTER_H
#define COUNTER_H


#define     DS_ENABLE                  1
#define     DS_DISABLE                 0

typedef struct {
    int enable;
    unsigned long read_count;
    unsigned long write_count;
} ds_counter;
////////////////////////////////////////////////////////////////////////

int is_counter_enabled(ds_counter * cnt);
void set_counter_state(ds_counter * cnt,int enable);

////////////////////////////////////////////////////////////////////////
#endif // COUNTER_H
