#include "mbed.h"
#include "DisplayBace.h"
#include "rtos.h"
#include "AsciiFont.h"
#include "USBHostSerial.h"
#include "recognition_proc.h"
#include "STBWrap.h"
#include "DisplayEfect.h"
#include "EasyAttach_CameraAndLCD.h"
#include "DisplayDebugLog.h"

#define UART_SETTING_TIMEOUT              1000            /* HVC setting command signal timeout period */
#define UART_EXECUTE_TIMEOUT               300            /* HVC execute command signal timeout period */

#define SENSOR_ROLL_ANGLE_DEFAULT            0            /* Camera angle setting */
#define USER_ID_NUM_MAX                     10

#define IMAGE_WIDTH                      (160)
#define IMAGE_HEIGHT                     (120)

#define DISP_PIXEL_WIDTH                 IMAGE_WIDTH
#define DISP_PIXEL_HEIGHT                (IMAGE_HEIGHT + (AsciiFont::CHAR_PIX_HEIGHT + 1) * 3 + 1)

/* RESULT BUFFER Parameter GRAPHICS_LAYER_2 */
#define RESULT_BUFFER_BYTE_PER_PIXEL  (2u)
#define RESULT_BUFFER_STRIDE          (((DISP_PIXEL_WIDTH * RESULT_BUFFER_BYTE_PER_PIXEL) + 31u) & ~31u)

static bool setting_req = false;
static recognition_setting_t setting = {
    HVC_ACTIV_FACE_DETECTION | HVC_ACTIV_FACE_DIRECTION | HVC_ACTIV_AGE_ESTIMATION | HVC_ACTIV_GENDER_ESTIMATION | HVC_ACTIV_EXPRESSION_ESTIMATION,
    { BODY_THRESHOLD_DEFAULT, HAND_THRESHOLD_DEFAULT, FACE_THRESHOLD_DEFAULT, REC_THRESHOLD_DEFAULT},
    { BODY_SIZE_RANGE_MIN_DEFAULT, BODY_SIZE_RANGE_MAX_DEFAULT, HAND_SIZE_RANGE_MIN_DEFAULT,
      HAND_SIZE_RANGE_MAX_DEFAULT, FACE_SIZE_RANGE_MIN_DEFAULT, FACE_SIZE_RANGE_MAX_DEFAULT},
    FACE_POSE_DEFAULT,
    FACE_ANGLE_DEFAULT,
    STB_RETRYCOUNT_DEFAULT,
    STB_POSSTEADINESS_DEFAULT,
    STB_SIZESTEADINESS_DEFAULT,
    STB_PE_FRAME_DEFAULT,
    STB_PE_ANGLEUDMIN_DEFAULT,
    STB_PE_ANGLEUDMAX_DEFAULT,
    STB_PE_ANGLELRMIN_DEFAULT,
    STB_PE_ANGLELRMAX_DEFAULT,
    STB_PE_THRESHOLD_DEFAULT,
    0,                                             // face_in_size
    10,                                            // face_out_cnt
    (LCD_PIXEL_WIDTH - IMAGE_WIDTH - 5),           // image_pos_x
    5,                                             // image_pos_y
    0,                                             // disp_image
    0,                                             // disp_tracking_id
    0,                                             // disp_face_size
    0                                              // disp_human_num
};
static USBHostSerial serial;

#if defined(__ICCARM__)
/* 32 bytes aligned */
#pragma data_alignment=32
static uint8_t user_frame_buffer_result[RESULT_BUFFER_STRIDE * DISP_PIXEL_HEIGHT]@ ".mirrorram";
#else
/* 32 bytes aligned */
static uint8_t user_frame_buffer_result[RESULT_BUFFER_STRIDE * DISP_PIXEL_HEIGHT]__attribute((section("NC_BSS"),aligned(32)));
#endif

static char str_human_num[64];
static int str_draw_x = 0;
static int str_draw_y = 0;
static int male_total[10] = {0};
static int female_total[10] = {0};
static bool human_num_change = false;
static AsciiFont ascii_font_human(user_frame_buffer_result, DISP_PIXEL_WIDTH, DISP_PIXEL_HEIGHT,
                                  RESULT_BUFFER_STRIDE, RESULT_BUFFER_BYTE_PER_PIXEL, 0x000000f0);
