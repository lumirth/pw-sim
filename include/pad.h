#ifndef PW_PAD_H
#define PW_PAD_H

#include "types.h"

#define BUTTON_CENTER 0x02
#define BUTTON_LEFT 0x04
#define BUTTON_RIGHT 0x08
#define BUTTON_ANY 0x0E

void InputInit(void);
void InputScan(void);
u8 InputPressed(u8 requestedMask);

#endif
