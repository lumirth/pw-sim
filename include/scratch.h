#ifndef PW_SCRATCH_H
#define PW_SCRATCH_H

#include "types.h"

/* Allocations remain valid until the next scratch reset or workspace owner
 * change. */

void ScratchReset(void);
void *ScratchAlloc(u16 byteCount);

#endif
