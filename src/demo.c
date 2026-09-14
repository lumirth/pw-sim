#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "display.h"
#include "diary.h"
#include <machine.h>
#include "beep.h"
#include "demo.h"
#include "eeprom.h"
#include "home.h"
#include "common.h"
#include "scratch.h"

extern const s8 g_ballDropYPositions6[];

typedef struct {
  u8 x;
  u8 y;
} SparkleXY;

extern const SparkleXY g_arrivalSparkleCoordinates3[];
extern const s8 g_departureAfterimageYPositions6[];

void BallDrop(void);
void BallSparkle(void);
void ArrivalCloud(void);
void ArrivalPokemon(void);
void ArrivalComplete(void);
void RewardInfo(void);
void DeparturePokemon(void);
void DepartureCloud(void);
void DepartureAfterimage(void);
void DepartureComplete(void);
void CommunicationComplete(void);

#define PW_EEPROM_ACTIVE_COURSE_VIEW_BASE EEPROM_COURSE
#define PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH 0xbe
#define PW_PANEL_RASTER_SIZE 0x180

/* Poké Ball drop, then edge-band wipe. */

void BallDrop(void)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  if (g_state.view == VIEW_EVENT_REWARD) {
    source = ((UiResources *)EEPROM_UI)->ball;
  } else {
    source = ((UiResources *)EEPROM_UI)->pokemonShadow;
  }
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->ball));
  DisplayBlit(0x2c, g_ballDropYPositions6[g_ui.view.presentation.frame], 0x08,
              0x08, raster);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
}

/* Settled ball plus sparkle frames. */
void BallSparkle(void)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  source = ((UiResources *)EEPROM_UI)->ball;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->ball));
  DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
  source = ((UiResources *)EEPROM_UI)->star;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->star));
  DisplayBlit(g_arrivalSparkleCoordinates3[g_ui.view.presentation.frame].x,
              g_arrivalSparkleCoordinates3[g_ui.view.presentation.frame].y,
              0x08, 0x08, raster);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
  if (g_ui.view.presentation.frame > 2) {
    g_ui.view.presentation.stage = 1;
    g_ui.view.presentation.frame = 0;
  }
}

/* 32x24 gas cloud. */
void ArrivalCloud(void)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  source = ((UiResources *)EEPROM_UI)->gas;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->gas));
  DisplayBlit(0x20, 0x10, 0x20, 0x18, raster);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  if (g_ui.view.presentation.frame == 0) {
    g_ui.view.presentation.frame++;
  }
}

/* Large held-Pokémon arrival pose. */
void ArrivalPokemon(void)
{
  RenderLargePokemon(0x10, 0x08);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
}

/* Arrival-complete names and home return. */
void ArrivalComplete(void)
{
  RenderHeldPokemon(0x20, 0x04);
  RenderHeldName(0x00, 0x20, 5);
  RenderMessage(0x30, MESSAGE_POKEMON_RECEIVED, 0x0e, 0);
  if (g_ui.view.presentation.frame < 0x10) {
    g_ui.view.presentation.frame++;
  }
  if ((BeepHasScore() == 0) && (g_ui.view.presentation.frame > 8)) {
    HomeInit();
    SetView(VIEW_HOME);
  }
}

