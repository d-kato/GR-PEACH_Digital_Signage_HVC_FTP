
#include "mbed.h"
#include <cstdarg>
#include <vector>
#include "AsciiFont.h"
#include "DisplayDebugLog.h"

#define DEBUG_LINE_HEIGHT (AsciiFont::CHAR_PIX_HEIGHT + 2)
#define DEBUG_LINE_SIZE   (DEBUG_LINE_HEIGHT * DEBUG_BUFFER_STRIDE)

/* DEBUG BUFFER Parameter GRAPHICS_LAYER_1 */
#define DEBUG_BUFFER_BYTE_PER_PIXEL  (2u)
#define DEBUG_BUFFER_STRIDE          (((DEBUG_PIXEL_WIDTH * DEBUG_BUFFER_BYTE_PER_PIXEL) + 31u) & ~31u)

#if defined(__ICCARM__)
/* 32 bytes aligned */
#pragma data_alignment=32
static uint8_t debug_log_buffer[DEBUG_BUFFER_STRIDE * DEBUG_PIXEL_HEIGHT]@ ".mirrorram";
#else
/* 32 bytes aligned */
static uint8_t debug_log_buffer[DEBUG_BUFFER_STRIDE * DEBUG_PIXEL_HEIGHT]__attribute((section("NC_BSS"),aligned(32)));
#endif
static char string_buffer[64];
static AsciiFont debug_font(debug_log_buffer, DEBUG_PIXEL_WIDTH, DEBUG_PIXEL_HEIGHT,
                            DEBUG_BUFFER_STRIDE, DEBUG_BUFFER_BYTE_PER_PIXEL, 0x00000000);
static int debug_idx = 0;

void StartDebugLog(DisplayBase * p_display, const int x, const int y) {
    DisplayBase::rect_t rect;

    memset(debug_log_buffer, 0, sizeof(debug_log_buffer));

    rect.vs = y;
    rect.vw = DEBUG_PIXEL_HEIGHT;
    rect.hs = x;
    rect.hw = DEBUG_PIXEL_WIDTH;
    p_display->Graphics_Read_Setting(
        DisplayBase::GRAPHICS_LAYER_1,
        (void *)debug_log_buffer,
        DEBUG_BUFFER_STRIDE,
        DisplayBase::GRAPHICS_FORMAT_ARGB4444,
        DisplayBase::WR_RD_WRSWA_32_16BIT,
        &rect
    );
    p_display->Graphics_Start(DisplayBase::GRAPHICS_LAYER_1);
}

void StopDebugLog(DisplayBase * p_display) {
    ClearDebugLog();
    p_display->Graphics_Stop(DisplayBase::GRAPHICS_LAYER_1);
}

int DrawDebugLog(const char *format, ...) {
    int res = -1;
    char * wk_pos;

    memset(string_buffer, 0, sizeof(string_buffer));

    // Output to terminal
    va_list args;
    va_start(args, format);
    if (vsprintf(string_buffer, format, args) >= 0) {
        res = vprintf(format, args);
    }
    va_end(args);

    // Output to Display
    wk_pos = strstr(string_buffer,"\r");
    if (wk_pos != NULL) {
        *wk_pos = '\0';
    }
    wk_pos = strstr(string_buffer,"\n");
    if (wk_pos != NULL) {
        *wk_pos = '\0';
    }

    if (debug_idx >= 2) {
        memcpy(&debug_log_buffer[DEBUG_LINE_SIZE * 0], &debug_log_buffer[DEBUG_LINE_SIZE * 1], DEBUG_LINE_SIZE);
    }
    if (debug_idx >= 1) {
        memcpy(&debug_log_buffer[DEBUG_LINE_SIZE * 1], &debug_log_buffer[DEBUG_LINE_SIZE * 2], DEBUG_LINE_SIZE);
    }
    if (debug_idx < 2) {
        debug_idx++;
    }

    debug_font.Erase(0x00000000, 0, DEBUG_LINE_HEIGHT * 2,
                     DEBUG_PIXEL_WIDTH, DEBUG_LINE_HEIGHT);
    debug_font.Erase(0x000000c0, 0, DEBUG_LINE_HEIGHT * 2,
                     (AsciiFont::CHAR_PIX_WIDTH * strlen(string_buffer) + 2), DEBUG_LINE_HEIGHT);
    debug_font.DrawStr(string_buffer, 1, DEBUG_LINE_HEIGHT * 2 + 1, 0x0000ffff, 1);

    return res;
}

void ClearDebugLog(void) {
    memset(debug_log_buffer, 0, sizeof(debug_log_buffer));
}