static int image_pos_x;
static int image_pos_y;

/****** Image Recognition ******/
extern "C" int UART_SendData(int inDataSize, UINT8 *inData) {
    return serial.writeBuf((char *)inData, inDataSize);
}

extern "C" int UART_ReceiveData(int inTimeOutTime, int inDataSize, UINT8 *outResult) {
    return serial.readBuf((char *)outResult, inDataSize, inTimeOutTime);
}

void SetSettingReq(void) {
    setting_req = true;
}

recognition_setting_t * GetRecognitionSettingPointer(void) {
    return &setting;
}

int GetMmaleNum(uint8_t age) {
    if (age <= 10) {
        return male_total[age];
    }
    return 0;
}

int GetFemaleNum(uint8_t age) {
    if (age <= 10) {
        return female_total[age];
    }
    return 0;
}

bool IsHumanNumChange(void) {
    bool ret = human_num_change;

    if (ret != false) {
        human_num_change = false;
    }
    return ret;
}

static void EraseImage(void) {
    if (setting.disp_image == 0) {
        return;
    }

    for (int i = 0; i < (IMAGE_HEIGHT * RESULT_BUFFER_STRIDE); i += 2) {
        user_frame_buffer_result[i+0] = 0x00;
        user_frame_buffer_result[i+1] = 0xf0;
    }
}

static void DrawImage(int x, int y, int nWidth, int nHeight, UINT8 *unImageBuffer) {
    int idx_base;
    int idx_w = 0;
    int i;
    int j;
    int idx_r = 0;

    if (setting.disp_image == 0) {
        return;
    }

    idx_base = (x + (DISP_PIXEL_WIDTH * y)) * RESULT_BUFFER_BYTE_PER_PIXEL;

    for (i = 0; i < nHeight; i++) {
        idx_w = idx_base + (DISP_PIXEL_WIDTH * RESULT_BUFFER_BYTE_PER_PIXEL * i);
        for (j = 0; j < nWidth; j++) {
            uint32_t wk_data = (unImageBuffer[idx_r] / 16);
            user_frame_buffer_result[idx_w+0] = (wk_data << 4) + (wk_data);
            user_frame_buffer_result[idx_w+1] = 0xf0 + (wk_data);
            idx_w += 2;
            idx_r++;
        }
    }
}

static void DrawSquare(int x, int y, int size, uint32_t const colour) {
    int wk_x;
    int wk_y;
    int wk_w = 0;
    int wk_h = 0;
    int idx_base;
    int wk_idx;
    int i;
    int j;
    uint8_t coller_pix[RESULT_BUFFER_BYTE_PER_PIXEL];  /* ARGB4444 */
    bool l_draw = true;
    bool r_draw = true;
    bool t_draw = true;
    bool b_draw = true;

    if (setting.disp_image == 0) {
        return;
    }

    if ((x - (size / 2)) < 0) {
        l_draw = false;
        wk_w += x;
        wk_x = 0;
    } else {
        wk_w += (size / 2);
        wk_x = x - (size / 2);
    }

    if ((x + (size / 2)) >= 1600) {
        r_draw = false;
        wk_w += (1600 - x);
    } else {
        wk_w += (size / 2);
    }

    if ((y - (size / 2)) < 0) {
        t_draw = false;
        wk_h += y;
        wk_y = 0;
    } else {
        wk_h += (size / 2);
        wk_y = y - (size / 2);
    }

    if ((y + (size / 2)) >= 1200) {
        b_draw = false;
        wk_h += (1200 - y);
    } else {
        wk_h += (size / 2);
    }

    wk_x = wk_x / 10;
    wk_y = wk_y / 10;
    wk_w = wk_w / 10;
    wk_h = wk_h / 10;

    str_draw_x = wk_x;
    str_draw_y = wk_y + wk_h + 1;

    idx_base = (wk_x + (DISP_PIXEL_WIDTH * wk_y)) * RESULT_BUFFER_BYTE_PER_PIXEL;

    /* Select color */
    coller_pix[0] = (colour >> 8) & 0xff;  /* 4:Green 4:Blue */
    coller_pix[1] = colour & 0xff;         /* 4:Alpha 4:Red  */

    /* top */
    if (t_draw) {
        wk_idx = idx_base;
        for (j = 0; j < wk_w; j++) {
            user_frame_buffer_result[wk_idx++] = coller_pix[0];
            user_frame_buffer_result[wk_idx++] = coller_pix[1];
        }
    }

    /* middle */
    for (i = 1; i < (wk_h - 1); i++) {
        wk_idx = idx_base + (DISP_PIXEL_WIDTH * RESULT_BUFFER_BYTE_PER_PIXEL * i);
        if (l_draw) {
            user_frame_buffer_result[wk_idx + 0] = coller_pix[0];
            user_frame_buffer_result[wk_idx + 1] = coller_pix[1];
        }
        wk_idx += (wk_w - 1) * 2;
        if (r_draw) {
            user_frame_buffer_result[wk_idx + 0] = coller_pix[0];
            user_frame_buffer_result[wk_idx + 1] = coller_pix[1];
        }
    }

    /* bottom */
    if (b_draw) {
        wk_idx = idx_base + (DISP_PIXEL_WIDTH * RESULT_BUFFER_BYTE_PER_PIXEL * (wk_h - 1));
        for (j = 0; j < wk_w; j++) {
            user_frame_buffer_result[wk_idx++] = coller_pix[0];
            user_frame_buffer_result[wk_idx++] = coller_pix[1];
        }
    }
}

