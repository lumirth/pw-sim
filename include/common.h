#ifndef PW_COMMON_H
#define PW_COMMON_H

#include "types.h"
#include "data.h"

void RenderHeldPokemon(u8 x, u8 y);
void RenderHeldPokemonMirrored(u8 x, u8 y);
void RenderEnemyPokemon(u8 x, u8 y, u8 recordIndex);
void RenderPeerPokemon(u8 x, u8 y, u8 mirrorRequested);
void RenderHeldName(u8 x, u8 y, u8 edgeFlags);
void RenderPeerName(u8 x, u8 y, u8 edgeFlags);
void RenderDistributionName(u8 x, u8 y, u8 edgeFlags);
void RenderBonusName(u8 x, u8 y, u8 edgeFlags);
void RenderTreasure(u8 x, u8 y);
void RenderEnemyName(u8 x, u8 y, u8 recordIndex, u8 edgeFlags);
void RenderCourseItem(u8 x, u8 y, u8 recordIndex, u8 edgeFlags);
void RenderDistributionItem(u8 x, u8 y, u8 edgeFlags);
void RenderItemCheckTreasure(u8 x, u8 y);
void RenderItemCheckPresent(u8 x, u8 y);
void RasterShift(u8 width, u8 height, s8 shift, u8 *raster);
u8 PokemonSlotFindEmpty(Pokemon *pokemon);
u16 CourseItemNumber(u8 courseItemIndex);
u8 ItemSlotFindEmpty(Item *item);
void WattsAdd(u8 increment);
void RenderDecimal(u8 x, u8 y, u32 value, u8 firstBandFill);
void RenderMessage(u8 y, u8 messageId, u8 edgeFlags, u8 blink);
void RasterMirror(u8 width, u8 height, u8 *raster);
void RenderBattery(u8 x, u8 y);
void RasterMasked(u8 *destination, u8 destinationWidth, u8 destinationHeight,
                  const u8 *source, const u8 *mask, u8 x, u8 y, u8 width,
                  u8 height);
void WatchdogDisable(void);
void WatchdogStart(void);
void StepPacingTick(void);
void ClockSleep(uint mode);
void WatchdogService(void);
void LowClockDelay(void);
void RandomSeed(u32 seed);
u32 RandomNext(void);
void BulkDecode(u8 *packed, u8 *out);

#endif
