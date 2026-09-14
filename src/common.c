#include "types.h"
#include "raster_column.h"
#include "eeprom_address.h"
#include <stddef.h>
#include <machine.h>
#include "iodefine.h"
#include "project.h"
#include "display.h"
#include "eeprom.h"
#include "common.h"
#include "power.h"
#include "scratch.h"

#define PW_ICON_FRAME_BYTES 0xc0
#define PW_NAME_RASTER_BYTES 0x140
#define PW_ITEM_RASTER_BYTES 0x180

/* Apply edge flags to two bands of width words. Bits 0/1 add top/bottom edges;
 * bits 2/3 fill the first/last columns. The height argument is unused. */
void RasterBorder(u8 width, u8 height, u16 *raster, u8 edgeFlags)
{
  u16 *cursor;
  u8 index;

  cursor = raster;
  if ((edgeFlags & 1) != 0) {
    index = 0;
    while (index < width) {
      cursor[index] = (cursor[index] | 0x0101);
      index++;
    }
  }
  {
    if ((edgeFlags & 4) != 0) {
      cursor[0] = 0xffff;
      cursor[width] = 0xffff;
    }
    if ((edgeFlags & 8) != 0) {
      (cursor + width)[-1] = 0xffff;
      (cursor + width * 2)[-1] = 0xffff;
    }
    if ((edgeFlags & 2) != 0) {
      index = 0;
      while (index < width) {
        (cursor + width)[index] = ((cursor + width)[index] | 0x8080);
        index++;
      }
    }
  }
}

void RenderHeldPokemon(u8 x, u8 y)
{
  u8 *raster;
  u8 *source;
  enum { base = EEPROM_COURSE + offsetof(CourseResources, pokemonImage) };

  ScratchReset();
  raster = ScratchAlloc(PW_ICON_FRAME_BYTES);
  source = (u8 *)(base + (g_state.uiFrame & 1) * PW_ICON_FRAME_BYTES);
  EepromRead((u16)source, raster, PW_ICON_FRAME_BYTES);
  DisplayBlit(x, y, 32, 24, raster);
}

void RenderHeldPokemonMirrored(u8 x, u8 y)
{
  u8 *raster;
  u8 *source;
  enum { base = EEPROM_COURSE + offsetof(CourseResources, pokemonImage) };

  ScratchReset();
  raster = ScratchAlloc(PW_ICON_FRAME_BYTES);
  source = (u8 *)(base + (g_state.uiFrame & 1) * PW_ICON_FRAME_BYTES);
  EepromRead((u16)source, raster, PW_ICON_FRAME_BYTES);
  RasterMirror(32, 24, raster);
  DisplayBlit(x, y, 32, 24, raster);
}

void RenderEnemyPokemon(u8 x, u8 y, u8 recordIndex)
{
  u8 *raster;
  u8 *source;
  enum {
    stride = sizeof(((CourseResources *)0)->encounterImages) / COURSE_ENCOUNTERS
  };

  ScratchReset();
  raster = ScratchAlloc(PW_ICON_FRAME_BYTES);
  source = ((CourseResources *)EEPROM_COURSE)->encounterImages +
           (recordIndex * stride + (g_state.uiFrame & 1) * PW_ICON_FRAME_BYTES);
  EepromRead((u16)source, raster, PW_ICON_FRAME_BYTES);
  DisplayBlit(x, y, 32, 24, raster);
}

void RenderPeerPokemon(u8 x, u8 y, u8 mirrorRequested)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(PW_ICON_FRAME_BYTES);
  EepromRead((EEPROM_PEER_IMAGE + (g_state.uiFrame & 1) * PW_ICON_FRAME_BYTES),
             raster, PW_ICON_FRAME_BYTES);
  if (mirrorRequested != 0) {
    RasterMirror(32, 24, raster);
  }
  DisplayWriteSpan(x, y, 0x20, 0x18, (RasterColumn *)raster);
}

