#include "mbed.h"
#include "EasyAttach_CameraAndLCD.h"
#include "RGA.h"
#include "BinaryImage_RZ_A1H.h"
#include "SdUsbConnect.h"
#include "FileAnalysis.h"
#include "setting_proc.h"
#include "recognition_proc.h"
#include "telop_proc.h"
#include "mouse_proc.h"
#ifdef TouckKey_LCD_shield
#include "touch_proc.h"
#endif
#include "DisplayEfect.h"
#include "DisplayDebugLog.h"
#if (MBED_CONF_APP_FTP_CONNECT == 1) || (MBED_CONF_APP_NTP_CONNECT == 1)
#include "network_proc.h"
#endif

/**** User Selection *********/
#define SLIDE_DISPLAY_TIME                  (10000) /* The automatic page turning interval is changed. The unit is ms. */
#define DISSOLVE_STEP_NUM                   (16)    /* The effect amount at automatic page turning is set. The minimum value is 1. */
#define SCROLL_STEP_NUM                     (5)     /* The effect amount at manual page turning is set. The minimum value is 1. */
#define SCROLL_DIRECTION                    (-1)    /* The scrolling direction at manual page turning is set. 1Fleft to right. -1Fright to left. */
/*****************************/

#define GRAPHICS_FORMAT                     (DisplayBase::GRAPHICS_FORMAT_RGB565)
#define WR_RD_WRSWA                         (DisplayBase::WR_RD_WRSWA_32_16BIT)

#if (SCROLL_DIRECTION == -1)
#define SCROLL_DIRECTION_NEXT               (-1)
#define SCROLL_DIRECTION_PREV               (1)
#else
#define SCROLL_DIRECTION_NEXT               (1)
#define SCROLL_DIRECTION_PREV               (-1)
#endif

/* FRAME BUFFER Parameter */
#define FRAME_BUFFER_BYTE_PER_PIXEL         (2)
#define FRAME_BUFFER_STRIDE                 (((LCD_PIXEL_WIDTH * FRAME_BUFFER_BYTE_PER_PIXEL) + 31u) & ~31u)

#define MAX_JPEG_SIZE                       (1024 * 400)

#define MOUNT_NAME                          "storage"


typedef struct {
    int  pos_x;
    int  pos_y;
    bool disp_pos;
    bool disp_time;
} mouse_info_t;

typedef struct {
    int       event_req;
    uint8_t * p_pic_next;
    uint8_t * p_pic_now;
    float magnification;
    int       x_move;
    int       swipe_end_move;
    int       drow_pos_x;
    int       drow_pos_y;
    int       scale_end_move_x;
    int       scale_end_move_y;
    int       min_x;
    int       min_y;
    bool      scroll;
} efect_info_t;

static InterruptIn button(USER_BUTTON0);
static DisplayBase Display;
static Canvas2D_ContextClass canvas2d;
static Timer system_timer;
static SdUsbConnect storage(MOUNT_NAME);

#if defined(__ICCARM__)
#pragma data_alignment=32
static uint8_t user_frame_buffer1[FRAME_BUFFER_STRIDE * LCD_PIXEL_HEIGHT];
#pragma data_alignment=32
static uint8_t user_frame_buffer2[FRAME_BUFFER_STRIDE * LCD_PIXEL_HEIGHT];
#else
static uint8_t user_frame_buffer1[FRAME_BUFFER_STRIDE * LCD_PIXEL_HEIGHT]__attribute((aligned(32)));  /* 32 bytes aligned */
static uint8_t user_frame_buffer2[FRAME_BUFFER_STRIDE * LCD_PIXEL_HEIGHT]__attribute((aligned(32))); /* 32 bytes aligned */
#endif

