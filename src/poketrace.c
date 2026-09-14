#include "types.h"
#include "eeprom_address.h"
#include <stddef.h>
#include "project.h"
#include "display.h"
#include <machine.h>
#include "battle.h"
#include "beep.h"
#include "eeprom.h"
#include "home.h"
#include "pad.h"
#include "common.h"
#include "poketrace.h"
#include "scratch.h"

/* Poke Radar view of the shared UI state. */

extern const u8 g_pokeradarGrassCountByStage[];
extern const u8 g_minigameSlotXPositions[4];
extern const u8 g_pokeradarGrassDelayRangeByStage[];
extern const u8 g_pokeradarItemFrameIndexByStage[];

void RollEncounter(void);
void SelectGrass(void);

#define PW_EEPROM_MIRROR_BYTE_COUNT 0x68
#define PW_EEPROM_DAILY_EVENT_FLAGS EEPROM_EVENTS

/* Bonus-course gate, then three ordinary encounters. */
void RollEncounter(void)
{
  u8 bitIndex;
  DeviceStatus *record;
  EncounterRule *gate;
  u8 dailyFlags;
  u32 value;
  u8 roll;
  Course *course;
  u8 index;

  g_ui.view.radar.encounter = 0;
  if (g_state.save.bonusCourse != 0) {
    bitIndex = EepromReadByte(PW_EEPROM_MEMBER_ADDRESS(
        EEPROM_BONUS_COURSE, BonusResources, values.pokemonReceipt));
    ScratchReset();
    record = ScratchAlloc(PW_EEPROM_MIRROR_BYTE_COUNT);
    if (StatusHasReceived(record, bitIndex) == 0) {
      dailyFlags = EepromReadByte(PW_EEPROM_DAILY_EVENT_FLAGS);
      if ((dailyFlags & 0x20) == 0) {
        ScratchReset();
        gate = ScratchAlloc(sizeof(EncounterRule));
        EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                            values.encounterRule),
                   gate, sizeof(*gate));
        if (g_state.dailySteps >=
            ((u16)((gate->stepsLe >> 8) | (gate->stepsLe << 8)))) {
          value = RandomNext() % 100ul;
          if (gate->chance > value) {
            g_ui.view.radar.encounter = 4;
            value = RandomNext();
            g_ui.view.radar.reverseFrame = (((value >> 3) & 1) + 3);
            return;
          }
        }
      }
    }
  }

  ScratchReset();
  record = ScratchAlloc(sizeof(((WalkData *)0)->pokemon));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon), record,
             sizeof(((WalkData *)0)->pokemon));
  roll = (RandomNext() % 100ul);
  ScratchReset();
  course = ScratchAlloc(sizeof(Course));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values),
             course, sizeof(Course));
  index = 0;
  while (index < 3) {
    if (g_state.dailySteps >= ((u16)((course->encounterSteps[index] >> 8) | (course->encounterSteps[index] << 8)))) {
      if (roll < course->encounterChance[index]) {
        g_ui.view.radar.encounter = (index + 1);
        value = RandomNext();

        {
          u8 draw = ((value >> 3) & 1);
          index = -index;
          index += draw;
          index += 3;
        }
        g_ui.view.radar.reverseFrame = index;
        return;
      }
    }
    index++;
  }
  g_ui.view.radar.encounter = 3;
  value = RandomNext();
  g_ui.view.radar.reverseFrame = (((value >> 3) & 1) + 1);
}

/* Clear substates and pick the first grass cell. */

void RadarInit(void)
{
  RollEncounter();
  g_ui.view.radar.substate = 0;
  g_ui.view.radar.cursor = 0;
  g_ui.view.radar.stage = 0;
  g_ui.view.radar.delay = 5;
  g_ui.view.radar.remaining =
      g_pokeradarGrassCountByStage[g_ui.view.radar.stage];
  g_ui.view.radar.target = ((((u16)((u16)RandomNext() << 3)) >> 8) & 3);
  g_ui.view.radar.revealFrame = 0;
}

/* Four-slot cursor, confirm, and timer decay. */
void SelectGrass(void)
{
  if (InputPressed(BUTTON_LEFT) != 0) {
    g_ui.view.radar.cursor = ((g_ui.view.radar.cursor + 3) & 3);
    BeepLoadScore(2);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    g_ui.view.radar.cursor = ((g_ui.view.radar.cursor + 1) & 3);
    BeepLoadScore(2);
  }
  if ((InputPressed(BUTTON_CENTER) != 0) && (g_ui.view.radar.remaining != 0)) {
    if (g_ui.view.radar.cursor == g_ui.view.radar.target) {
      BeepLoadScore(3);
      g_ui.view.radar.substate = 3;
      g_ui.view.radar.revealFrame = 0x10;
      return;
    }
    if (g_ui.view.radar.stage == 0) {
      BeepLoadScore(4);
      return;
    }
  } else {
    {
      u8 delay;

      delay = g_ui.view.radar.delay;
      if (delay != 0) {
        delay--;
        g_ui.view.radar.delay = delay;
        return;
      }
    }
    {
      u8 remaining;

      remaining = g_ui.view.radar.remaining;
      if (remaining != 0) {
        remaining--;
        g_ui.view.radar.remaining = remaining;
      }
      if (g_ui.view.radar.remaining != 0) {
        return;
      }
    }
  }
  BeepLoadScore(0x0e);
  SetView(VIEW_RADAR_FAILURE);
}

