#ifndef PW_FIRMWARE_IRC_H
#define PW_FIRMWARE_IRC_H

#include "types.h"

#define PW_IR_TRANSPORT_XOR 0xAAu

u8 *IrPayload(void);
void IrBegin(void);
void IrProtocolTick(void);

void IrInit(void);

#endif