static frame_buffer_t frmbuf_info;
static volatile int32_t vsync_count = 0;
static char face_file_name[64] = {0};
static Mutex lock_event_req;
static mouse_info_t mouse_info = {0, 0, false, false};
static int disp_wait_time = SLIDE_DISPLAY_TIME;
static uint32_t file_id_now = 0xffffffff;
static uint32_t file_id_next = 0xffffffff;
static int dissolve_seq = 0;
static efect_info_t efect_info;
static uint32_t total_file_num = 0;
static telop_task_cfg_t telop_task_cfg = {&Display, 0xFFFF, 2};
static Thread recognitionTask(osPriorityNormal, 1024 * 8);
static Thread telopTask(osPriorityNormal, 1024 * 2);
static Thread mouseTask(osPriorityNormal, 1024 * 2);
#ifdef TouckKey_LCD_shield
static Thread touchTask(osPriorityNormal, 1024 * 8);
#endif
static bool telopTask_started = false;

static const graphics_image_t* number_tbl[10] = {
    number0_Img,
    number1_Img,
    number2_Img,
    number3_Img,
    number4_Img,
    number5_Img,
    number6_Img,
    number7_Img,
    number8_Img,
    number9_Img
};

/****** LCD ******/
static void IntCallbackFunc_LoVsync(DisplayBase::int_type_t int_type) {
    /* Interrupt callback function for Vsync interruption */
    if (vsync_count > 0) {
        vsync_count--;
    }
    if (telopTask_started) {
        telopTask.signal_set(1);
    }
}

static void Wait_Vsync(const int32_t wait_count) {
    /* Wait for the specified number of times Vsync occurs */
    vsync_count = wait_count;
    while (vsync_count > 0) {
        Thread::wait(2);
    }
}

static void Start_LCD_Display(void) {
    errnum_t err;
    Canvas2D_ContextConfigClass config;
    DisplayBase::rect_t rect;
    DisplayBase::graphics_error_t error;

    /* Interrupt callback function setting (Vsync signal output from scaler 0) */
    error = Display.Graphics_Irq_Handler_Set(DisplayBase::INT_TYPE_S0_LO_VSYNC, 0, IntCallbackFunc_LoVsync);
    if (error != DisplayBase::GRAPHICS_OK) {
        printf("Line %d, error %d\n", __LINE__, error);
        mbed_die();
    }

    memset(user_frame_buffer1, 0, sizeof(user_frame_buffer1));
    memset(user_frame_buffer2, 0, sizeof(user_frame_buffer2));
    frmbuf_info.buffer_address[0] = user_frame_buffer1;
    frmbuf_info.buffer_address[1] = user_frame_buffer2;
    frmbuf_info.buffer_count      = 2;
    frmbuf_info.show_buffer_index = 0;
    frmbuf_info.draw_buffer_index = 0;
    frmbuf_info.width             = LCD_PIXEL_WIDTH;
    frmbuf_info.byte_per_pixel    = FRAME_BUFFER_BYTE_PER_PIXEL;
    frmbuf_info.stride            = LCD_PIXEL_WIDTH * FRAME_BUFFER_BYTE_PER_PIXEL;
    frmbuf_info.height            = LCD_PIXEL_HEIGHT;
    frmbuf_info.pixel_format      = PIXEL_FORMAT_RGB565;

    config.frame_buffer = &frmbuf_info;
    canvas2d = R_RGA_New_Canvas2D_ContextClass(config);
    err = R_OSPL_GetErrNum();
    if (err != 0) {
        printf("Line %d, error %d\n", __LINE__, err);
        mbed_die();
    }

    rect.vs = 0;
    rect.vw = LCD_PIXEL_HEIGHT;
    rect.hs = 0;
    rect.hw = LCD_PIXEL_WIDTH;
    Display.Graphics_Read_Setting(
        DisplayBase::GRAPHICS_LAYER_0,
        (void *)frmbuf_info.buffer_address[0],
        FRAME_BUFFER_STRIDE,
        GRAPHICS_FORMAT,
        WR_RD_WRSWA,
        &rect
    );
    Display.Graphics_Start(DisplayBase::GRAPHICS_LAYER_0);

}

