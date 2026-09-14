#ifndef PW_USER_H
#define PW_USER_H

#include "types.h"

void PersistentReset(u8 factoryClearRequested,
                     u8 clearLifetimeProgressRequested);
void BootRestore(void);
void RtcRestore(void);

#endif
