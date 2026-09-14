#ifndef PW_CHECK_H
#define PW_CHECK_H

#include "types.h"

#define PW_DISCARD_INVENTORY_POKEMON 0
#define PW_DISCARD_INVENTORY_ITEM 1

void DiscardInit(void);

void DiscardUpdate(void);
void DiscardRender(void);

#endif