static void Update_LCD_Display(void) {
    Display.Graphics_Read_Change(DisplayBase::GRAPHICS_LAYER_0,
     (void *)frmbuf_info.buffer_address[frmbuf_info.draw_buffer_index]);
    Wait_Vsync(1);
    Thread::wait(5);
}

static void Swap_FrameBuffer(void) {
    if (frmbuf_info.draw_buffer_index == 1) {
        frmbuf_info.draw_buffer_index = 0;
    } else {
        frmbuf_info.draw_buffer_index = 1;
    }
}


/****** Draw Image ******/
static void draw_mouse_pos(void) {
    if (mouse_info.disp_pos != false) {
        /* Draw a image */
        canvas2d.drawImage(mousu_pos_Img, mouse_info.pos_x, mouse_info.pos_y);
        R_OSPL_CLEAR_ERROR();
    }
}

static void draw_disp_time(void) {
    if (mouse_info.disp_time != false) {
        int wk_ofs = (LCD_PIXEL_WIDTH - 215) / 2 + 103;
        int wk_sec;

        /* Draw a image */
        canvas2d.drawImage(disp_xsec_Img, (LCD_PIXEL_WIDTH - 215) / 2, LCD_PIXEL_HEIGHT / 2);
        R_OSPL_CLEAR_ERROR();

        if (disp_wait_time == 0) {
            /* Draw a image */
            canvas2d.drawImage(number_inf_Img, wk_ofs, LCD_PIXEL_HEIGHT / 2);
            R_OSPL_CLEAR_ERROR();
        } else {
            wk_sec = (disp_wait_time / 10000) % 10;
            if (wk_sec != 0) {
                /* Draw a image */
                canvas2d.drawImage(number_tbl[wk_sec], wk_ofs, LCD_PIXEL_HEIGHT / 2);
                R_OSPL_CLEAR_ERROR();
                wk_ofs += 12;
            } else {
                wk_ofs += 6;
            }
            wk_sec = (disp_wait_time / 1000) % 10;
            /* Draw a image */
            canvas2d.drawImage(number_tbl[wk_sec], wk_ofs, LCD_PIXEL_HEIGHT / 2);
            R_OSPL_CLEAR_ERROR();
        }
    }
}

static void draw_image_scroll(const graphics_image_t* image_last,
                              const graphics_image_t* image_new, float scroll, int ditection) {
    Swap_FrameBuffer();
    /* Clear */
    canvas2d.clearRect(0, 0, frmbuf_info.width, frmbuf_info.height);
    /* Draw a image */
    canvas2d.globalAlpha = 1.0f;
    canvas2d.drawImage((const graphics_image_t*)image_last,
                       (int_t)(frmbuf_info.width * scroll) * ditection, 0,
                       frmbuf_info.width, frmbuf_info.height);
    R_OSPL_CLEAR_ERROR();
    canvas2d.globalAlpha = 1.0f;
    canvas2d.drawImage((const graphics_image_t*)image_new,
                       ((int_t)(frmbuf_info.width * scroll) - frmbuf_info.width) * ditection, 0,
                       frmbuf_info.width, frmbuf_info.height);
    R_OSPL_CLEAR_ERROR();
    /* mouse pos */
    draw_mouse_pos();
    draw_disp_time();
    /* Complete drawing */
    R_GRAPHICS_Finish(canvas2d.c_LanguageContext);
    Update_LCD_Display();
}

