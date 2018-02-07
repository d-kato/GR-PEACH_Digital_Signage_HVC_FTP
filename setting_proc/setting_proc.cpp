#include "mbed.h"
#include "DisplayDebugLog.h"
#include "FileAnalysis.h"
#include "setting_proc.h"

static bool check_pram(char * p_buf, const char * p_pram, int * pram_len) {
    *pram_len = strlen(p_pram);

    if (memcmp(p_buf, p_pram, *pram_len) == 0) {
        return true;
    }
    return false;
}

void check_setting(setting_t * p_setting) {
    uint8_t * work_buff = new uint8_t[1024];
    size_t read_size;

    if (work_buff == NULL) {
        DrawDebugLog("Memory error check_setting\r\n");
        return;
    }

    memset(work_buff, 0, 1024);
    read_size = GetFileData(work_buff, 1024, "setting.txt");
    if (read_size != 0) {
        char* token = (char *)work_buff;
        char* savept = NULL;

        DrawDebugLog("\"setting.txt\" is being read...\r\n");
        while (token) {
            token = strtok_r(token,"\r\n", &savept);
            if (token != NULL) {
                int len;

//                printf("%s\r\n", token);
                if (check_pram(token, "FACE_DETECTION_SLIDE=", &len)) {
                    uint8_t wk_buf[1];
                    if (GetFileData(wk_buf, 1, &token[len]) == 1) {
                        strcpy(p_setting->p_face_file, &token[len]);
                    }
                } else if (check_pram(token, "SLIDE_DISPLAY_TIME=", &len)) {
                    *p_setting->p_disp_wait_time = atoi(&token[len]);
                } else if (check_pram(token, "FACE_THRESHOLD=", &len)) {
                    p_setting->p_rec_cfg->threshold.dtThreshold = atoi(&token[len]);
                } else if (check_pram(token, "FACE_SIZE_RANGE_MIN=", &len)) {
                    p_setting->p_rec_cfg->sizeRange.dtMinSize = atoi(&token[len]);
                } else if (check_pram(token, "FACE_SIZE_RANGE_MAX=", &len)) {
                    p_setting->p_rec_cfg->sizeRange.dtMaxSize = atoi(&token[len]);
                } else if (check_pram(token, "FACE_POSE=", &len)) {
                    p_setting->p_rec_cfg->pose = atoi(&token[len]);
                } else if (check_pram(token, "FACE_ANGLE=", &len)) {
                    p_setting->p_rec_cfg->angle = atoi(&token[len]);
                } else if (check_pram(token, "STB_RETRYCOUNT=", &len)) {
                    p_setting->p_rec_cfg->stb_retrycount = atoi(&token[len]);
                } else if (check_pram(token, "STB_POSSTEADINESS=", &len)) {
                    p_setting->p_rec_cfg->stb_possteadiness = atoi(&token[len]);
                } else if (check_pram(token, "STB_SIZESTEADINESS=", &len)) {
                    p_setting->p_rec_cfg->stb_sizesteadiness = atoi(&token[len]);
                } else if (check_pram(token, "STB_PE_FRAME=", &len)) {
                    p_setting->p_rec_cfg->stb_pe_frame = atoi(&token[len]);
                } else if (check_pram(token, "STB_PE_ANGLEUDMIN=", &len)) {
                    p_setting->p_rec_cfg->stb_pe_angleudmin = atoi(&token[len]);
                } else if (check_pram(token, "STB_PE_ANGLEUDMAX=", &len)) {
                    p_setting->p_rec_cfg->stb_pe_angleudmax = atoi(&token[len]);
                } else if (check_pram(token, "STB_PE_ANGLELRMIN=", &len)) {
                    p_setting->p_rec_cfg->stb_pe_anglelrmin = atoi(&token[len]);
                } else if (check_pram(token, "STB_PE_ANGLELRMAX=", &len)) {
                    p_setting->p_rec_cfg->stb_pe_anglelrmax = atoi(&token[len]);
                } else if (check_pram(token, "STB_PE_THRESHOLD=", &len)) {
                    p_setting->p_rec_cfg->stb_pe_threshold = atoi(&token[len]);
                } else if (check_pram(token, "FACE_IN_SIZE=", &len)) {
                    p_setting->p_rec_cfg->face_in_size = atoi(&token[len]);
                } else if (check_pram(token, "FACE_OUT_CNT=", &len)) {
                    p_setting->p_rec_cfg->face_out_cnt = atoi(&token[len]);
                } else if (check_pram(token, "IMAGE_POS_X=", &len)) {
                    p_setting->p_rec_cfg->image_pos_x = atoi(&token[len]);
                } else if (check_pram(token, "IMAGE_POS_Y=", &len)) {
                    p_setting->p_rec_cfg->image_pos_y = atoi(&token[len]);
                } else if (check_pram(token, "DISP_IMAGE=", &len)) {
                    p_setting->p_rec_cfg->disp_image = atoi(&token[len]);
                } else if (check_pram(token, "DISP_TRACKING_ID=", &len)) {
                    p_setting->p_rec_cfg->disp_tracking_id = atoi(&token[len]);
                } else if (check_pram(token, "DISP_FACE_SIZE=", &len)) {
                    p_setting->p_rec_cfg->disp_face_size = atoi(&token[len]);
                } else if (check_pram(token, "DISP_HUMAN_NUM=", &len)) {
                    p_setting->p_rec_cfg->disp_human_num = atoi(&token[len]);
                } else if (check_pram(token, "TELOP_POS_Y=", &len)) {
                    p_setting->p_telop_cfg->pos_y = atoi(&token[len]);
                } else if (check_pram(token, "TELOP_SPEED=", &len)) {
                    p_setting->p_telop_cfg->speed = atoi(&token[len]);
                }
                token = savept;
            }
        }
        DrawDebugLog("done\r\n");
    } else {
        DrawDebugLog("\"setting.txt\" was not found\r\n");
    }

    delete[] work_buff;
}

