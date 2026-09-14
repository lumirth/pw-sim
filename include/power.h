#ifndef PW_POWER_H
#define PW_POWER_H

#include "types.h"

void MotionSessionWake(void);
u8 MotionActivityCheck(void);
void MotionSessionStart(void);
void MotionSessionIdleCheck(void);
void StoreTotalSteps(u32 value);

#endif
