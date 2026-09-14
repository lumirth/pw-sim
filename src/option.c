#include "types.h"
#include "project.h"
#include "display.h"
#include "beep.h"
#include "dowsing.h"
#include "eeprom.h"
#include "home.h"
#include "home_setting.h"
#include "list.h"
#include "option.h"
#include "pad.h"
#include "common.h"
#include "poketrace.h"
#include "scratch.h"
#include "viewer.h"

extern const u8 g_mainMenuWattCosts[6];
extern const u8 g_menuEntryRasterBitOffsets[6];

#define PW_MENU_SELECTION_LAST 5
#define PW_MENU_ENTRY_COUNT 6
#define PW_BEEP_ERROR 4
#define PW_BEEP_CONFIRM 0
#define PW_BEEP_MOVE 2
#define PW_BEEP_BACK 1
#define PW_EEPROM_SAVE_DATA_LENGTH 0x18

void MenuReset(void)
{
  g_ui.view.menu.error = 0;
}

/* Main-menu confirm spends watts and dispatches the six entries; PREV/NEXT wrap
 * the cursor through a signed remainder. */
void MainMenuUpdate(void)
{
  u16 watt;
  u16 presence[2];
  u8 *costs;
  s16 index;

  if (g_ui.view.menu.error != 0) {
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    g_ui.view.menu.error = 0;
    BeepLoadScore(PW_BEEP_CONFIRM);
    return;
  }

  if (InputPressed(BUTTON_CENTER) != 0) {
    watt = g_state.save.watts;
    costs = (u8 *)g_mainMenuWattCosts;
    if (watt < costs[g_state.menuSelection]) {
      g_ui.view.menu.error = 1;
      BeepLoadScore(PW_BEEP_ERROR);
      return;
    }
    switch (g_state.menuSelection) {
    case 0:
      if (g_state.flags.bits.hasPokemon == 0) {
        g_ui.view.menu.error = 2;
        BeepLoadScore(PW_BEEP_ERROR);
        return;
      }
      if (watt < costs[g_state.menuSelection]) {
        g_state.save.watts = 0;
      } else {
        g_state.save.watts =
            (g_state.save.watts - costs[g_state.menuSelection]);
      }
      EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                        (u8 *)&g_state.save, PW_EEPROM_SAVE_DATA_LENGTH);
      SetView(VIEW_RADAR);
      RadarInit();
      BeepLoadScore(PW_BEEP_CONFIRM);
      return;
    case 1:
      if (g_state.save.watts < costs[g_state.menuSelection]) {
        g_state.save.watts = 0;
      } else {
        g_state.save.watts =
            (g_state.save.watts - costs[g_state.menuSelection]);
      }
      EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                        (u8 *)&g_state.save, PW_EEPROM_SAVE_DATA_LENGTH);
      DowsingInit();
      SetView(VIEW_DOWSING);
      BeepLoadScore(PW_BEEP_CONFIRM);
      return;
    case 2:
      TryBeginIr();
      return;
    case 3:
      SetView(VIEW_TRAINER);
      TrainerInit();
      BeepLoadScore(PW_BEEP_CONFIRM);
      return;
    case 4:
      InventoryMasks((u8 *)presence);
      if (presence[0] != 0) {
        g_ui.view.inventory.pokemonPresent = presence[0];
        g_ui.view.inventory.itemPresent = presence[1];
        PokemonSelectFirst();
        SetView(VIEW_POKEMON_LIST);
        BeepLoadScore(PW_BEEP_CONFIRM);
        return;
      }
      if (presence[1] != 0) {
        g_ui.view.inventory.pokemonPresent = presence[0];
        g_ui.view.inventory.itemPresent = presence[1];
        ItemSelectFirst();
        SetView(VIEW_ITEM_LIST);
        BeepLoadScore(PW_BEEP_CONFIRM);
        return;
      }
      g_ui.view.menu.error = 3;
      BeepLoadScore(PW_BEEP_ERROR);
      return;
    case 5:
      SettingsInit();
      SetView(VIEW_SETTINGS);
      BeepLoadScore(PW_BEEP_CONFIRM);
      return;
    }
  }

  if (InputPressed(BUTTON_LEFT) != 0) {
    if (g_state.menuSelection == 0) {
      HomeInit();
      SetView(VIEW_HOME);
      BeepLoadScore(PW_BEEP_BACK);
      return;
    }
    index = g_state.menuSelection;
    index = (index + 5) % PW_MENU_ENTRY_COUNT;
    g_state.menuSelection = index;
    BeepLoadScore(PW_BEEP_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    if (g_state.menuSelection == PW_MENU_SELECTION_LAST) {
      HomeInit();
      SetView(VIEW_HOME);
      BeepLoadScore(PW_BEEP_BACK);
      return;
    }
    index = g_state.menuSelection;
    index = (index + 1) % PW_MENU_ENTRY_COUNT;
    g_state.menuSelection = index;
    BeepLoadScore(PW_BEEP_MOVE);
  }
}

