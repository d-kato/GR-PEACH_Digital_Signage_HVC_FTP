#include "mbed.h"
#include "DisplayBace.h"
#include "telop_proc.h"
#include "EasyAttach_CameraAndLCD.h"
#include "FlashAccess.h"
#include "dcache-control.h"
#include "DisplayDebugLog.h"

/* TELOP BUFFER Parameter GRAPHICS_LAYER_3 */
#define TELOP_BUFFER_BYTE_PER_PIXEL  (2u)

#define FLASH_START_ADDR             (0x200000)
#define FLASH_END_ADDR               (0x800000)
#define BUFF_SIZE                    (256)

#define TELOP_START_ADDR             (FLASH_START_ADDR)
#define TELOP_TBL_TOP                (TELOP_START_ADDR + 0x18000000)
#define TELOP_DATA_ADDR              (TELOP_TBL_TOP + 32)

static uint32_t telop_data_idx = 0;
static uint32_t telop_data_size;
static FlashAccess flash;

static uint8_t get_next_data(void) {
    uint8_t ret = 0;
    uint8_t * p_telop_data = (uint8_t *)TELOP_DATA_ADDR;

    ret = p_telop_data[telop_data_idx];
    telop_data_idx++;
    if (telop_data_idx >= telop_data_size) {
        telop_data_idx = 0;
    }

    return ret;
}

void telop_data_save(const char * file_name) {
    FILE * fp = NULL;
    size_t read_size;
    uint32_t flash_addr = TELOP_START_ADDR;
    bool loop_end = false;
    uint8_t * work_buff;
    uint32_t saved_file_idx = 0;
    uint8_t * p_telop_tbl_top = (uint8_t *)TELOP_TBL_TOP;
    bool skip = false;

    DrawDebugLog("Telop data check\r\n");
    work_buff = new uint8_t[BUFF_SIZE];
    if (work_buff == NULL) {
        DrawDebugLog("alloc error\r\n");
        return;
    }

    // data check
    fp = fopen(file_name, "r");
    if (fp != NULL) {
        while (true) {
            read_size = fread(work_buff, sizeof(char), BUFF_SIZE, fp);
            if (memcmp(work_buff, &p_telop_tbl_top[saved_file_idx], read_size) != 0) {
                break;
            }
            saved_file_idx += read_size;
            if (read_size < BUFF_SIZE) {
                skip = true;
                break;
            }
        }
        fclose(fp);
    }

    if (skip == false) {
        DrawDebugLog("Telop data saving...\r\n");
        fp = fopen(file_name, "r");
        if (fp != NULL) {
            while (loop_end == false) {
                flash.SectorErase(flash_addr);
                for (int i = 0; i < 16; i++) {
                    read_size = fread(work_buff, sizeof(char), BUFF_SIZE, fp);
                    flash.PageProgram(flash_addr, work_buff, BUFF_SIZE);
                    flash_addr += BUFF_SIZE;
                    if ((read_size < BUFF_SIZE) || (flash_addr >= FLASH_END_ADDR)) {
                        loop_end = true;
                        break;
                    }
                }
            }
            fclose(fp);
            DrawDebugLog("done\r\n");
        } else {
            DrawDebugLog("file open error\r\n");
        }
    } else {
        DrawDebugLog("Telop data is same\r\n");
    }
    delete[] work_buff;
}

