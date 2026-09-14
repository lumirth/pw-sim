#ifndef PW_RTC_H
#define PW_RTC_H

#include "types.h"

void RtcDispatch(void);
void RtcSetTime(u32 seconds);
void RtcReadStable(u8 *secondOut, u8 *minuteOut, u8 *hourOut);

#endif
