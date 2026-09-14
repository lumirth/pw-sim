#ifndef PW_IRC_STATE_H
#define PW_IRC_STATE_H

#include "flags.h"

#include "types.h"

/* Factory setup mode 3 queues this foreground action. Other completion
 * actions retain the command value that requested them. */
enum { IR_ACTION_FACTORY_RESET = 0xE0 };

typedef struct {
  volatile u32 localSessionWord;
  u32 sessionToken;
  u8 phase;
  u8 timeoutRetryCount;
  u8 reservedAfterTimeout;
  u8 writeOnlyZeroByte;
  u8 checksumFailureCount;
  IrSessionFlags sessionFlags;
  u8 completionAction;
  u8 bulkMode;
  u16 bulkBytesRemaining;
  u16 bulkSourceEepromAddress;
  u16 bulkDestinationEepromAddress;
  u8 bulkChunksCompleted;
  u8 sci3RxDrainByte;
} IrcWork;

#endif