void telop_task(telop_task_cfg_t * p_cfg) {
    DisplayBase::rect_t rect;
    int frame_buffer_idx = 0;
    uint8_t * p_dest_1;
    uint8_t * p_dest_2;
    int wk_idx_1;
    int wk_idx_2;
    uint32_t data_width;
    uint32_t data_height;
    uint8_t * p_telop_tbl_top = (uint8_t *)TELOP_TBL_TOP;
    uint8_t * wk_data_top;
    uint8_t * user_frame_buffer_telop;
    uint32_t frame_buffer_size;
    uint32_t surplus_size = (LCD_PIXEL_WIDTH * 2) % p_cfg->speed;
    int display_return_idx = (LCD_PIXEL_WIDTH + surplus_size) * TELOP_BUFFER_BYTE_PER_PIXEL;
    uint32_t telop_buffer_stride = ((((LCD_PIXEL_WIDTH * 2) + surplus_size) * TELOP_BUFFER_BYTE_PER_PIXEL) + 31u) & ~31u;

    /* Since it rotates 90 degrees, the height and width change */
    data_height = ((uint32_t)p_telop_tbl_top[13] << 8) + (uint32_t)p_telop_tbl_top[12];
    data_width = ((uint32_t)p_telop_tbl_top[15] << 8) + (uint32_t)p_telop_tbl_top[14];
    telop_data_size = data_width * data_height * TELOP_BUFFER_BYTE_PER_PIXEL;
    frame_buffer_size = (telop_buffer_stride * data_height);

    /* Alloc frame buffer */
    wk_data_top = new uint8_t[frame_buffer_size + 31];
    if (wk_data_top == NULL) {
        return;
    }
    user_frame_buffer_telop = (uint8_t *)((uint32_t)wk_data_top + ((uint32_t)wk_data_top % 32));
    memset(user_frame_buffer_telop, 0x00, frame_buffer_size);
    dcache_clean(user_frame_buffer_telop, frame_buffer_size);

    /* The layer by which the image telop is drawn */
    if (p_cfg->pos_y > (LCD_PIXEL_HEIGHT - data_height)) {
        rect.vs = LCD_PIXEL_HEIGHT - data_height;
    } else {
        rect.vs = p_cfg->pos_y;
    }
    rect.vw = data_height;
    rect.hs = 0;
    rect.hw = LCD_PIXEL_WIDTH;
    p_cfg->p_display->Graphics_Read_Setting(
        DisplayBase::GRAPHICS_LAYER_3,
        (void *)user_frame_buffer_telop,
        telop_buffer_stride,
        DisplayBase::GRAPHICS_FORMAT_ARGB4444,
        DisplayBase::WR_RD_WRSWA_32_16BIT,
        &rect
    );
    p_cfg->p_display->Graphics_Start(DisplayBase::GRAPHICS_LAYER_3);
    Thread::signal_wait(1);

    while (true) {
        /* Scroll process */
        frame_buffer_idx += (TELOP_BUFFER_BYTE_PER_PIXEL * p_cfg->speed);
        if (frame_buffer_idx > display_return_idx) {
            frame_buffer_idx = 0;
        }
        p_cfg->p_display->Graphics_Read_Change(DisplayBase::GRAPHICS_LAYER_3,
         (void *)&user_frame_buffer_telop[frame_buffer_idx]);
         Thread::signal_wait(1);

        /* Drawing process */
        wk_idx_1 = frame_buffer_idx - (TELOP_BUFFER_BYTE_PER_PIXEL * p_cfg->speed);
        wk_idx_2 = frame_buffer_idx;
        for (int speed_cnt = 0; speed_cnt < p_cfg->speed; speed_cnt++) {
            if (wk_idx_1 >= 0) {
                p_dest_1 = &user_frame_buffer_telop[wk_idx_1];
                p_dest_1 += (data_height * telop_buffer_stride);
            } else {
                p_dest_1 = NULL;
            }
            if (wk_idx_2 < display_return_idx) {
                p_dest_2 = &user_frame_buffer_telop[(LCD_PIXEL_WIDTH * TELOP_BUFFER_BYTE_PER_PIXEL) + wk_idx_2];
                p_dest_2 += (data_height * telop_buffer_stride);
            } else {
                p_dest_2 = NULL;
            }
            for (int h = 0; h < (int)data_height; h++) {
                uint8_t wk_data1 = get_next_data();
                uint8_t wk_data2 = get_next_data();
                if (p_dest_1 != NULL) {
                    p_dest_1 -= telop_buffer_stride;
                    *(p_dest_1 + 0) = wk_data1;
                    *(p_dest_1 + 1) = wk_data2;
                }
                if (p_dest_2 != NULL) {
                    p_dest_2 -= telop_buffer_stride;
                    *(p_dest_2 + 0) = wk_data1;
                    *(p_dest_2 + 1) = wk_data2;
                }
            }
            wk_idx_1 += TELOP_BUFFER_BYTE_PER_PIXEL;
            wk_idx_2 += TELOP_BUFFER_BYTE_PER_PIXEL;
        }
        dcache_clean(user_frame_buffer_telop, frame_buffer_size);
    }
}
