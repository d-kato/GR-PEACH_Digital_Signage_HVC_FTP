#ifndef TELOP_PROCESSING_H
#define TELOP_PROCESSING_H

#include "DisplayBace.h"

typedef struct {
    DisplayBase * p_display;
    unsigned short pos_y;
    int speed;
} telop_task_cfg_t;

extern void telop_data_save(const char * file_name);
extern void telop_task(telop_task_cfg_t * p_cfg);

#endif
