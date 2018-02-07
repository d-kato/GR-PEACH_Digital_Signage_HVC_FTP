#include "mbed.h"
#include "EasyAttach_CameraAndLCD.h"
#include "USBHostMouse.h"
#include "DisplayEfect.h"

/****** Mouse ******/
void onMouseEvent(uint8_t buttons, int8_t x, int8_t y, int8_t z) {
    static uint8_t last_buttons = 0;
    static int pos_x = 0;
    static int pos_y = 0;
    static bool mouse_point = false;
    int wk_disp_time;
    int mouse_move_x;
    int mouse_move_y;

//    printf("buttons: %d, x: %d, y: %d, z: %d\r\n", buttons, x, y, z);

    pos_x += x;
    mouse_move_x = x;
    if (pos_x < 0) {
        pos_x = 0;
        mouse_move_x = 0;
    }
    if (pos_x > (LCD_PIXEL_WIDTH - 10)) {
        pos_x = LCD_PIXEL_WIDTH - 10;
        mouse_move_x = 0;
    }

    pos_y += y;
    mouse_move_y = y;
    if (pos_y < 0) {
        pos_y = 0;
        mouse_move_y = 0;
    }
    if (pos_y > (LCD_PIXEL_HEIGHT - 10)) {
        pos_y = (LCD_PIXEL_HEIGHT - 10);
        mouse_move_y = 0;
    }
    SetMousePos(pos_x, pos_y);

    if (mouse_point != false) {
        if ((buttons & 0x01) != 0) {
            SetMove(mouse_move_x, mouse_move_y);
        } else if (((buttons & 0x01) == 0) && ((last_buttons & 0x01) != 0)) {
            SetMoveEnd(mouse_move_x, mouse_move_y);
        } else {
            SetEfectDisp();
        }
    } else {
        // left
        if (((buttons & 0x01) == 0) && ((last_buttons & 0x01) != 0)) {
            SetEfectReq(EVENT_NEXT);
        }

        // rigth
        if (((buttons & 0x02) == 0) && ((last_buttons & 0x02) != 0)) {
            SetEfectReq(EVENT_PREV);
        }
    }

    if (((buttons & 0x04) != 0) && ((last_buttons & 0x04) == 0)) {
        mouse_point = !mouse_point;
        SetMousePointer(mouse_point);
    }

    if (z != 0) {
        if (mouse_point == false) {
            wk_disp_time = GetDispWaitTime();
            if (z > 0) {
                wk_disp_time += 1000;
                if (wk_disp_time > 15000) {
                    wk_disp_time = 15000;
                }
            } else {
                wk_disp_time -= 1000;
                if (wk_disp_time < 1000) {
                    wk_disp_time = 0;
                }
            }
            SetDispWaitTime(wk_disp_time);
        } else {
            SetZoom(pos_x + 3, pos_y + 3, pos_x + 3, pos_y + 3,
                      GetEfectMagnification() * (1.0f + (float_t)z / 10.0f));
        }
    }

    last_buttons = buttons;
}

void mouse_task(void) {
    USBHostMouse mouse;

    while (1) {
        /* try to connect a USB mouse */
        while (!mouse.connect()) {
            Thread::wait(500);
        }

        /* when connected, attach handler called on mouse event */
        mouse.attachEvent(onMouseEvent);

        /* wait until the mouse is disconnected */
        while (mouse.connected()) {
            Thread::wait(500);
        }
    }
}

