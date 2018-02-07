#ifndef SETTING_PROCESSING_H
#define SETTING_PROCESSING_H

#include "recognition_proc.h"
#include "telop_proc.h"

typedef struct {
    char * p_face_file;
    int * p_disp_wait_time;
    recognition_setting_t * p_rec_cfg;
    telop_task_cfg_t * p_telop_cfg;
} setting_t;

extern void check_setting(setting_t * p_setting);

#endif
