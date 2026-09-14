#ifndef PW_DISPLAY_H
#define PW_DISPLAY_H

#include "types.h"
#include "raster_column.h"

/* Raster columns contain two plane bytes; coordinates are pixels. */

/* The caller supplies the following page touched by shifted high bits. */
void RasterOr(u8 width, u8 height, const u8 *source, u8 x, u8 y,
              u8 *destination, u8 destinationWidth, u8 destinationHeight);
void DisplayInit(void);
void DisplaySetContrast(u8 contrastDelta);
void DisplayAddress(u8 x, u8 y);
void DisplayToggleBank(void);
void DisplaySelectBank(u8 bank);
void DisplayFill(u8 pixelValue);
void DisplayFillRect(u8 x, u8 y, u8 width, u8 height, u8 fillPattern);
void DisplayExitPowerSave(void);
void DisplayEnterPowerSave(void);
void DisplayClear(u8 rasterBitSpan);
void DisplayBlit(u8 x, u8 y, u8 width, u8 height, const u8 *raster);
void DisplayWriteSpan(s8 x, s8 y, uint columnCount, uint rowCount,
                      RasterColumn *raster);
void DisplayText(u8 x, u8 y, const char *glyphStream);
void DisplayFrame45(void);
void DisplayFrame67(void);
void DisplayRule(void);
void RenderDecoratedNumber(u8 x, u8 y, u16 value, u8 edgeFlags);

#endif