/* Eight event-reward subtypes. */
void RewardInfo(void)
{
  u8 *raster;
  UiResources *image;
  u8 *source;
  u16 length;
  u16 iconLength;

  image = (UiResources *)EEPROM_UI;
  iconLength = (sizeof(image->stamps) / 4);
  ScratchReset();
  length = PW_PANEL_RASTER_SIZE;
  raster = ScratchAlloc(length);
  switch (g_ui.view.presentation.rewardKind) {
  case 3:
    source = image->itemMap;
    EepromRead((u16)source, raster, sizeof(image->itemMap));
    DisplayBlit(0x20, 0x04, 0x20, 0x18, raster);
    RenderMessage(0x20, MESSAGE_SPECIAL_MAP, 0x0d, 0);
    break;
  case 0:
    EepromRead((g_state.uiFrame & 1) * 0xc0 +
                   PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon,
                                            pokemonImage),
               raster, length);
    DisplayBlit(0x20, 0x08, 0x20, 0x18, raster);
    RenderDistributionName(0x00, 0x20, 5);
    break;
  case 2:
    RenderItemCheckTreasure(0x20, 0x04);
    RenderDistributionItem(0x00, 0x20, 0x0d);
    break;
  case 1:
    source = image->itemMap;
    EepromRead((u16)source, raster, sizeof(image->itemMap));
    DisplayBlit(0x20, 0x04, 0x20, 0x18, raster);
    RenderMessage(0x20, MESSAGE_SPECIAL_COURSE, 0x0d, 0);
    break;
  case 4:
    source = image->stamps;
    EepromRead((u16)source, raster, iconLength);
    DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
    RenderMessage(0x20, MESSAGE_STAMP, 0x0d, 0);
    break;
  case 5:
    source = image->stamps + (sizeof(image->stamps) / 4);
    EepromRead((u16)source, raster, iconLength);
    DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
    RenderMessage(0x20, MESSAGE_STAMP, 0x0d, 0);
    break;
  case 6:
    source = image->stamps + (sizeof(image->stamps) / 4) * 2;
    EepromRead((u16)source, raster, iconLength);
    DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
    RenderMessage(0x20, MESSAGE_STAMP, 0x0d, 0);
    break;
  case 7:
    source = image->stamps + (sizeof(image->stamps) / 4) * 3;
    EepromRead((u16)source, raster, iconLength);
    DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
    RenderMessage(0x20, MESSAGE_STAMP, 0x0d, 0);
    break;
  }

  RenderMessage(0x30, MESSAGE_POKEMON_GIVEN, 0x0e, 0);
  if (g_ui.view.presentation.frame < 0x10) {
    g_ui.view.presentation.frame++;
  }
}

void WalkStartUpdate(void)
{
  u8 frame;

  frame = g_ui.view.presentation.frame;
  switch (g_ui.view.presentation.stage) {
  case 0:
    if (frame > 4) {
      g_ui.view.presentation.stage = 1;
      g_ui.view.presentation.frame = 0;
    }
    break;
  case 1:
    if (frame == 0) {
      break;
    }
    g_ui.view.presentation.stage = 2;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(0);
    break;
  case 2:
    if (frame <= 8) {
      break;
    }
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.stage = 3;
    BeepLoadScore(6);
    break;
  }
}

void WalkStartRender(void)
{
  switch (g_ui.view.presentation.stage) {
  case 0:
    BallDrop();
    break;
  case 1:
    ArrivalCloud();
    break;
  case 2:
    ArrivalPokemon();
    break;
  case 3:
    ArrivalComplete();
    break;
  default:
    break;
  }
  RenderBattery(0, 0);
}

/* Event-distribution result commit. */
void EventRewardUpdate(void)
{
  u8 frame;
  u16 length;
  Course *course;
  u8 *record;
  u8 encounter;
  EventItem *item;

  frame = g_ui.view.presentation.frame;
  switch (g_ui.view.presentation.stage) {
  case 0:
    if (frame <= 4) {
      break;
    }
    g_ui.view.presentation.stage = 4;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(0);
    break;
  case 1:
    if (frame == 0) {
      break;
    }
    g_ui.view.presentation.stage = 3;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(6);
    break;
  case 3:
    if (BeepHasScore() != 0) {
      break;
    }
    if (g_ui.view.presentation.frame <= 8) {
      break;
    }
    length = PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH;
    switch (g_ui.view.presentation.rewardKind) {
    case 0:
      ScratchReset();
      course = ScratchAlloc(length);
      EepromRead(PW_EEPROM_ACTIVE_COURSE_VIEW_BASE, course, length);
      record = ScratchAlloc(sizeof(Pokemon));
      EepromRead(
          PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, pokemon),
          record, sizeof(Pokemon));
      encounter = 4;
      DiaryAppend(course, ScratchAlloc(sizeof(DiaryEntry)), 0x1d,
                  g_state.save.bonusCourse, encounter, 0);
      break;
    case 2:
      ScratchReset();
      course = ScratchAlloc(length);
      EepromRead(PW_EEPROM_ACTIVE_COURSE_VIEW_BASE, course, length);
      item = ScratchAlloc(sizeof(EventItem));
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_ITEM, EventItem, header),
                 item, sizeof(EventItem));
      encounter = 0;
      DiaryAppend(course, ScratchAlloc(sizeof(DiaryEntry)), 0x1c,
                  g_state.save.bonusCourse, encounter, item->header.itemIdLe);
      break;
    default:
      break;
    }
    HomeInit();
    SetView(VIEW_HOME);
    break;
  default:
    break;
  }
}