void RenderHeldName(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(PW_NAME_RASTER_BYTES);
  if (y == PW_DISPLAY_BAND_6_ORIGIN_Y) {
    DisplayFrame67();
  } else {
    DisplayFrame45();
  }
  source = ((CourseResources *)EEPROM_COURSE)->pokemonName;
  EepromRead((u16)source, raster, PW_NAME_RASTER_BYTES);
  RasterBorder(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderPeerName(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;
  enum { addr = EEPROM_PEER_IMAGE + 384 };

  ScratchReset();
  raster = ScratchAlloc(PW_NAME_RASTER_BYTES);
  DisplayFrame45();
  EepromRead(addr, raster, PW_NAME_RASTER_BYTES);
  RasterBorder(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderDistributionName(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(PW_NAME_RASTER_BYTES);
  if (y == PW_DISPLAY_BAND_6_ORIGIN_Y) {
    DisplayFrame67();
  } else {
    DisplayFrame45();
  }
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, pokemonName),
      raster, PW_NAME_RASTER_BYTES);
  RasterBorder(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderBonusName(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(PW_NAME_RASTER_BYTES);
  if (y == PW_DISPLAY_BAND_6_ORIGIN_Y) {
    DisplayFrame67();
  } else {
    DisplayFrame45();
  }
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      pokemonName),
             raster, PW_NAME_RASTER_BYTES);
  RasterBorder(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderTreasure(u8 x, u8 y)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->treasure));
  source = ((UiResources *)EEPROM_UI)->treasure;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->treasure));
  DisplayBlit(x, y, 8, 8, raster);
}

void RenderEnemyName(u8 x, u8 y, u8 recordIndex, u8 edgeFlags)
{
  CourseResources *course;
  u8 *raster;

  course = (CourseResources *)EEPROM_COURSE;
  ScratchReset();
  raster = ScratchAlloc(PW_ITEM_RASTER_BYTES);
  if (y == PW_DISPLAY_BAND_6_ORIGIN_Y) {
    DisplayFrame67();
  } else {
    DisplayFrame45();
  }
  EepromRead((u16)&course->encounterNames[recordIndex * PW_NAME_RASTER_BYTES],
             raster, PW_NAME_RASTER_BYTES);
  RasterBorder(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderCourseItem(u8 x, u8 y, u8 recordIndex, u8 edgeFlags)
{
  CourseResources *course;
  u8 *raster;
  u16 length;
  u8 *source;

  course = (CourseResources *)EEPROM_COURSE;
  ScratchReset();
  length = PW_ITEM_RASTER_BYTES;
  raster = ScratchAlloc(length);
  source = course->itemImages + recordIndex * length;
  EepromRead((u16)source, raster, length);
  RasterBorder(96, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 96, 16, raster);
}

void RenderDistributionItem(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(PW_ITEM_RASTER_BYTES);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_ITEM, EventItem, itemName),
             raster, PW_ITEM_RASTER_BYTES);
  RasterBorder(96, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 96, 16, raster);
}

void RenderItemCheckTreasure(u8 x, u8 y)
{
  u8 *raster;
  u8 *source;

  source = ((UiResources *)EEPROM_UI)->itemTreasure;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->itemTreasure));
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->itemTreasure));
  DisplayBlit(x, y, 32, 24, raster);
}

void RenderItemCheckPresent(u8 x, u8 y)
{
  u8 *raster;
  u8 *source;

  source = ((UiResources *)EEPROM_UI)->itemPresent;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->itemPresent));
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->itemPresent));
  DisplayBlit(x, y, 32, 24, raster);
}

/* Set an event-receipt bit in DeviceStatus and commit the mirrored registration
 * records. Event ID zero is reserved and does nothing. */
void StatusSetReceived(DeviceStatus *status, u8 eventId)
{
  u8 byteIndex;

  if (eventId != 0) {
    byteIndex = (eventId >> 3);
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                     sizeof(DeviceStatus));
    status->receivedEvents[byteIndex] |= (1u << (eventId & 7));
    EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                      sizeof(DeviceStatus));
  }
}

/* Load the mirrored DeviceStatus and test its event-receipt bit. Event ID zero
 * returns false. */
u8 StatusHasReceived(DeviceStatus *status, u8 eventId)
{
  u8 byteIndex;

  if (eventId != 0) {
    byteIndex = (eventId >> 3);
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                     sizeof(DeviceStatus));
    if ((status->receivedEvents[byteIndex] & (1u << (eventId & 7))) != 0) {
      return 1;
    }
  }
  return 0;
}

/* Shift whole eight-row pages in place, merging the neighboring page before
 * advancing. The byte loop indices remain distinct from the input dimensions.
 */
void RasterShift(u8 width, u8 height, s8 shift, u8 *raster)
{
  u8 band, column;

  if (shift < 0) {
    for (band = 0; band < height / 8; ++band) {
      for (column = 0; column < width * 2; ++column) {
        raster[column] >>= -shift;
        if (band != height / 8 - 1)
          raster[column] =
              raster[column] | (raster[width * 2 + column] << (8 + shift));
      }
      raster += width * 2;
    }
  } else {
    raster += (height / 8 - 1) * width * 2;
    for (band = height / 8; band != 0; --band) {
      for (column = 0; column < width * 2; ++column) {
        raster[column] <<= shift;
        if (band != 1)
          raster[column] =
              raster[column] | (raster[column - width * 2] >> (8 - shift));
      }
      raster -= width * 2;
    }
  }
}

