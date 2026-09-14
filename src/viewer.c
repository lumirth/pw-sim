#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "display.h"
#include "beep.h"
#include "eeprom.h"
#include "home.h"
#include "option.h"
#include "pad.h"
#include "common.h"
#include "scratch.h"
#include "viewer.h"

/* Trainer-card view of the shared UI state. */

void TrainerTime(void);
void TrainerHistory(void);
void TrainerEndingMetrics(void);
void TrainerEndingReward(void);

#define PW_STEP_COUNTER_DISPLAY_MAX 9999999ul
#define PW_TRAINER_CARD_LAST_SUBPAGE 7
#define PW_DECIMAL_GLYPH_BYTES 0x20
#define PW_DECIMAL_GLYPH_ATLAS_BYTES 0xC0

/* Clear the trainer-card page and subpage. */
void TrainerInit(void)
{
  u8 zero;

  zero = 0;
  g_ui.view.trainer.page = zero;
  g_ui.view.trainer.subpage = zero;
}

/* Page/subpage input, ending unlock, and exit. Keep the numeric ending-time
 * base separate from the following event byte. */
void TrainerUpdate(void)
{
  u8 flag;
  u16 endingTime;

  if (g_ui.view.trainer.page == 0) {
    if (InputPressed(BUTTON_LEFT) != 0) {
      if (g_ui.view.trainer.subpage == 0) {
        BeepLoadScore(1);
        MenuReset();
        SetView(VIEW_MAIN_MENU);
        return;
      }
      g_ui.view.trainer.subpage--;
      BeepLoadScore(2);
    }
    if ((InputPressed(BUTTON_RIGHT) != 0) &&
        (g_ui.view.trainer.subpage < PW_TRAINER_CARD_LAST_SUBPAGE)) {
      g_ui.view.trainer.subpage++;
      BeepLoadScore(2);
    }
  }

  if (InputPressed(BUTTON_CENTER) == 0) {
    return;
  }

  switch (g_ui.view.trainer.page) {
  case 0:
    if (g_state.save.totalSteps < PW_STEP_COUNTER_DISPLAY_MAX) {
      BeepLoadScore(0);
      HomeInit();
      SetView(VIEW_HOME);
      return;
    }
    g_ui.view.trainer.page = 1;
    BeepLoadScore(0);
    return;
  case 1:
    endingTime = PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, endingTime);
    flag = EepromReadByte((endingTime + sizeof(((WalkData *)0)->endingTime)));
    if ((flag & 1) == 0) {
      EepromWriteByte((endingTime + sizeof(((WalkData *)0)->endingTime)),
                      (flag | 1));
      g_ui.view.trainer.page = 2;
      BeepLoadScore(7);
      return;
    }
    BeepLoadScore(0);
    HomeInit();
    SetView(VIEW_HOME);
    return;
  case 2:
    BeepLoadScore(0);
    HomeInit();
    SetView(VIEW_HOME);
    return;
  }
}

/* Draw the current packed-BCD clock. */
void TrainerTime(void)
{
  UiResources *image;
  u8 *raster;
  u8 *source;
  u16 length;
  u16 iconLength;

  image = (UiResources *)EEPROM_UI;
  iconLength = sizeof(image->nameIcon);
  ScratchReset();
  length = sizeof(image->name);
  raster = ScratchAlloc(length);
  source = image->menuLabels + 3 * (sizeof(image->menuLabels) / 6);
  EepromRead((u16)source, raster, length);
  DisplayBlit(8, 0, 0x50, 0x10, raster);
  source = image->nameIcon;
  EepromRead((u16)source, raster, iconLength);
  DisplayBlit(0x00, 0x10, 0x10, 0x10, raster);
  source = image->name;
  EepromRead((u16)source, raster, length);
  DisplayBlit(0x10, 0x10, 0x50, 0x10, raster);
  source = image->course;
  EepromRead((u16)source, raster, iconLength);
  DisplayBlit(0x00, 0x20, 0x10, 0x10, raster);
  if (g_state.save.bonusCourse) {
    source = ((BonusResources *)EEPROM_BONUS_COURSE)->courseName;
  } else {
    source = ((CourseResources *)EEPROM_COURSE)->courseName;
  }
  EepromRead((u16)source, raster, length);
  DisplayBlit(0x10, 0x20, 0x50, 0x10, raster);
  source = image->largeArrows;
  EepromRead((u16)source, raster, PW_DECIMAL_GLYPH_ATLAS_BYTES);
  DisplayBlit(0, 0, 0x08, 0x10, raster + sizeof(image->nameIcon));
  DisplayBlit(0x58, 0x00, 0x08, 0x10, raster + 0x20);
  source = image->amPm;
  EepromRead((u16)source, raster, 0x80);
  DisplayBlit(0x00, 0x30, 0x20, 0x10, raster);
  source = image->digits;
  EepromRead((u16)source, raster, length);
  DisplayBlit(0x20, 0x30, 0x08, 0x10,
              raster + ((g_state.time.hourBcd24h >> 4) & 7) *
                           PW_DECIMAL_GLYPH_BYTES);
  DisplayBlit(0x28, 0x30, 0x08, 0x10,
              raster +
                  (g_state.time.hourBcd24h & 0x0f) * PW_DECIMAL_GLYPH_BYTES);
  DisplayBlit(0x38, 0x30, 0x08, 0x10,
              raster +
                  ((g_state.time.minuteBcd >> 4) & 7) * PW_DECIMAL_GLYPH_BYTES);
  DisplayBlit(0x40, 0x30, 0x08, 0x10,
              raster +
                  (g_state.time.minuteBcd & 0x0f) * PW_DECIMAL_GLYPH_BYTES);
  DisplayBlit(0x50, 0x30, 0x08, 0x10,
              raster +
                  ((g_state.time.secondBcd >> 4) & 7) * PW_DECIMAL_GLYPH_BYTES);
  DisplayBlit(0x58, 0x30, 0x08, 0x10,
              raster +
                  (g_state.time.secondBcd & 0x0f) * PW_DECIMAL_GLYPH_BYTES);
  EepromRead((u16)(image->digits + sizeof(image->name)), raster,
             PW_DECIMAL_GLYPH_BYTES);
  DisplayBlit(0x30, 0x30, 0x08, 0x10, raster);
  DisplayBlit(0x48, 0x30, 0x08, 0x10, raster);
}