static void DrawString(const char * str, uint32_t const colour) {
    if (setting.disp_image == 0) {
        return;
    }
    AsciiFont ascii_font(user_frame_buffer_result, IMAGE_WIDTH, IMAGE_HEIGHT,
                            RESULT_BUFFER_STRIDE, RESULT_BUFFER_BYTE_PER_PIXEL, 0x000000f0);

    ascii_font.Erase(0x000000f0, str_draw_x, str_draw_y,
                     (AsciiFont::CHAR_PIX_WIDTH * strlen(str) + 2),
                     (AsciiFont::CHAR_PIX_HEIGHT + 2));
    ascii_font.DrawStr(str, str_draw_x + 1, str_draw_y + 1, colour, 1);
    str_draw_y += AsciiFont::CHAR_PIX_HEIGHT + 1;
}

static void UpdateHumanNum(void) {
    int str_human_y = AsciiFont::CHAR_PIX_HEIGHT + 1;

    if (setting.disp_image != 0) {
        str_human_y += IMAGE_HEIGHT;
    }

    memset(str_human_num, 0, sizeof(str_human_num));
    sprintf(str_human_num, "M|%5d|%5d|%5d|%5d|", 
            male_total[0] + male_total[1],
            male_total[2] + male_total[3],
            male_total[4] + male_total[5],
            male_total[6] + male_total[7] + male_total[8] + male_total[9]);
    ascii_font_human.DrawStr(str_human_num, 1, str_human_y + 1, 0x0000ffff, 1);
    str_human_y += AsciiFont::CHAR_PIX_HEIGHT + 1;

    memset(str_human_num, 0, sizeof(str_human_num));
    sprintf(str_human_num, "F|%5d|%5d|%5d|%5d|", 
            female_total[0] + female_total[1],
            female_total[2] + female_total[3],
            female_total[4] + female_total[5],
            female_total[6] + female_total[7] + female_total[8] + female_total[9]);
    ascii_font_human.DrawStr(str_human_num, 1, str_human_y + 1, 0x0000ffff, 1);
}

static void DrawHumanNum(void) {
    int str_human_y = 0;

    if (setting.disp_image != 0) {
        str_human_y += IMAGE_HEIGHT;
    }

    ascii_font_human.Erase(0x000000f0, 0, str_human_y, DISP_PIXEL_WIDTH, (AsciiFont::CHAR_PIX_HEIGHT + 1) * 3 + 1);

    memset(str_human_num, 0, sizeof(str_human_num));
    sprintf(str_human_num, "G| 0-19|20-39|40-59|  60+|"); 
    ascii_font_human.DrawStr(str_human_num, 1, str_human_y + 1, 0x0000ffff, 1);

    UpdateHumanNum();
}

