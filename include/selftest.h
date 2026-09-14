#ifndef PW_SELFTEST_H
#define PW_SELFTEST_H

#include "types.h"

u8 RtcStartupCheck(void);
u8 FactoryAccelDump(void);
u8 FactoryAdcCalibrate(void);
void DiagnosticsInit(void);
void DiagnosticsUpdate(void);
void DiagnosticsRender(void);
void ThresholdInit(void);
void ThresholdUpdate(void);
void ThresholdRender(void);
void ThresholdFailureRender(void);
void BootSignatureWrite(void);
u8 BootSignatureValid(void);

#endif
