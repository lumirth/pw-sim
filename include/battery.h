#ifndef PW_BATTERY_H
#define PW_BATTERY_H

#include "types.h"

uint BatteryProtect(uint value);
u8 BatteryVerify(u16 value);
uint BatterySample(void);
u8 BatteryLow(u16 scaleFactor);
void BatteryUpdate(void);

#endif