/* Draw the selected daily step-history page. */
void TrainerHistory(void)
{
  UiResources *image;
  WalkData *data;
  u8 *raster;
  u8 *source;
  u16 length;
  u32 historySteps;

  length = 0x140;
  image = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(length);
  source = image->largeArrows;
  EepromRead((u16)source, raster, 0xC0);
  DisplayBlit(0, 0, 0x08, 0x10, raster);
  if (g_ui.view.trainer.subpage < PW_TRAINER_CARD_LAST_SUBPAGE) {
    DisplayBlit(0x58, 0x00, 0x08, 0x10, raster + 0x20);
  }
  source = image->elapsedDays;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->elapsedDays));
  DisplayBlit(0x28, 0x00, 0x28, 0x10, raster);
  source = image->totalDays;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->totalDays));
  DisplayBlit(0x00, 0x20, 0x40, 0x10, raster);
  source = image->steps;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->steps));
  DisplayBlit(0x38, 0x10, 0x28, 0x10, raster);
  DisplayBlit(0x38, 0x30, 0x28, 0x10, raster);
  source = image->digits + 11 * PW_DECIMAL_GLYPH_BYTES;
  EepromRead((u16)source, raster, PW_DECIMAL_GLYPH_BYTES);
  DisplayBlit(0x18, 0x00, 0x08, 0x10, raster);
  source = image->digits;
  EepromRead((u16)source, raster, length);
  DisplayBlit(0x20, 0x00, 0x08, 0x10,
              raster + g_ui.view.trainer.subpage * PW_DECIMAL_GLYPH_BYTES);
  data = (WalkData *)EEPROM_WALK;
  EepromRead((u16)&data->dailySteps[g_ui.view.trainer.subpage - 1],
             &historySteps, sizeof(historySteps));
  RenderDecimal(0x30, 0x10, historySteps, 0);
  RenderDecimal(0x58, 0x20, g_state.save.days, 0);
  RenderDecimal(0x30, 0x30, g_state.save.totalSteps, 0);
}

/* Ending metrics page with elapsed-hour total. */
void TrainerEndingMetrics(void)
{
  UiResources *image;
  u8 *raster;
  u16 length;
  u8 column;

  length = 0x140;
  image = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(length);
  EepromRead((u16)image->digits, raster, length);
  column = 0;
  do {
    DisplayBlit((column * 8), 0x08, 0x08, 0x10, raster + 0x120);
    column++;
  } while (column < 7);
  EepromRead((u16)image->steps, raster, sizeof(((UiResources *)0)->steps));
  DisplayBlit(0x38, 0x08, 0x28, 0x10, raster);
  EepromRead((u16)image->ending, raster, sizeof(((UiResources *)0)->ending));
  DisplayBlit(0x38, 0x28, 0x28, 0x10, raster);
  RenderDecimal(0x30, 0x28, g_state.save.elapsedHours, 0);
  RenderMessage(0x18, MESSAGE_ENDING_SECOND, 0x0f, 1);
}

/* Gated reward panel. */
void TrainerEndingReward(void)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->itemTreasure));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, itemTreasure),
             raster, sizeof(((UiResources *)0)->itemTreasure));
  DisplayBlit(0x20, 0x04, 0x20, 0x18, raster);
  RenderMessage(0x20, MESSAGE_ENDING_FIRST, 0x0d, 0);
  RenderMessage(0x30, MESSAGE_POKEMON_GIVEN, 0x0e, 1);
}

/* Select the trainer-card page renderer. */
void TrainerRender(void)
{
  switch (g_ui.view.trainer.page) {
  case 0:
    if (g_ui.view.trainer.subpage == 0) {
      TrainerTime();
    } else {
      TrainerHistory();
    }
    break;
  case 1:
    TrainerEndingMetrics();
    break;
  case 2:
    TrainerEndingReward();
    break;
  }
  RenderBattery(0x58, 0x00);
}