/* Compose the six-entry menu: selected-row label, per-entry icons, watt
 * readout, and the overlay messages for the three error states. */
void MainMenuRender(void)
{
  UiResources *image;
  u8 *raster;
  u8 *iconBits;
  u8 *source;
  u16 length;
  u16 iconLength;
  s16 i;

  image = (UiResources *)EEPROM_UI;
  iconLength = (sizeof(image->menuIcons) / 6);
  ScratchReset();
  length = (sizeof(image->menuLabels) / 6);
  raster = ScratchAlloc(length);
  iconBits = ScratchAlloc(0x80);
  source = image->menuLabels + g_state.menuSelection * length;
  EepromRead((u16)source, raster, length);
  DisplayBlit(8, 0, 0x50, 0x10, raster);

  i = 0;
  do {
    u16 j;
    j = 0;
    do {
      iconBits[j] = 0;
      j++;
    } while (j < 0x80);
    if (i == g_state.menuSelection) {
      source = image->arrows + ((g_state.uiFrame & 1) + 3) * 0x10;
      EepromRead((u16)source, raster, 0x10);
      RasterOr(8, 8, raster, 4, (g_menuEntryRasterBitOffsets[i] - 8), iconBits,
               0x10, 0x20);
    }
    source = image->menuIcons + i * iconLength;
    EepromRead((u16)source, raster, iconLength);
    RasterOr(0x10, 0x10, raster, 0, g_menuEntryRasterBitOffsets[i], iconBits,
             0x10, 0x20);
    DisplayBlit((i * 0x10), 0x10, 0x10, 0x20, iconBits);
    i++;
  } while (i < PW_MENU_ENTRY_COUNT);

  switch (g_ui.view.menu.error) {
  case 0:
    RenderDecimal(0x48, 0x30, g_state.save.watts, 0);
    switch (g_state.menuSelection) {
    case 0:
      RenderDecimal(8, 0x30, 10ul, 0);
      break;
    case 1:
      RenderDecimal(8, 0x30, 3ul, 0);
      break;
    }
    source = image->watts;
    EepromRead((u16)source, raster, iconLength);
    DisplayBlit(0x50, 0x30, 0x10, 0x10, raster);
    if (g_state.menuSelection < 2) {
      DisplayBlit(0x18, 0x30, 0x10, 0x10, raster);
      source = image->digits + 12 * 0x20;
      EepromRead((u16)source, raster, 0x20);
      DisplayBlit(0x28, 0x30, 8, 0x10, raster);
    }
    break;
  case 1:
    RenderMessage(0x30, MESSAGE_NOT_ENOUGH_WATTS, 0x0f, 1);
    break;
  case 2:
    RenderMessage(0x30, MESSAGE_NO_POKEMON, 0x0f, 1);
    break;
  case 3:
    RenderMessage(0x30, MESSAGE_NO_ITEMS, 0x0f, 1);
    break;
  }

  source = image->largeArrows;
  EepromRead((u16)source, raster, iconLength);
  DisplayBlit(0, 0, 8, 0x10, raster);
  DisplayBlit(0x58, 0, 8, 0x10, raster + 0x20);
  RenderBattery(0x58, 0);
}

/* Watt cost per main menu entry */
const u8 g_mainMenuWattCosts[6] = {0x0a, 0x03, 0x00, 0x00, 0x00, 0x00};

/* menu entry raster bit offsets */
const u8 g_menuEntryRasterBitOffsets[6] = {0x08, 0x0b, 0x0d, 0x0d, 0x0b, 0x08};
