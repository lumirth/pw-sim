#ifndef PW_ACCELEROMETER_H
#define PW_ACCELEROMETER_H

#include "types.h"

u8 AccelRead(u8 registerAddress, u8 *destination, u8 byteCount);
void AccelWrite(u8 registerAddress, u8 value);
u8 AccelInit(void);

#endif