/* Walk WalkData.pokemon[3] and return the first slot whose species id is zero,
 * or 3 when every slot is occupied. */
u8 PokemonSlotFindEmpty(Pokemon *pokemon)
{
  u8 i;

  for (i = 0; i < 3; i++) {
    if (pokemon[i].id == 0) {
      return i;
    }
  }
  return 3;
}

/* Load the live Course from EEPROM and return itemNumber[index] for the
 * selected course-item slot. */
u16 CourseItemNumber(u8 courseItemIndex)
{
  Course *course;

  ScratchReset();
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  return course->itemId[courseItemIndex];
}

/* Walk WalkData.item[3] and return the first slot whose item ID is zero, or 3
 * when every slot is occupied. */
u8 ItemSlotFindEmpty(Item *item)
{
  u8 i;

  for (i = 0; i < 3; i++) {
    if (item[i].id == 0) {
      return i;
    }
  }
  return 3;
}

/* Add Watts, saturate the balance, and commit the mirrored save
 * records. */
void WattsAdd(u8 increment)
{
  g_state.save.watts = (g_state.save.watts + increment);
  if (g_state.save.watts > WATTS_MAX) {
    g_state.save.watts = WATTS_MAX;
  }
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
}

/* Draw a 16-bit decimal value with the band-4/5 edge, cleared digit well and
 * Watt icon. Apply the stripe to the first 16 of the 32 loaded words. */
void RenderDecoratedNumber(u8 x, u8 y, u16 value, u8 edgeFlags)
{
  u8 *raster;
  u8 i;
  u16 *words;
  const u8 *source;

  ScratchReset();
  raster = ScratchAlloc(0x140);
  DisplayFrame45();
  DisplayFillRect(1, 0x28, 0x5e, 8, 0);
  RenderDecimal((x + 8), y, value, 1);
  source = ((const UiResources *)EEPROM_UI)->watts;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->watts));
  words = (u16 *)raster;
  for (i = 0; i < 0x10; i++) {
    words[i] |= 0x0101;
  }
  DisplayBlit((x + 0x10), y, 16, 16, raster);
}

/* Read the ten digit glyphs and draw the decimal value right-to-left from (x,
 * y). The optional first-band stripe fills eight words of each nine-word glyph.
 */
void RenderDecimal(u8 x, u8 y, u32 value, u8 firstBandFill)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(0x140);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, digits), raster,
             0x140);
  if (firstBandFill != 0) {
    u16 *cursor;
    u8 n;

    cursor = (u16 *)raster;
    n = 10;
    do {
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor += 9;
      n--;
    } while (n != 0);
  }
  if (value == 0) {
    DisplayBlit(x, y, 8, 16, raster);
  } else {
    u8 digit;

    while (value != 0) {
      digit = (value % 10);
      DisplayBlit(x, y, 8, 16, raster + digit * 0x20);
      value = value / 10;
      x = (x - 8);
    }
  }
}

/* Read UiResources.message[id], apply edge flags, optionally stamp the
 * next-cursor overlay when blink is requested and the UI phase bit is set, then
 * blit the 96x16 panel at (0, y). The message raster is 0x180 bytes; the AND
 * loop strides raster by 2 (even/odd halves) and overlay by 1; the OR loop then
 * visits all 16 raster bytes contiguously. The two mask bytes per column and
 * two plane bytes have distinct strides. */
void RenderMessage(u8 y, u8 messageId, u8 edgeFlags, u8 blink)
{
  u8 *raster;
  u8 *overlay;
  u16 length;
  u8 *source;
  u8 i;

  ScratchReset();
  length = 0x180;
  raster = ScratchAlloc(length);
  overlay = ScratchAlloc(0x18);
  source = ((UiResources *)EEPROM_UI)->messages + messageId * length;
  EepromRead((u16)source, raster, length);
  RasterBorder(0x60, 0x10, (u16 *)raster, edgeFlags);
  if (blink != 0) {
    if (((g_state.uiFrame >> 1) & 1) != 0) {
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, next),
                 overlay, 0x18);
      i = 0;
      do {
        (raster + i * 2)[0x170] &= overlay[i + 0x10];
        (raster + (i * 2 + 1))[0x170] &= overlay[i + 0x10];
        i++;
      } while (i < 8);
      i = 0;
      do {
        raster[i + 0x170] = (raster[i + 0x170] | overlay[i]);
        i++;
      } while (i < 0x10);
    }
  }
  DisplayBlit(0, y, 0x60, 0x10, raster);
}

