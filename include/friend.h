#ifndef PW_FRIEND_H
#define PW_FRIEND_H

#include "types.h"

void PeerFinalize(void);
void PeerUpdate(void);
void PeerRender(void);
u8 SeenPeer(u8 *uniqueId);

#endif