static void draw_image_dissolve(const graphics_image_t* image_last,
                                const graphics_image_t* image_new, float alpha) {
    Swap_FrameBuffer();
    /* Clear */
    canvas2d.clearRect(0, 0, frmbuf_info.width, frmbuf_info.height);
    /* Draw a image */
    canvas2d.globalAlpha = 1.0f - alpha;
    canvas2d.drawImage((const graphics_image_t*)image_last, 0, 0, frmbuf_info.width, frmbuf_info.height);
    R_OSPL_CLEAR_ERROR();
    canvas2d.globalAlpha = alpha;
    canvas2d.drawImage((const graphics_image_t*)image_new, 0, 0, frmbuf_info.width, frmbuf_info.height);
    R_OSPL_CLEAR_ERROR();
    /* mouse pos */
    draw_mouse_pos();
    draw_disp_time();
    /* Complete drawing */
    R_GRAPHICS_Finish(canvas2d.c_LanguageContext);
    Update_LCD_Display();
}

static void draw_image(const graphics_image_t* image_new, 
 uint32_t pos_x, uint32_t pos_y, graphics_matrix_float_t zoom) {
    int_t dest_width;
    int_t dest_height;

    Swap_FrameBuffer();
    /* Clear */
    canvas2d.clearRect(0, 0, frmbuf_info.width, frmbuf_info.height);
    /* Draw a image */
    canvas2d.globalAlpha = 1.0f;
    dest_width  = frmbuf_info.width * zoom;
    dest_height = frmbuf_info.height * zoom;
    canvas2d.drawImage((const graphics_image_t*)image_new,
                        pos_x, pos_y, dest_width, dest_height);
    R_OSPL_CLEAR_ERROR();
    /* mouse pos */
    draw_mouse_pos();
    draw_disp_time();
    /* Complete drawing */
    R_GRAPHICS_Finish(canvas2d.c_LanguageContext);
    Update_LCD_Display();
}


/****** File Access ******/
static void swap_file_buff(void) {
    uint8_t * p_wk_pic = efect_info.p_pic_now;
    uint32_t wk_file_id = file_id_now;

    efect_info.p_pic_now = efect_info.p_pic_next;
    efect_info.p_pic_next = p_wk_pic;
    file_id_now = file_id_next;
    file_id_next = wk_file_id;
}

static bool read_next_file(void) {
    uint32_t wk_file_id;
    bool ret = false;

    if (file_id_now < (total_file_num - 1)) {
        wk_file_id = file_id_now + 1;
    } else {
        wk_file_id = 0;
    }
    if (wk_file_id != file_id_next) {
        if (GetFileData(efect_info.p_pic_next, MAX_JPEG_SIZE, get_file_name(wk_file_id)) > 0) {
            file_id_next = wk_file_id;
            ret = true;
        }
    } else {
        ret = true;
    }

    return ret;
}

static bool read_prev_file(void) {
    uint32_t wk_file_id;
    bool ret = false;

    if (file_id_now >= total_file_num) {
        wk_file_id = 0;
    } else if (file_id_now != 0) {
        wk_file_id = file_id_now - 1;
    } else {
        wk_file_id = total_file_num - 1;
    }
    if (wk_file_id != file_id_next) {
        if (GetFileData(efect_info.p_pic_next, MAX_JPEG_SIZE, get_file_name(wk_file_id)) > 0) {
            file_id_next = wk_file_id;
            ret = true;
        }
    } else {
        ret = true;
    }

    return ret;
}


/****** Efect setting ******/
static void zoom_scroll(int x, int y) {
    efect_info.drow_pos_x += x;
    if (efect_info.drow_pos_x < efect_info.min_x) {
        efect_info.drow_pos_x = efect_info.min_x;
    }
    if (efect_info.drow_pos_x > 0) {
        efect_info.drow_pos_x = 0;
    }
    efect_info.drow_pos_y += y;
    if (efect_info.drow_pos_y < efect_info.min_y) {
        efect_info.drow_pos_y = efect_info.min_y;
    }
    if (efect_info.drow_pos_y > 0) {
        efect_info.drow_pos_y = 0;
    }
}

void SetMove(int x, int y) {
    if (efect_info.magnification != 1.0f) {
        zoom_scroll(x, y);
        SetEfectReq(EVENT_DISP);
    } else {
        efect_info.scroll = true;
        efect_info.x_move += x;
        SetEfectReq(EVENT_MOVE);
    }
}