/* Horizontally reverse a two-bitplane raster in place by XOR-swapping column
 * words from the outside in, one 8-row page at a time. */
void RasterMirror(u8 width, u8 height, u8 *raster)
{
  u8 band;
  u8 left;
  u8 last;
  u8 right;
  u16 *words;
  s16 half;
  s16 bands;

  words = (u16 *)raster;
  band = 0;
  half = (width) >> 1;
  bands = (height) >> 3;
  last = (width + 0xff);
  while (band < bands) {
    right = last;
    left = 0;
    while (left < half) {
      words[left] ^= words[right];
      words[right] ^= words[left];
      words[left] ^= words[right];
      left++;
      right--;
    }
    words += width;
    band++;
  }
}

/* Draw the 8x8 UiResources.battery icon when shared-event bit 1 is set and the
 * UI phase gate (phase >> 2) is clear. */
void RenderBattery(u8 x, u8 y)
{
  u8 *raster;

  if (g_state.events.bits.batteryLow == 0) {
    return;
  }
  if (((g_state.uiFrame >> 2) & 1) != 0) {
    return;
  }
  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->battery));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, battery), raster,
             sizeof(((UiResources *)0)->battery));
  DisplayBlit(x, y, 8, 8, raster);
}

/* Apply a one-byte-per-column mask to both destination planes, then OR in one
 * source page. Clip to destination width and spill into the next page when y is
 * unaligned. The height argument is unused. */
void RasterMasked(u8 *destination, u8 destinationWidth, u8 destinationHeight,
                  const u8 *source, const u8 *mask, u8 x, u8 y, u8 width,
                  u8 height)
{
  u8 column, span;

  if (x + width > destinationWidth)
    span = destinationWidth - x;
  else
    span = width;
  destination += ((y / 8) * destinationWidth + x) * 2;
  for (column = 0; column < span; ++column) {
    *(destination + column * 2) =
        *(destination + column * 2) &
        ((*(mask + column) << (8 - (y & 7))) | (0xff >> (y & 7)));
    *(destination + column * 2 + 1) =
        *(destination + column * 2 + 1) &
        ((*(mask + column) << (y & 7)) | (0xff >> (8 - (y & 7))));
    if (y + 8 < destinationHeight) {
      *(destination + (column + destinationWidth) * 2) =
          *(destination + (column + destinationWidth) * 2) &
          ((*(mask + column) >> (8 - (y & 7))) | (0xff << (y & 7)));
      *(destination + (column + destinationWidth) * 2 + 1) =
          *(destination + (column + destinationWidth) * 2 + 1) &
          ((*(mask + column) >> (8 - (y & 7))) | (0xff << (y & 7)));
    }
  }
  for (column = 0; column < span; ++column) {
    *(destination + column * 2) =
        *(destination + column * 2) | (*(source + column * 2) << (y & 7));
    *(destination + column * 2 + 1) = *(destination + column * 2 + 1) |
                                      (*(source + column * 2 + 1) << (y & 7));
    if (y + 8 < destinationHeight) {
      *(destination + (column + destinationWidth) * 2) =
          *(destination + (column + destinationWidth) * 2) |
          (*(source + column * 2) >> (8 - (y & 7)));
      *(destination + (column + destinationWidth) * 2 + 1) =
          *(destination + (column + destinationWidth) * 2 + 1) |
          (*(source + column * 2 + 1) >> (8 - (y & 7)));
    }
  }
}

/* WDT unlock sequence, then stop the counter. */
void WatchdogDisable(void)
{
  WDT.TCSRWD1.BYTE = 0x9e;
  WDT.TCSRWD1.BYTE = 0xa2;
  WDT.TCSRWD1.BYTE = 0x8e;
}

/* WDT unlock, enable, and load the reload value. */
void WatchdogStart(void)
{
  WDT.TCSRWD1.BYTE = 0x9e;
  WDT.TCSRWD1.BYTE = 0xa6;
  WDT.TCSRWD1.BYTE = 0x8e;
  WDT.TMWD.BYTE = 0xf5;
}

/* Motion state and scratch allocations share storage with the infrared receive
 * frame. */

/* Reset the upward-growing scratch allocator. */
void ScratchReset(void)
{
  g_state.scratchUsed = 0;
}

/* Allocate byte_count bytes from the scratch arena. Sleep if the stored cursor
 * has already passed its 0x400-byte limit. */
