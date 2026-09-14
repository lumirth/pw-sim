#include "types.h"
#include "project.h"
#include "display.h"
#include "beep.h"
#include "eeprom.h"
#include "home.h"
#include "home_setting.h"
#include "option.h"
#include "pad.h"
#include "common.h"
#include "scratch.h"

/* Settings view of the shared UI state. */

#define PW_BEEP_SEQUENCE_CONFIRM 0
#define PW_BEEP_SEQUENCE_BACK 1
#define PW_BEEP_SEQUENCE_MOVE 2
#define PW_SAVE_VOLUME_COUNT 3
#define PW_SETTINGS_LIST_INDEX 5

void SettingsInit(void)
{
  g_ui.view.settings.page = 0;
  g_ui.view.settings.cursor = 0;
}

/* LEFT on page 0 returns to the main menu; LEFT on page 1 goes back to page 0.
 * RIGHT on page 0 advances to page 1. */
void SettingsNavigate(void)
{
  if (InputPressed(BUTTON_LEFT) != 0) {
    if (g_ui.view.settings.cursor == 0) {
      BeepLoadScore(PW_BEEP_SEQUENCE_BACK);
      MenuReset();
      SetView(VIEW_MAIN_MENU);
      return;
    }
    g_ui.view.settings.cursor = 0;
    BeepLoadScore(PW_BEEP_SEQUENCE_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) == 0) {
    return;
  }
  if (g_ui.view.settings.cursor == 1) {
    return;
  }
  g_ui.view.settings.cursor = 1;
  BeepLoadScore(PW_BEEP_SEQUENCE_MOVE);
}

/* LEFT or RIGHT cycles save.volume through 0,1,2 with a signed remainder, then
 * applies the beeper output mode and a move beep. */
void SettingsVolume(void)
{
  s16 volume;

  if (InputPressed(BUTTON_LEFT) != 0) {
    volume = g_state.save.volume;
    volume = (volume + 2) % PW_SAVE_VOLUME_COUNT;
    g_state.save.volume = volume;
    BeepSetOutputMode(g_state.save.volume);
    BeepLoadScore(PW_BEEP_SEQUENCE_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    volume = g_state.save.volume;
    volume = (volume + 1) % PW_SAVE_VOLUME_COUNT;
    g_state.save.volume = volume;
    BeepSetOutputMode(g_state.save.volume);
    BeepLoadScore(PW_BEEP_SEQUENCE_MOVE);
  }
}

/* LEFT decrements contrast while it is nonzero; RIGHT increments while it is
 * below 9. Both paths apply the LCD contrast even when the value did not
 * change. */
void SettingsContrast(void)
{
  u8 raw;

  if (InputPressed(BUTTON_LEFT) != 0) {
    raw = ((const u8 *)&g_state.save)[SAVE_SETTINGS_OFFSET];
    if ((raw & 0x78) != 0) {
      g_state.save.contrast--;
      BeepLoadScore(PW_BEEP_SEQUENCE_MOVE);
    }
    DisplaySetContrast(g_state.save.contrast);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    raw = ((const u8 *)&g_state.save)[SAVE_SETTINGS_OFFSET];
    if ((raw & 0x78) < 0x48) {
      g_state.save.contrast++;
      BeepLoadScore(PW_BEEP_SEQUENCE_MOVE);
    }
    DisplaySetContrast(g_state.save.contrast);
  }
}

/* bank[0] selects page-nav / volume / contrast. HOME on the chooser enters the
 * highlighted sub-page; HOME on a sub-page commits save data and returns to the
 * home view. */
void SettingsUpdate(void)
{
  u8 page;

  page = g_ui.view.settings.page;
  switch (page) {
  case 0:
    SettingsNavigate();
    break;
  case 1:
    SettingsVolume();
    break;
  case 2:
    SettingsContrast();
    break;
  }

  if (InputPressed(BUTTON_CENTER) == 0) {
    return;
  }
  if (g_ui.view.settings.page == 0) {
    BeepLoadScore(PW_BEEP_SEQUENCE_CONFIRM);
    g_ui.view.settings.page = (g_ui.view.settings.cursor + 1);
  } else {
    BeepLoadScore(PW_BEEP_SEQUENCE_CONFIRM);
    HomeInit();
    SetView(VIEW_HOME);
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
  }
}

/* Settings composition: the settings list label, volume and contrast names,
 * then a page-specific body (chooser cursor, SE-on graphics, or contrast
 * meter) and a shared arrow / battery tail. */
void SettingsRender(void)
{
  UiResources *image;
  u8 *raster;
  u8 *source;
  u8 cursorX;
  s16 i;

  image = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(sizeof(image->menuLabels) / 6);
  source = &image->menuLabels[PW_SETTINGS_LIST_INDEX *
                              (sizeof(image->menuLabels) / 6)];
  EepromRead((u16)source, raster, sizeof(image->menuLabels) / 6);
  DisplayBlit(8, 0, 80, 16, raster);
  source = image->volume;
  EepromRead((u16)source, raster, sizeof(image->volume));
  DisplayBlit(8, 16, 40, 16, raster);
  source = image->contrast;
  EepromRead((u16)source, raster, sizeof(image->contrast));
  DisplayBlit(56, 16, 40, 16, raster);
  source = image->arrows;
  EepromRead((u16)source, raster, sizeof(image->arrows));
  cursorX = (g_ui.view.settings.cursor * 48);

  switch (g_ui.view.settings.page) {
  case 0:
    DisplayBlit(cursorX, 20, 8, 8, raster + ((g_state.uiFrame & 1) + 9) * 16);
    break;
  case 1:
    DisplayBlit(cursorX, 20, 8, 8, raster + 11 * 16);
    DisplayBlit((g_state.save.volume * 32), 44, 8, 8,
                raster + ((g_state.uiFrame & 1) + 9) * 16);
    source = image->volumeOff;
    EepromRead((u16)source, raster,
               sizeof(image->volumeOff) + sizeof(image->volumeLow) +
                   sizeof(image->volumeHigh));
    DisplayBlit(8, 40, 24, 16, raster);
    DisplayBlit(40, 40, 24, 16, raster + sizeof(image->volumeOff));
    DisplayBlit(72, 40, 24, 16,
                raster + sizeof(image->volumeOff) + sizeof(image->volumeLow));
    break;
  case 2:
    DisplayBlit((g_state.save.contrast * 8 + 8), 32, 8, 8,
                raster + ((g_state.uiFrame & 1) + 3) * 16);
    DisplayBlit((g_ui.view.settings.cursor * 48), 20, 8, 8, raster + 11 * 16);
    source = image->meter;
    EepromRead((u16)source, raster, sizeof(image->meter));
    for (i = 0; i < 10; i++) {
      DisplayBlit((i * 8 + 8), 40, 8, 16, raster);
    }
    break;
  }

  source = image->largeArrows + sizeof(image->largeArrows) / 2;
  EepromRead((u16)source, raster, sizeof(image->largeArrows) / 4);
  DisplayBlit(0, 0, 8, 16, raster);
  RenderBattery(88, 0);
}
