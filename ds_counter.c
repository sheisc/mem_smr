#include "ds_counter.h"

////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////


/**
 * @brief set_counter_state
 *      enable /disable a counter @cnt
 * @param cnt
 * @param enable
 */
void set_counter_state(ds_counter * cnt,int enable){
    cnt->enable = enable;
}


int is_counter_enabled(ds_counter * cnt){
    return cnt->enable;
}