void SetMoveEnd(int x, int y) {
    if (efect_info.magnification != 1.0f) {
        efect_info.scale_end_move_x = x;
        efect_info.scale_end_move_y = y;
    } else {
        if (efect_info.scroll != false) {
            efect_info.scroll = false;
            if (efect_info.x_move != 0) {
                efect_info.swipe_end_move = x;
                SetEfectReq(EVENT_MOVE_END);
            }
        }
    }
}

void SetZoom(uint32_t center_x_last, uint32_t center_y_last, uint32_t center_x, uint32_t center_y, float new_magnification) {
    if (new_magnification != efect_info.magnification) {
        if (new_magnification < 1.0f) {
            new_magnification = 1.0f;
        }
        if (new_magnification > 10.0f) {
            new_magnification = 10.0f;
        }
        efect_info.min_x =  0 - (frmbuf_info.width * (new_magnification - 1.0f));
        efect_info.min_y =  0 - (frmbuf_info.height * (new_magnification - 1.0f));

        efect_info.drow_pos_x = -((center_x_last - efect_info.drow_pos_x) / efect_info.magnification * new_magnification - center_x);
        if (efect_info.drow_pos_x < efect_info.min_x) {
            efect_info.drow_pos_x = efect_info.min_x;
        }
        if (efect_info.drow_pos_x > 0) {
            efect_info.drow_pos_x = 0;
        }

        efect_info.drow_pos_y = -((center_y_last - efect_info.drow_pos_y) / efect_info.magnification * new_magnification - center_y);
        if (efect_info.drow_pos_y < efect_info.min_y) {
            efect_info.drow_pos_y = efect_info.min_y;
        }
        if (efect_info.drow_pos_y > 0) {
            efect_info.drow_pos_y = 0;
        }
        if (new_magnification == 1.0f) {
            efect_info.drow_pos_x = 0;
            efect_info.drow_pos_y = 0;
        }
        efect_info.x_move = 0;
        efect_info.magnification = new_magnification;
        lock_event_req.lock();
        if (efect_info.event_req == EVENT_NONE) {
            efect_info.event_req = EVENT_DISP;
        }
        lock_event_req.unlock();
    }
}

float GetEfectMagnification(void) {
    return efect_info.magnification;
}

int GetDispWaitTime(void) {
    return disp_wait_time;
}

void SetMouseDispTime(void) {
    mouse_info.disp_time = true;
}

void SetMousePointer(bool disp) {
    mouse_info.disp_pos = disp;
    SetEfectDisp();
}

void SetMousePos(int x, int y) {
    mouse_info.pos_x = x;
    mouse_info.pos_y = y;
}

void SetDispWaitTime(int wait_time) {
    disp_wait_time = wait_time;
    system_timer.reset();
    system_timer.start();
    mouse_info.disp_time = true;
    SetEfectDisp();
}

void SetEfectReq(int effect_type) {
    lock_event_req.lock();
    efect_info.event_req = effect_type;
    lock_event_req.unlock();
}

void SetEfectDisp(void) {
    lock_event_req.lock();
    if ((efect_info.event_req == EVENT_NONE) && (dissolve_seq == 0)) {
        efect_info.event_req = EVENT_DISP;
    }
    lock_event_req.unlock();
}

static void button_rise(void) {
    int wk_disp_time;

    wk_disp_time = disp_wait_time;
    wk_disp_time += 1000;
    if (wk_disp_time > 15000) {
        wk_disp_time = 0;
    }
    SetDispWaitTime(wk_disp_time);
}