/* Selection, success handoff, and restage. */
void RadarUpdate(void)
{
  u32 value;

  if (BeepHasScore() != 0) {
    return;
  }
  switch (g_ui.view.radar.substate) {
  case 0:
    SelectGrass();
    return;
  case 1:
    if (g_ui.view.radar.remaining <= 4) {
      return;
    }
    BattleInit();
    SetView(VIEW_BATTLE);
    return;
  case 2:
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    BeepLoadScore(0);
    HomeInit();
    SetView(VIEW_HOME);
    return;
  case 3:
    if (g_ui.view.radar.revealFrame == 0) {
      return;
    }
    if (--g_ui.view.radar.revealFrame != 0) {
      return;
    }
    if (g_ui.view.radar.stage >= (g_ui.view.radar.reverseFrame - 1)) {
      g_ui.view.radar.substate = 1;
      g_ui.view.radar.revealFrame = 1;
      g_ui.view.radar.remaining = 0;
      return;
    }
    g_ui.view.radar.substate = 0;
    value = RandomNext();
    g_ui.view.radar.delay =
        ((u8)(value >> 2) %
             g_pokeradarGrassDelayRangeByStage[g_ui.view.radar.stage] +
         0x10);
    g_ui.view.radar.remaining =
        g_pokeradarGrassCountByStage[++g_ui.view.radar.stage];
    value = RandomNext();
    g_ui.view.radar.target = ((value >> 5) & 3);
    return;
  }
}

/* Grass, cursor, response, and encounter panels. */
void RadarRender(void)
{
  const UiResources *image;
  u8 *raster;
  const u8 *source;
  s16 i;

  image = (const UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(sizeof(image->radarIcons));
  source = image->arrows + ((g_state.uiFrame & 1) + 9) * 0x10;
  EepromRead((u16)source, raster, 0x10);
  DisplayBlit((g_minigameSlotXPositions[g_ui.view.radar.cursor] - 8),
              ((g_ui.view.radar.cursor & 1) * 0x18 + 8), 0x08, 0x08, raster);
  source = image->radarGrass;
  EepromRead((u16)source, raster, sizeof(image->radarGrass));
  i = 0;
  do {
    DisplayBlit(g_minigameSlotXPositions[i], ((i & 1) * 0x18), 0x20, 0x18,
                raster);
    i++;
  } while (i < 4);

  if (g_ui.view.radar.revealFrame != 0) {
    source = image->radarIcons;
    EepromRead((u16)source, raster, sizeof(image->radarIcons));
    DisplayBlit((g_minigameSlotXPositions[g_ui.view.radar.target] + 0x10),
                ((g_ui.view.radar.target & 1) * 0x18), 0x10, 0x10,
                raster + 192);
    switch (g_ui.view.radar.substate) {
    case 3:
      RenderMessage(0x30, MESSAGE_RADAR_RESPONSE, 0x0f, 0);
      break;
    case 1:
      DisplayFillRect(0, 0, 0x60, (g_ui.view.radar.remaining * 8), 3);
      DisplayFillRect(0, (0x40 - g_ui.view.radar.remaining * 8), 0x60,
                      (g_ui.view.radar.remaining * 8), 3);
      g_ui.view.radar.remaining++;
      break;
    case 2:
      RenderDecoratedNumber(0x02, 0x20, g_ui.view.radar.encounter, 0x0d);
      RenderMessage(0x30, MESSAGE_POKEMON_GIVEN, 0x0e, 1);
      break;
    }
  } else {
    RenderMessage(0x30, MESSAGE_RADAR_SEARCH, 0x0f, 0);
    if (g_ui.view.radar.delay == 0) {
      source = image->radarIcons;
      EepromRead((u16)source, raster, sizeof(image->radarIcons));
      DisplayBlit((g_minigameSlotXPositions[g_ui.view.radar.target] + 0x10),
                  ((g_ui.view.radar.target & 1) * 0x18), 0x10, 0x10,
                  raster +
                      g_pokeradarItemFrameIndexByStage[g_ui.view.radar.stage] *
                          64);
    }
  }
  RenderBattery(0, 0);
}

/* Any-button return home from the escape screen. */
void RadarFailureUpdate(void)
{
  if (InputPressed(BUTTON_ANY) != 0) {
    g_ui.view.radar.cursor = 0;
    BeepLoadScore(4);
    HomeInit();
    SetView(VIEW_HOME);
  }
}

/* Draw the four grass patches and the escape message. */
void RadarFailureRender(void)
{
  UiResources *image;
  u8 *raster;
  u8 *source;
  s16 i;

  image = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(sizeof(image->radarGrass));
  source = image->radarGrass;
  EepromRead((u16)source, raster, sizeof(image->radarGrass));
  i = 0;
  do {
    DisplayBlit(g_minigameSlotXPositions[i], ((i & 1) * 0x18), 0x20, 0x18,
                raster);
    i++;
  } while (i < 4);
  RenderMessage(0x30, MESSAGE_RADAR_ESCAPE, 0x0f, 1);
  RenderBattery(0, 0);
}

/* grass patches per stage */
const u8 g_pokeradarGrassCountByStage[4] = {0x40, 0x30, 0x20, 0x18};

/* grass delay range per stage */
const u8 g_pokeradarGrassDelayRangeByStage[3] = {0x10, 0x20, 0x30};

/* item frame index per stage */
const u8 g_pokeradarItemFrameIndexByStage[4] = {0x00, 0x00, 0x01, 0x02};

/* Grass-slot x positions. */
const u8 g_minigameSlotXPositions[4] = {0x08, 0x10, 0x38, 0x40};