void *ScratchAlloc(u16 byteCount)
{
  u16 *cursor;
  u16 next;
  u8 *old;

  cursor = &g_state.scratchUsed;
  next = *cursor;
  old = g_work.motion.scratch.layout.scratch + next;
  next += byteCount;
  *cursor = next;
  if (*cursor > 0x400) {
    __builtin_trap();
  }
  return old;
}

/* Emit at most one accounted step per application tick. Credit hour, day and
 * total steps, award one Watt every 20 steps, then wrap the phase accumulator.
 */
void StepPacingTick(void)
{
  if (g_state.stepPacing.stepsEmitted != g_state.stepPacing.batchSteps) {
    g_state.stepPacing.stepPhase =
        (g_state.stepPacing.stepPhase + g_state.stepPacing.batchSteps);
    if (g_state.stepPacing.stepPhase > 64) {
      g_state.hourSteps++;
      if (g_state.hourSteps > 9999) {
        g_state.hourSteps = 9999;
      }
      g_state.dailySteps++;
      if (g_state.dailySteps > 99999ul) {
        g_state.dailySteps = 99999ul;
      }
      StoreTotalSteps(g_state.save.totalSteps + 1);
      g_state.save.stepsTowardNextWatt++;
      if (g_state.save.stepsTowardNextWatt >= STEPS_PER_WATT) {
        g_state.save.stepsTowardNextWatt =
            (g_state.save.stepsTowardNextWatt - STEPS_PER_WATT);
        g_state.save.watts++;
        if (g_state.save.watts > WATTS_MAX) {
          g_state.save.watts = WATTS_MAX;
        }
      }
      g_state.stepPacing.stepsEmitted++;
      if (g_state.stepPacing.stepsEmitted > g_state.stepPacing.batchSteps) {
        g_state.stepPacing.stepsEmitted = g_state.stepPacing.batchSteps;
      }
      g_state.stepPacing.stepPhase = (g_state.stepPacing.stepPhase - 64);
    }
  }
}

/* Mode 0 selects the low-power clock and sets shared-event bit 4; mode 1
 * restores the normal clock and clears that bit. Both paths sleep after the
 * SYSCR write. */
void ClockSleep(uint mode)
{
  if (mode == 0) {
    SYSCR1.BYTE = 0xa7;
    SYSCR2.BYTE = 0xe0;
    g_state.events.bits.lowPowerClock = 1;
    sleep();
  } else if (--mode == 0) {
    SYSCR1.BYTE = 0xaf;
    SYSCR2.BYTE = 0xe3;
    g_state.events.bits.lowPowerClock = 0;
    sleep();
  }
}

/* Kick the watchdog: clear the counter under the write-enable key. */
void WatchdogService(void)
{
  WDT.TCSRWD1.BYTE = 0x5e;
  WDT.TCWD = 0;
  WDT.TCSRWD1.BYTE = 0x9e;
}

/* When shared-event bit 4 marks low-power clock mode, spin 37 times with five
 * NOPs per iteration so the delay tracks the active clock. */
void LowClockDelay(void)
{
  u16 remaining;

  if (g_state.events.bits.lowPowerClock != 0) {
    remaining = 37;
    do {
      nop();
      nop();
      nop();
      nop();
      nop();
      remaining--;
    } while (remaining != 0);
  }
}

void RandomSeed(u32 seed)
{
  g_state.randomState = seed;
}

/* Advance the 32-bit LCG: state = state * 1664525 + 1013904223. */
u32 RandomNext(void)
{
  u32 *state;
  u32 value;

  state = &g_state.randomState;
  value = *state;
  value = value * 1664525ul + 1013904223ul;
  *state = value;
  return value;
}

/* Expand a bit-flagged IR bulk payload: a flags byte selects literal copies or
 * (length, distance) runs, until the remaining-count in packed[1] is exhausted.
 * Compute the run length before adding its three-byte minimum, and consume it
 * in the copy-loop condition. */
void BulkDecode(u8 *packed, u8 *out)
{
  u8 remaining;
  u8 flags;
  s8 bits;
  u8 length;
  u16 back;

  remaining = packed[1];
  packed += 4;
  while (remaining != 0) {
    flags = *packed++;
    bits = 8;
    while (--bits >= 0) {
      if ((flags & 0x80) == 0) {
        *out++ = *packed++;
        remaining--;
      } else {
        length = (*packed / 16);
        length += 3;
        packed++;
        back = (*packed++ + 1);
        remaining = (remaining - length);
        back = -back;
        do {
          *out = *(out + back);
          out++;
        } while (--length != 0);
      }
      if (remaining == 0) {
        break;
      }
      flags = (flags << 1);
    }
  }
}
