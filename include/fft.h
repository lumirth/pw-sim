#ifndef PW_FFT_H
#define PW_FFT_H

#include "types.h"

void FftAccumulate(const volatile s8 *samples);

extern const s16 g_sineQ11[];

#endif
