#ifndef PW_INTERFACE_H
#define PW_INTERFACE_H

#include "types.h"

void RenderIrRomFrame(u8 signalIconRequested);
void RenderIrUploadedFrame(u8 optionalPrefixRequested);
void MainTick(void);

extern const u8 g_socialBubbles[];

#endif
