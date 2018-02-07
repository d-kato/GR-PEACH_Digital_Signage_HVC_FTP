
#ifndef DISPLAY_EFECT_PROCESSING_H
#define DISPLAY_EFECT_PROCESSING_H

#define EVENT_NONE                          (0)
#define EVENT_DISP                          (1)
#define EVENT_NEXT                          (2)
#define EVENT_PREV                          (3)
#define EVENT_MOVE                          (4)
#define EVENT_MOVE_END                      (5)
#define EVENT_FACE_IN                       (6)
#define EVENT_FACE_OUT                      (7)

extern float GetEfectMagnification(void);
extern int GetDispWaitTime(void);
extern void SetDispWaitTime(int wait_time);
extern void SetMousePointer(bool disp);
extern void SetMousePos(int x, int y);
extern void SetEfectReq(int effect_type);
extern void SetEfectDisp(void);
extern void SetZoom(uint32_t center_x_last, uint32_t center_y_last, uint32_t center_x, uint32_t center_y, float new_magnification);
extern void SetMove(int x, int y);
extern void SetMoveEnd(int x, int y);

#endif