void init_recognition_layers(DisplayBase * p_display) {
    DisplayBase::rect_t rect;

    memset(user_frame_buffer_result, 0, sizeof(user_frame_buffer_result));

    image_pos_x = setting.image_pos_x;
    image_pos_y = setting.image_pos_y;

    /* The layer by which the image recognition is drawn */
    rect.vs = image_pos_y;
    rect.vw = DISP_PIXEL_HEIGHT;
    rect.hs = image_pos_x;
    rect.hw = DISP_PIXEL_WIDTH;
    p_display->Graphics_Read_Setting(
        DisplayBase::GRAPHICS_LAYER_2,
        (void *)user_frame_buffer_result,
        RESULT_BUFFER_STRIDE,
        DisplayBase::GRAPHICS_FORMAT_ARGB4444,
        DisplayBase::WR_RD_WRSWA_32_16BIT,
        &rect
    );
    p_display->Graphics_Start(DisplayBase::GRAPHICS_LAYER_2);
}

void recognition_task(DisplayBase * p_display) {
    INT32 ret = 0;
    UINT8 status;
    HVC_VERSION version;
    HVC_RESULT *pHVCResult = NULL;

    int nSTBFaceCount;
    STB_FACE *pSTBFaceResult;
    int nSTBBodyCount;
    STB_BODY *pSTBBodyResult;
    int nIndex;
    uint32_t colour;

    INT32 execFlag;
    INT32 imageNo;
    const char *pExStr[] = {"?", "Neutral", "Happiness", "Surprise", "Anger", "Sadness"};
    uint32_t i;
    char Str_disp[64];
    int  Str_len;
    INT32 TrackingID;

    int face_cnt = 0;
    bool face_in = false;

    /* Initializing Recognition layers */
    EraseImage();
    init_recognition_layers(p_display);

    /* Result Structure Allocation */
    pHVCResult = (HVC_RESULT *)malloc(sizeof(HVC_RESULT));
    if (pHVCResult == NULL) {
        DrawDebugLog("Memory Allocation Error : %08x\n", sizeof(HVC_RESULT));
        mbed_die();
    }

    /* STB Initialize */
    ret = STB_Init(STB_FUNC_BD | STB_FUNC_DT | STB_FUNC_PT | STB_FUNC_AG | STB_FUNC_GN);
    if (ret != 0) {
        DrawDebugLog("STB_Init Error : %d\n", ret);
        mbed_die();
    }

    while (1) {
        /* try to connect a serial device */
        while (!serial.connect()) {
            ThisThread::sleep_for(200);
        }
        serial.baud(921600);
        setting_req = true;

        do {
            /* Get Model and Version */
            ret = HVC_GetVersion(UART_SETTING_TIMEOUT, &version, &status);
            if ((ret != 0) || (status != 0)) {
                break;
            }

            while (1) {
                if (!serial.connected()) {
                    break;
                }

                /* Execute Setting */
                if (setting_req) {
                    setting_req = false;

                    if (setting.disp_image == 0) {
                        memset(user_frame_buffer_result, 0, sizeof(user_frame_buffer_result));
                    } else {
                        if ((image_pos_x != setting.image_pos_x) || (image_pos_y != setting.image_pos_y)) {
                            image_pos_x = setting.image_pos_x;
                            image_pos_y = setting.image_pos_y;
                            // TO DO
                        }
                    }

                    if (setting.disp_human_num == 0) {
                        memset(user_frame_buffer_result, 0, sizeof(user_frame_buffer_result));
                    } else {
                        DrawHumanNum();
                    }

                    /* Set Camera Angle */
                    ret = HVC_SetCameraAngle(UART_SETTING_TIMEOUT, SENSOR_ROLL_ANGLE_DEFAULT, &status);
                    if ((ret != 0) || (status != 0)) {
                        break;
                    }
                    /* Set Threshold Values */
                    ret = HVC_SetThreshold(UART_SETTING_TIMEOUT, &setting.threshold, &status);
                    if ((ret != 0) || (status != 0)) {
                        break;
                    }
                    ret = HVC_GetThreshold(UART_SETTING_TIMEOUT, &setting.threshold, &status);
                    if ((ret != 0) || (status != 0)) {
                        break;
                    }
                    /* Set Detection Size */
                    ret = HVC_SetSizeRange(UART_SETTING_TIMEOUT, &setting.sizeRange, &status);
                    if ((ret != 0) || (status != 0)) {
                        break;
                    }
                    ret = HVC_GetSizeRange(UART_SETTING_TIMEOUT, &setting.sizeRange, &status);
                    if ((ret != 0) || (status != 0)) {
                        break;
                    }
                    /* Set Face Angle */
                    ret = HVC_SetFaceDetectionAngle(UART_SETTING_TIMEOUT, setting.pose, setting.angle, &status);
                    if ((ret != 0) || (status != 0)) {
                        break;
                    }
                    ret = HVC_GetFaceDetectionAngle(UART_SETTING_TIMEOUT, &setting.pose, &setting.angle, &status);
                    if ((ret != 0) || (status != 0)) {
                        break;
                    }

                    /* Set STB Parameters */
                    ret = STB_SetTrParam(setting.stb_retrycount, setting.stb_possteadiness, setting.stb_sizesteadiness);
                    if (ret != 0) {
                        break;
                    }
                    ret = STB_SetPeParam(setting.stb_pe_threshold, setting.stb_pe_angleudmin, setting.stb_pe_angleudmax,
                                         setting.stb_pe_anglelrmin, setting.stb_pe_anglelrmax, setting.stb_pe_frame);
                    if (ret != 0) {
                        break;
                    }
                }

                /* Execute Detection */
                execFlag = setting.execFlag;
                if ((execFlag & HVC_ACTIV_FACE_DETECTION) == 0) {
                    execFlag &= ~(HVC_ACTIV_AGE_ESTIMATION | HVC_ACTIV_GENDER_ESTIMATION | HVC_ACTIV_EXPRESSION_ESTIMATION);
                }
                if (execFlag != 0) { // for STB
                    execFlag |= HVC_ACTIV_FACE_DIRECTION;
                }
                if (setting.disp_image == 0) {
                    imageNo = HVC_EXECUTE_IMAGE_NONE;
                } else if (setting.disp_image == 1) {
                    imageNo = HVC_EXECUTE_IMAGE_QVGA_HALF;
                } else {
                    imageNo = HVC_EXECUTE_IMAGE_NONE;
                }
                ret = HVC_ExecuteEx(UART_EXECUTE_TIMEOUT, execFlag, imageNo, pHVCResult, &status);
                if ((ret == 0) && (status == 0)) {
                    /* STB */
                    if (STB_Exec(pHVCResult->executedFunc, pHVCResult, &nSTBFaceCount, &pSTBFaceResult, &nSTBBodyCount, &pSTBBodyResult) == 0) {
                        for (i = 0; i < (uint32_t)nSTBBodyCount; i++) {
                            if ( pHVCResult->bdResult.num <= i ) break;

                            nIndex = pSTBBodyResult[i].nDetectID;
                            if (nIndex >= 0) {
                                pHVCResult->bdResult.bdResult[nIndex].posX = (short)pSTBBodyResult[i].center.x;
                                pHVCResult->bdResult.bdResult[nIndex].posY = (short)pSTBBodyResult[i].center.y;
                                pHVCResult->bdResult.bdResult[nIndex].size = pSTBBodyResult[i].nSize;
                            }
                        }
                        for (i = 0; i < (uint32_t)nSTBFaceCount; i++) {
                            if (pHVCResult->fdResult.num <= i) break;

                            nIndex = pSTBFaceResult[i].nDetectID;
                            if (nIndex >= 0) {
                                pHVCResult->fdResult.fcResult[nIndex].dtResult.posX = (short)pSTBFaceResult[i].center.x;
                                pHVCResult->fdResult.fcResult[nIndex].dtResult.posY = (short)pSTBFaceResult[i].center.y;
                                pHVCResult->fdResult.fcResult[nIndex].dtResult.size = pSTBFaceResult[i].nSize;

                                if (pHVCResult->executedFunc & HVC_ACTIV_AGE_ESTIMATION) {
                                    pHVCResult->fdResult.fcResult[nIndex].ageResult.confidence += 10000; // During
                                    if (pSTBFaceResult[i].age.status >= STB_STATUS_COMPLETE) {
                                        pHVCResult->fdResult.fcResult[nIndex].ageResult.age = pSTBFaceResult[i].age.value;
                                        pHVCResult->fdResult.fcResult[nIndex].ageResult.confidence += 10000; // Complete
                                        if (pSTBFaceResult[i].age.status == STB_STATUS_COMPLETE) {
                                            pHVCResult->fdResult.fcResult[nIndex].ageResult.confidence += 10000; // Just Complete
                                        }
                                    }
                                }
                                if (pHVCResult->executedFunc & HVC_ACTIV_GENDER_ESTIMATION) {
                                    pHVCResult->fdResult.fcResult[nIndex].genderResult.confidence += 10000; // During
                                    if (pSTBFaceResult[i].gender.status >= STB_STATUS_COMPLETE) {
                                        pHVCResult->fdResult.fcResult[nIndex].genderResult.gender = pSTBFaceResult[i].gender.value;
                                        pHVCResult->fdResult.fcResult[nIndex].genderResult.confidence += 10000; // Complete
                                        if (pSTBFaceResult[i].gender.status >= STB_STATUS_COMPLETE) {
                                            pHVCResult->fdResult.fcResult[nIndex].genderResult.confidence += 10000; // Just Complete
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (imageNo == HVC_EXECUTE_IMAGE_QVGA_HALF) {
                        DrawImage(0, 0, pHVCResult->image.width, pHVCResult->image.height, pHVCResult->image.image);
                    } else {
                        EraseImage();
                        str_draw_x = 0;
                        str_draw_y = 0;
                    }
                    /* Face Detection result */
                    if (pHVCResult->executedFunc &
                            (HVC_ACTIV_FACE_DETECTION | HVC_ACTIV_FACE_DIRECTION |
                             HVC_ACTIV_AGE_ESTIMATION | HVC_ACTIV_GENDER_ESTIMATION |
                             HVC_ACTIV_GAZE_ESTIMATION | HVC_ACTIV_BLINK_ESTIMATION |
                             HVC_ACTIV_EXPRESSION_ESTIMATION | HVC_ACTIV_FACE_RECOGNITION)){
                        for (i = 0; i < pHVCResult->fdResult.num; i++) {
                            if (pHVCResult->executedFunc & HVC_ACTIV_FACE_DETECTION) {
                                /* Detection */
                                DrawSquare(pHVCResult->fdResult.fcResult[i].dtResult.posX,
                                           pHVCResult->fdResult.fcResult[i].dtResult.posY,
                                           pHVCResult->fdResult.fcResult[i].dtResult.size,
                                           0x0000f0f0);
                                if (pHVCResult->fdResult.fcResult[i].dtResult.size >= setting.face_in_size) {
                                    face_cnt = setting.face_out_cnt;
                                    if (face_in == false) {
                                        face_in = true;
                                        SetEfectReq(EVENT_FACE_IN);
                                    }
                                }
                            }

                            if (setting.disp_tracking_id != 0) {
                                TrackingID = 0;
                                for (int j = 0; j < nSTBFaceCount; j++) {
                                    if (pSTBFaceResult[j].nDetectID == (STB_INT32)i) {
                                        TrackingID = pSTBFaceResult[j].nTrackingID;
                                        break;
                                    }
                                }
                                memset(Str_disp, 0, sizeof(Str_disp));
                                if (pHVCResult->executedFunc & HVC_ACTIV_FACE_RECOGNITION) {
                                    /* Recognition */
                                    if (pHVCResult->fdResult.fcResult[i].recognitionResult.uid < 0) {
                                        sprintf(Str_disp, "ID:%d", TrackingID);
                                    } else {
                                        sprintf(Str_disp, "USER%03d", pHVCResult->fdResult.fcResult[i].recognitionResult.uid + 1);
                                    }
                                } else {
                                    sprintf(Str_disp, "ID:%d", TrackingID);
                                }
                                DrawString(Str_disp, 0x0000f0ff);
                            }

                            if (setting.disp_face_size != 0) {
                                memset(Str_disp, 0, sizeof(Str_disp));
                                sprintf(Str_disp, "size:%d", pHVCResult->fdResult.fcResult[i].dtResult.size);
                                DrawString(Str_disp, 0x0000f0ff);
                            }

                            Str_len = 0;
                            memset(Str_disp, 0, sizeof(Str_disp));
                            colour = 0x0000ffff;
                            if (pHVCResult->executedFunc & HVC_ACTIV_GENDER_ESTIMATION) {
                                /* Gender */
                                if (-128 != pHVCResult->fdResult.fcResult[i].genderResult.gender) {
                                    if (1 == pHVCResult->fdResult.fcResult[i].genderResult.gender) {
                                        sprintf(&Str_disp[Str_len], "Male");
                                        colour = 0x0000fff4;
                                    } else {
                                        sprintf(&Str_disp[Str_len], "Female");
                                        colour = 0x00006dff;
                                    }
                                }
                            }
                            Str_len += strlen(&Str_disp[Str_len]);
                            if (pHVCResult->executedFunc & HVC_ACTIV_AGE_ESTIMATION) {
                                /* Age */
                                if (-128 != pHVCResult->fdResult.fcResult[i].ageResult.age) {
                                    sprintf(&Str_disp[Str_len], " %d", pHVCResult->fdResult.fcResult[i].ageResult.age);
                                    Str_len += strlen(&Str_disp[Str_len]);
                                    if (pHVCResult->fdResult.fcResult[i].ageResult.confidence < 20000) {
                                        strcat(Str_disp, " (?)");
                                    }
                                    if (pHVCResult->fdResult.fcResult[i].ageResult.confidence >= 30000) {
                                        // Just Complete
                                        if (pHVCResult->fdResult.fcResult[i].genderResult.gender == 1) {
                                            male_total[pHVCResult->fdResult.fcResult[i].ageResult.age / 10]++;
                                        } else {
                                            female_total[pHVCResult->fdResult.fcResult[i].ageResult.age / 10]++;
                                        }
                                        human_num_change = true;
                                        if (setting.disp_human_num == 1) {
                                            UpdateHumanNum();
                                        }
                                    }
                                }
                            }
                            DrawString(Str_disp, colour);

                            if (pHVCResult->executedFunc & HVC_ACTIV_EXPRESSION_ESTIMATION) {
                                /* Expression */
                                if (-128 != pHVCResult->fdResult.fcResult[i].expressionResult.score[0]) {
                                    if (pHVCResult->fdResult.fcResult[i].expressionResult.topExpression > EX_SADNESS) {
                                        pHVCResult->fdResult.fcResult[i].expressionResult.topExpression = 0;
                                    }
                                    switch (pHVCResult->fdResult.fcResult[i].expressionResult.topExpression) {
                                        case 1:  colour = 0x0000ffff; break;  /* white */
                                        case 2:  colour = 0x0000f0ff; break;  /* yellow */
                                        case 3:  colour = 0x000060ff; break;  /* orange */
                                        case 4:  colour = 0x00000fff; break;  /* purple */
                                        case 5:  colour = 0x0000fff4; break;  /* blue */
                                        default: colour = 0x0000ffff; break;  /* white */
                                    }
                                    DrawString(pExStr[pHVCResult->fdResult.fcResult[i].expressionResult.topExpression], colour);
                                }
                            }
                        }
                    }
                    if (face_cnt > 0) {
                        face_cnt--;
                        if (face_cnt == 0) {
                            face_in = false;
                            SetEfectReq(EVENT_FACE_OUT);
                        }
                    }
                    ThisThread::sleep_for(50);
                } else {
                    ThisThread::sleep_for(200);
                }
            }
        } while(0);

        memset(user_frame_buffer_result, 0, sizeof(user_frame_buffer_result));

        if (face_in != false) {
            face_in = false;
            face_cnt = 0;
            SetEfectReq(EVENT_FACE_OUT);
        }
        ThisThread::sleep_for(200);
    }
}
