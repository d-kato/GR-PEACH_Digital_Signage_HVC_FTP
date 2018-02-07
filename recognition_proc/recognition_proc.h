#ifndef RECOGNITION_PROCESSING_H
#define RECOGNITION_PROCESSING_H

#include "DisplayBace.h"
#include "HVCApi.h"
#include "HVCDef.h"
#include "HVCExtraUartFunc.h"

#define BODY_THRESHOLD_DEFAULT             500            /* Threshold for Human Body Detection */
#define FACE_THRESHOLD_DEFAULT             500            /* Threshold for Face Detection */
#define HAND_THRESHOLD_DEFAULT             500            /* Threshold for Hand Detection */
#define REC_THRESHOLD_DEFAULT              500            /* Threshold for Face Recognition */

#define BODY_SIZE_RANGE_MIN_DEFAULT        180            /* Human Body Detection minimum detection size */
#define BODY_SIZE_RANGE_MAX_DEFAULT       1000            /* Human Body Detection maximum detection size */
#define HAND_SIZE_RANGE_MIN_DEFAULT        100            /* Hand Detection minimum detection size */
#define HAND_SIZE_RANGE_MAX_DEFAULT       1000            /* Hand Detection maximum detection size */
#define FACE_SIZE_RANGE_MIN_DEFAULT        180            /* Face Detection minimum detection size */
#define FACE_SIZE_RANGE_MAX_DEFAULT       1000            /* Face Detection maximum detection size */

#define FACE_POSE_DEFAULT                    0            /* Face Detection facial pose (frontal face)*/
#define FACE_ANGLE_DEFAULT                   0            /* Face Detection roll angle (б▐15бы)*/

/* STB */
#define STB_RETRYCOUNT_DEFAULT              10            /* Retry Count for STB */
#define STB_POSSTEADINESS_DEFAULT           30            /* Position Steadiness for STB */
#define STB_SIZESTEADINESS_DEFAULT          30            /* Size Steadiness for STB */
#define STB_PE_FRAME_DEFAULT                 8            /* Complete Frame Count for property estimation in STB */
#define STB_PE_ANGLEUDMIN_DEFAULT          -15            /* Up/Down face angle minimum value for property estimation in STB */
#define STB_PE_ANGLEUDMAX_DEFAULT           20            /* Up/Down face angle maximum value for property estimation in STB */
#define STB_PE_ANGLELRMIN_DEFAULT          -20            /* Left/Right face angle minimum value for property estimation in STB */
#define STB_PE_ANGLELRMAX_DEFAULT           20            /* Left/Right face angle maximum value for property estimation in STB */
#define STB_PE_THRESHOLD_DEFAULT           300            /* Threshold for property estimation in STB */

typedef struct {
    INT32         execFlag;
    HVC_THRESHOLD threshold;
    HVC_SIZERANGE sizeRange;
    INT32         pose;
    INT32         angle;
    int           stb_retrycount;
    int           stb_possteadiness;
    int           stb_sizesteadiness;
    int           stb_pe_frame;
    int           stb_pe_angleudmin;
    int           stb_pe_angleudmax;
    int           stb_pe_anglelrmin;
    int           stb_pe_anglelrmax;
    int           stb_pe_threshold;
    int           face_in_size;
    int           face_out_cnt;
    int           image_pos_x;
    int           image_pos_y;
    int           disp_image;
    int           disp_tracking_id;
    int           disp_face_size;
    int           disp_human_num;
} recognition_setting_t;

extern void recognition_task(DisplayBase * p_display);

extern recognition_setting_t * GetRecognitionSettingPointer(void);
extern void SetSettingReq(void);

extern int GetMmaleNum(uint8_t age);
extern int GetFemaleNum(uint8_t age);
extern bool IsHumanNumChange(void);

#endif
