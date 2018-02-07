
#include "mbed.h"
#include "EasyAttach_CameraAndLCD.h"
#include "DisplayEfect.h"

#define TOUCH_NUM                           (2u)

#ifdef TouckKey_LCD_shield
static Semaphore   sem_touch_int(0);

/****** Touch ******/
static float get_distance(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    float distance_x;
    float distance_y;

    if (x0 > x1) {
        distance_x = x0 - x1;
    } else {
        distance_x = x1 - x0;
    }
    if (y0 > y1) {
        distance_y = y0 - y1;
    } else {
        distance_y = y1 - y0;
    }
    return hypotf(distance_x, distance_y);
}

static uint32_t get_center(uint32_t xy0, uint32_t xy1) {
    uint32_t center_pos;

    if (xy0 > xy1) {
        center_pos = (xy0 - xy1) / 2 + xy1;
    } else {
        center_pos = (xy1 - xy0) / 2 + xy0;
    }

    return center_pos;
}

static void touch_int_callback(void) {
    sem_touch_int.release();
}

void touch_task(void) {
    bool skip = false;
    bool zoom_on = false;
    uint32_t center_x;
    uint32_t center_y;
    uint32_t center_x_last = 0;
    uint32_t center_y_last = 0;
    int click_cnt = 0;
    int move_x = 0;
    int move_y = 0;
    float distance;
    float distance_last;
    int down_time = 0;;
    int last_down_time = 0;
    int event_time = 0;;
    int event_time_last = 0;
    TouchKey::touch_pos_t touch_pos[TOUCH_NUM];
    TouchKey::touch_pos_t touch_pos_last[TOUCH_NUM];
    int touch_num = 0;
    int touch_num_last = 0;
    Timer touch_timer;
    TouckKey_LCD_shield touch(P4_0, P2_13, I2C_SDA, I2C_SCL);

    /* Callback setting */
    touch.SetCallback(&touch_int_callback);

    /* Reset touch IC */
    touch.Reset();

    touch_timer.reset();
    touch_timer.start();

    while (1) {
        /* Wait touch event */
        sem_touch_int.wait();

        /* Get touch coordinates */
        touch_num = touch.GetCoordinates(TOUCH_NUM, touch_pos);
        event_time = touch_timer.read_ms();
        if (touch_num > touch_num_last) {
            if (touch_num_last == 0) {
                down_time = event_time;
            }
            if (touch_num == 2) {
                zoom_on = true;
            }
            move_x = 0;
            move_y = 0;
            distance_last = 0;
            if ((down_time - last_down_time) > 500) {
                click_cnt = 0;
            }
            last_down_time = down_time;
        } else if ((touch_num == 0) && (touch_num_last != 0)) {
            if (((event_time - down_time) < 200)
             && (abs(move_x) < 10) && (abs(move_y) < 10)) {
                click_cnt++;
                if (click_cnt == 2) {
                    SetZoom(0, 0, 0, 0, 1.0f);
                }
                move_x = 0;
                move_y = 0;
            } else {
                click_cnt = 0;
            }
            if (zoom_on == false) {
                SetMoveEnd(move_x, move_y);
            }
            zoom_on = false;
            distance_last = 0;
        } else if ((touch_num == 1) && (touch_num_last == 2)) {
            distance_last = 0;
        } else if ((touch_num != 0) && ((event_time - event_time_last) >= 50)) {
            event_time_last = event_time;
            if (touch_num == 1) {
                if (zoom_on == false) {
                    move_x = (touch_pos[0].x - touch_pos_last[0].x);
                    move_y = (touch_pos[0].y - touch_pos_last[0].y);
                    SetMove(move_x, move_y);
                }
            } else {
                center_x = get_center(touch_pos[0].x, touch_pos[1].x);
                center_y = get_center(touch_pos[0].y, touch_pos[1].y);
                distance = get_distance(touch_pos[0].x, touch_pos[0].y, touch_pos[1].x, touch_pos[1].y);
                if (distance < 1) {
                    distance = 1;
                }
                if (distance_last != 0) {
                    SetZoom(center_x_last, center_y_last, center_x, center_y, GetEfectMagnification() * (distance / distance_last));
                }
                center_x_last = center_x;
                center_y_last = center_y;
                distance_last = distance;
            }
        } else {
            skip = true;
        }
        if (skip == false) {
            touch_pos_last[0] = touch_pos[0];
            touch_pos_last[1] = touch_pos[1];
            touch_num_last = touch_num;
        }
        skip = false;
    }
}
#endif