void EventRewardRender(void)
{
  switch (g_ui.view.presentation.stage) {
  case 0:
    BallDrop();
    break;
  case 4:
    BallSparkle();
    break;
  case 1:
    ArrivalCloud();
    break;
  case 3:
    RewardInfo();
    break;
  default:
    break;
  }
  RenderBattery(0, 0);
}

void DeparturePokemon(void)
{
  RenderLargePokemon(0x10, 0x08);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
}

void DepartureCloud(void)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  source = ((UiResources *)EEPROM_UI)->gas;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->gas));
  DisplayBlit(0x20, 0x10, 0x20, 0x18, raster);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.stage = 0;
  g_ui.view.presentation.frame = 0;
}

void DepartureAfterimage(void)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  source = ((UiResources *)EEPROM_UI)->pokemonShadow;
  if (g_ui.view.presentation.frame <= 4) {
    EepromRead((u16)source, raster, sizeof(((UiResources *)0)->pokemonShadow));
    DisplayBlit(0x2c,
                g_departureAfterimageYPositions6[g_ui.view.presentation.frame],
                0x08, 0x08, raster);
  }
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
}

void DepartureComplete(void)
{
  ScratchReset();
  ScratchAlloc(PW_PANEL_RASTER_SIZE);
  RenderHeldName(0x00, 0x20, 5);
  RenderMessage(0x30, MESSAGE_POKEMON_RETURNED, 0x0e, 0);
  if (g_ui.view.presentation.frame < 0x10) {
    g_ui.view.presentation.frame++;
  }
  if ((BeepHasScore() == 0) && (g_ui.view.presentation.frame > 8)) {
    HomeInit();
    SetView(VIEW_HOME);
  }
}

void CommunicationComplete(void)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->walker));
  source = ((UiResources *)EEPROM_UI)->walker;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->walker));
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  RenderMessage(0x30, MESSAGE_COMMUNICATION_COMPLETE, 0x0f, 0);
  if (g_ui.view.presentation.frame < 0x10) {
    g_ui.view.presentation.frame++;
  }
  if ((BeepHasScore() == 0) && (g_ui.view.presentation.frame > 8)) {
    HomeInit();
    SetView(VIEW_HOME);
  }
}

void WalkEndUpdate(void)
{
  u8 frame;

  frame = g_ui.view.presentation.frame;
  switch (g_ui.view.presentation.stage) {
  case 5:
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.stage = 2;
    BeepLoadScore(0);
    break;
  case 2:
    if (frame <= 8) {
      break;
    }
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.stage = 1;
    break;
  case 0:
    if (frame < 9) {
      break;
    }
    g_ui.view.presentation.stage = 3;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(6);
    break;
  case 6:
    g_ui.view.presentation.stage = 7;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(6);
    break;
  default:
    break;
  }
}

void WalkEndRender(void)
{
  switch (g_ui.view.presentation.stage) {
  case 2:
    DeparturePokemon();
    break;
  case 1:
    DepartureCloud();
    break;
  case 0:
    DepartureAfterimage();
    break;
  case 3:
    DepartureComplete();
    break;
  case 6:
  case 7:
    CommunicationComplete();
    break;
  default:
    break;
  }
  RenderBattery(0, 0);
}

/* ball drop y per frame */
const s8 g_ballDropYPositions6[6] = {0, 2, 6, 12, 20, 16};

/* arrival sparkle x,y */
const SparkleXY g_arrivalSparkleCoordinates3[3] = {{36, 22}, {52, 16}, {42, 8}};

/* departure afterimage y per frame */
const s8 g_departureAfterimageYPositions6[6] = {20, 18, 14, 8, 0, 0};