int main(void) {
    int wait_time = 0;
    bool touch_key_in = false;
    int direction;
    int i;
    int scroll_step;
    int wk_event_req;
    int type;
    int wk_abs_x_pos;
    uint32_t last_file_id = 0;
    uint8_t dummy_buf[1];
    uint8_t * p_jpeg_heap_0;
    uint8_t * p_jpeg_heap_1;
#if (MBED_CONF_APP_FTP_CONNECT == 1) || (MBED_CONF_APP_NTP_CONNECT == 1)
    bool network_connect_flg = (button != 0);
#endif

    /* Initialization of LCD */
    EasyAttach_Init(Display);

    /* Start of LCD */
    Start_LCD_Display();

    p_jpeg_heap_0 =  new uint8_t[MAX_JPEG_SIZE + 31];
    p_jpeg_heap_1 =  new uint8_t[MAX_JPEG_SIZE + 31];

    efect_info.p_pic_now  = (uint8_t *)(((uint32_t)p_jpeg_heap_0 + 31) & ~31ul);
    efect_info.p_pic_next = (uint8_t *)(((uint32_t)p_jpeg_heap_1 + 31) & ~31ul);
    efect_info.event_req = EVENT_NONE;
    efect_info.magnification = 1.0f;
    efect_info.swipe_end_move = 0;
    efect_info.drow_pos_x = 0;
    efect_info.drow_pos_y = 0;
    efect_info.scale_end_move_x = 0;
    efect_info.scale_end_move_y = 0;
    efect_info.min_x = 0;
    efect_info.min_y = 0;
    efect_info.scroll = false;

    memcpy(efect_info.p_pic_now, (uint8_t *)Renesas_logo_File, 71024);

    // Renesas Logo
    file_id_now = 0xfffffffe;
    draw_image((const graphics_image_t*)efect_info.p_pic_now,
               efect_info.drow_pos_x,
               efect_info.drow_pos_y,
               efect_info.magnification);

    StartDebugLog(&Display, 5, (LCD_PIXEL_HEIGHT - DEBUG_PIXEL_HEIGHT - 5));

    Thread::wait(50);
    EasyAttach_LcdBacklight(true);

    DrawDebugLog("Finding a storage...\r\n");
    storage.wait_connect();
    DrawDebugLog("done\r\n");

#if (MBED_CONF_APP_FTP_CONNECT == 1) || (MBED_CONF_APP_NTP_CONNECT == 1)
    if (network_connect_flg) {
        network_connect("/"MOUNT_NAME, &storage);
    } else 
#endif
    {
        DrawDebugLog("Skip FTP Connect\r\n");
        DrawDebugLog("The logo display time is 3 seconds\r\n");
        Thread::wait(3000);
    }

    SetMountPath("/"MOUNT_NAME);
#if (MBED_CONF_APP_NTP_CONNECT == 1)
    if (init_timetable(&total_file_num) == false)
#endif
    {
        total_file_num = FileAnalysis("index.txt");
    }
    DrawDebugLog("The number of slides is %d\r\n", total_file_num);
    if (total_file_num == 0) {
        DrawDebugLog("No file error\r\n");
        while (true) { 
            Thread::wait(1000);
        }
    }

    setting_t setting;
    setting.p_face_file = face_file_name;
    setting.p_disp_wait_time = &disp_wait_time;
    setting.p_rec_cfg = GetRecognitionSettingPointer();
    setting.p_telop_cfg = &telop_task_cfg;
    check_setting(&setting);
    if (face_file_name[0] == '\0') {
        strcpy(face_file_name, get_file_name(0));
    }

    if (GetFileData(dummy_buf, 1, "telop.bin") == 1) {
        telop_data_save("/"MOUNT_NAME"/telop.bin");
        telopTask.start(callback(telop_task, &telop_task_cfg));
        telopTask_started = true;
    }

    recognitionTask.start(callback(recognition_task, &Display));
    mouseTask.start(callback(mouse_task));
#ifdef TouckKey_LCD_shield
    touchTask.start(callback(touch_task));
#endif

    button.rise(&button_rise);

    DrawDebugLog("Start slide show\r\n");
    StopDebugLog(&Display);

    // First page
    file_id_now = 0;
    GetFileData(efect_info.p_pic_now, MAX_JPEG_SIZE, get_file_name(0));
    draw_image((const graphics_image_t*)efect_info.p_pic_now,
               efect_info.drow_pos_x,
               efect_info.drow_pos_y,
               efect_info.magnification);

    while (1) {
        lock_event_req.lock();
        wk_event_req = efect_info.event_req;
        efect_info.event_req = EVENT_NONE;
        lock_event_req.unlock();

        if (wk_event_req != EVENT_NONE) {
            dissolve_seq = 0;
        }

#if (MBED_CONF_APP_NTP_CONNECT == 1)
        if (dissolve_seq == 0) {
            if (check_timetable(&total_file_num)) {
                file_id_now = 0xfffffffe;
            }
        }
#endif

        switch (wk_event_req) {
            case EVENT_NEXT:
            case EVENT_PREV:
            case EVENT_FACE_IN:
            case EVENT_FACE_OUT:
                SetZoom(0, 0, 0, 0, 1.0f);
                scroll_step = SCROLL_STEP_NUM;
                if (wk_event_req == EVENT_NEXT) {
                    direction = SCROLL_DIRECTION_NEXT;
                    read_next_file();
                } else if (wk_event_req == EVENT_FACE_IN) {
                    direction = SCROLL_DIRECTION_NEXT;
                    last_file_id = file_id_now;
                    GetFileData(efect_info.p_pic_next, MAX_JPEG_SIZE, face_file_name);
                    file_id_next = 0xffffffff;
                    scroll_step = 5;
                } else if (wk_event_req == EVENT_FACE_OUT) {
                    direction = SCROLL_DIRECTION_PREV;
                    GetFileData(efect_info.p_pic_next, MAX_JPEG_SIZE, get_file_name(last_file_id));
                    file_id_next = last_file_id;
                    scroll_step = 5;
                } else {
                    direction = SCROLL_DIRECTION_PREV;
                    read_prev_file();
                }
                for (i = 1; i <= scroll_step; i++) {
                    draw_image_scroll((const graphics_image_t*)efect_info.p_pic_now,
                                      (const graphics_image_t*)efect_info.p_pic_next,
                                      (float)i / (float)scroll_step, direction);
                }
                swap_file_buff();
                break;
            case EVENT_MOVE:
                if ((efect_info.x_move * SCROLL_DIRECTION) >= 0) {
                    direction = SCROLL_DIRECTION_NEXT;
                    read_next_file();
                } else {
                    direction = SCROLL_DIRECTION_PREV;
                    read_prev_file();
                }
                draw_image_scroll((const graphics_image_t*)efect_info.p_pic_now,
                                  (const graphics_image_t*)efect_info.p_pic_next,
                                  (float)abs(efect_info.x_move) / (float)LCD_PIXEL_WIDTH, direction);
                break;
            case EVENT_MOVE_END:
                type = 0;
                if ((efect_info.x_move * SCROLL_DIRECTION) >= 0) {
                    direction = SCROLL_DIRECTION_NEXT;
                    read_next_file();
                } else {
                    direction = SCROLL_DIRECTION_PREV;
                    read_prev_file();
                }

                wk_abs_x_pos = abs(efect_info.x_move);
                if (abs(efect_info.swipe_end_move) > 10) {
                    if ((efect_info.swipe_end_move * SCROLL_DIRECTION) >= 0) {
                        if (direction != SCROLL_DIRECTION_NEXT) {
                            type = 1;
                        }
                    } else {
                        if (direction != SCROLL_DIRECTION_PREV) {
                            type = 1;
                        }
                    }
                } else {
                    if (wk_abs_x_pos < (int)(LCD_PIXEL_WIDTH / 2)) {
                        type = 1;
                    }
                }

                if (type == 0) {
                    while (wk_abs_x_pos < (int)LCD_PIXEL_WIDTH) {
                        wk_abs_x_pos += (LCD_PIXEL_WIDTH / SCROLL_STEP_NUM);
                        if (wk_abs_x_pos > (int)LCD_PIXEL_WIDTH) {
                            wk_abs_x_pos = LCD_PIXEL_WIDTH;
                        }
                        draw_image_scroll((const graphics_image_t*)efect_info.p_pic_now,
                                          (const graphics_image_t*)efect_info.p_pic_next,
                                          (float)wk_abs_x_pos / (float)LCD_PIXEL_WIDTH, direction);
                    }
                    swap_file_buff();
                } else {
                    while (wk_abs_x_pos > 0) {
                        wk_abs_x_pos -= (LCD_PIXEL_WIDTH / SCROLL_STEP_NUM);
                        if (wk_abs_x_pos < 0) {
                            wk_abs_x_pos = 0;
                        }
                        draw_image_scroll((const graphics_image_t*)efect_info.p_pic_now,
                                          (const graphics_image_t*)efect_info.p_pic_next,
                                          (float)wk_abs_x_pos / (float)LCD_PIXEL_WIDTH, direction);
                    }
                }
                efect_info.x_move = 0;
                break;
            case EVENT_NONE:
            case EVENT_DISP:
            default:
                if ((efect_info.scale_end_move_x != 0) || (efect_info.scale_end_move_y != 0) || (wk_event_req == EVENT_DISP)) {
                    if ((efect_info.scale_end_move_x != 0) || (efect_info.scale_end_move_y != 0)) {
                        efect_info.scale_end_move_x *= 0.8f;
                        efect_info.scale_end_move_y *= 0.8f;
                        zoom_scroll(efect_info.scale_end_move_x, efect_info.scale_end_move_y);
                        if (abs(efect_info.scale_end_move_x) < 5) {
                            efect_info.scale_end_move_x = 0;
                        }
                        if (abs(efect_info.scale_end_move_y) < 5) {
                            efect_info.scale_end_move_y = 0;
                        }
                    }
                    draw_image((const graphics_image_t*)efect_info.p_pic_now,
                               efect_info.drow_pos_x,
                               efect_info.drow_pos_y,
                               efect_info.magnification);
                    wait_time = 0;
                } else if ((disp_wait_time != 0) && (wait_time >= disp_wait_time)) {
                    efect_info.magnification = 1.0f;
                    read_next_file();
                    dissolve_seq = 1;
                } else {
                    /* do nothing */
                }
                if (dissolve_seq != 0) {
                    draw_image_dissolve((const graphics_image_t*)efect_info.p_pic_now,
                                        (const graphics_image_t*)efect_info.p_pic_next,
                                        (float)dissolve_seq / (float)DISSOLVE_STEP_NUM);
                    if (dissolve_seq >= DISSOLVE_STEP_NUM) {
                        dissolve_seq = 0;
                        swap_file_buff();
                    } else {
                        dissolve_seq++;
                    }
                    wait_time = 0;
                }
                break;
        }
        if ((mouse_info.disp_time != false) && (system_timer.read_ms() > 1500)) {
            mouse_info.disp_time = false;
            if ((efect_info.event_req == EVENT_NONE) && (dissolve_seq == 0)) {
                draw_image((const graphics_image_t*)efect_info.p_pic_now,
                           efect_info.drow_pos_x,
                           efect_info.drow_pos_y,
                           efect_info.magnification);
            }
        }
        if ((wk_event_req != EVENT_NONE) || (touch_key_in != false) || (efect_info.magnification != 1.0f)
         || (dissolve_seq != 0) || (file_id_now == 0xffffffff) || (mouse_info.disp_pos != false)
         || (mouse_info.disp_time != false) || (efect_info.scroll != false)) {
            wait_time = 0;
            Thread::wait(5);
        } else {
            Thread::wait(50);
            if ((file_id_now == 0) && (total_file_num <= 1)) {
                /* do nothing */
            } else if (wait_time < 100000) {
                wait_time += 50;
            }
        }
    }
}
