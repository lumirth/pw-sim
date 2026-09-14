#ifndef PW_HOME_H
#define PW_HOME_H

#include "types.h"

void InstallTask(void (*nextTarget)(void));
void TryBeginIr(void);
void SetView(u8 nextState);
void RenderLargePokemon(u8 x, u8 y);
void HomeInit(void);
void HomeUpdate(void);
void RenderFeeling(u8 recordIndex);
void HomeRender(void);

#endif
