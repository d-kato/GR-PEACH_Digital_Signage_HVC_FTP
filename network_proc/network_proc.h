
#ifndef NETWORK_PROCT_H
#define NETWORK_PROCT_H
#include "SdUsbConnect.h"

extern bool network_connect(const char * mount_path, SdUsbConnect * p_storage);
extern bool init_timetable(uint32_t * p_total_file_num);
extern bool check_timetable(uint32_t * p_total_file_num);

#endif
