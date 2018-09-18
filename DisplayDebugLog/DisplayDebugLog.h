#ifndef DISPLAY_DEBUG_LOG_H
#define DISPLAY_DEBUG_LOG_H

#include "mbed.h"
#include "DisplayBace.h"

#define DEBUG_PIXEL_WIDTH                 (320)
#define DEBUG_PIXEL_HEIGHT                (160)

extern void StartDebugLog(DisplayBase * p_display, const int x, const int y);
extern void StopDebugLog(DisplayBase * p_display);
extern int DrawDebugLog(const char *format, ...);
extern void ClearDebugLog(void);

#endif
