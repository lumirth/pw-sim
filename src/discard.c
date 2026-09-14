#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "display.h"
#include "diary.h"
#include "beep.h"
#include "discard.h"
#include "eeprom.h"
#include "home.h"
#include "pad.h"
#include "common.h"
#include "scratch.h"

void DiscardInit(void)
{
  g_ui.view.discard.selectedSlot = 1;
}

#define PW_EEPROM_POKEMON_SLOT_BYTES 0x30
#define PW_EEPROM_ITEM_SLOT_BYTES 0x0c
#define PW_EEPROM_ENCOUNTER_RECORD_BYTES 0x10
#define PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH 0xbe
#define PW_DIARY_RECORD_SIZE 0x88
#define PW_DIARY_ACTION_CAPTURED_ROUTE_POKEMON 0x0d
#define PW_DIARY_ACTION_GOT_ITEM 0x0b
#define PW_DISCARD_PICKER_LAST_SLOT 2

/* Copy the course encounter into the selected Pokemon slot, persist the
 * inventory, and append diary action 0x0d. */
void ReplacePokemon(void)
{
  u8 *records;
  u8 *course;

  if (g_ui.view.discard.sourceIndex == 0) {
    return;
  }
  ScratchReset();
  records = ScratchAlloc(PW_EEPROM_POKEMON_SLOT_BYTES);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon), records,
             PW_EEPROM_POKEMON_SLOT_BYTES);
  EepromRead(
      ((g_ui.view.discard.sourceIndex - 1) * PW_EEPROM_ENCOUNTER_RECORD_BYTES) +
          PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, encounters[0]),
      records + ((s8)g_ui.view.discard.selectedSlot *
                 PW_EEPROM_ENCOUNTER_RECORD_BYTES),
      PW_EEPROM_ENCOUNTER_RECORD_BYTES);
  EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon), records,
              PW_EEPROM_POKEMON_SLOT_BYTES);
  course = ScratchAlloc(PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
  EepromRead(EEPROM_COURSE, course, PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
  DiaryAppend((Course *)course, ScratchAlloc(PW_DIARY_RECORD_SIZE),
              PW_DIARY_ACTION_CAPTURED_ROUTE_POKEMON, 0,
              g_ui.view.discard.sourceIndex, 0);
}

/* Copy the course item number into the selected item slot and append diary
 * action 0x0b. */
void ReplaceItem(void)
{
  u8 *records;
  u8 *course;

  ScratchReset();
  records = ScratchAlloc(PW_EEPROM_ITEM_SLOT_BYTES);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), records,
             PW_EEPROM_ITEM_SLOT_BYTES);
  EepromRead((g_ui.view.discard.sourceIndex * 2) +
                 PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, itemId[0]),
             records + ((s8)g_ui.view.discard.selectedSlot * 4), 2);
  EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), records,
              PW_EEPROM_ITEM_SLOT_BYTES);
  course = ScratchAlloc(PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
  EepromRead(EEPROM_COURSE, course, PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
  DiaryAppend((Course *)course, ScratchAlloc(PW_DIARY_RECORD_SIZE),
              PW_DIARY_ACTION_GOT_ITEM, 0, 0,
              ((Course *)course)->itemId[g_ui.view.discard.sourceIndex]);
}

void DiscardUpdate(void)
{
  if (InputPressed(BUTTON_LEFT) != 0) {
    if (g_ui.view.discard.selectedSlot == 0) {
      HomeInit();
      SetView(VIEW_HOME);
      BeepLoadScore(1);
      return;
    }
    g_ui.view.discard.selectedSlot--;
    BeepLoadScore(2);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    if (g_ui.view.discard.selectedSlot == PW_DISCARD_PICKER_LAST_SLOT) {
      BeepLoadScore(1);
      return;
    }
    g_ui.view.discard.selectedSlot++;
    BeepLoadScore(2);
  }
  if (InputPressed(BUTTON_CENTER) != 0) {
    if (g_ui.view.discard.inventoryKind == PW_DISCARD_INVENTORY_POKEMON) {
      ReplacePokemon();
    } else {
      ReplaceItem();
    }
    HomeInit();
    SetView(VIEW_HOME);
    BeepLoadScore(0);
  }
}

void SelectedPokemon(void)
{
  u8 *records;
  u8 i;

  ScratchReset();
  records = ScratchAlloc(0x40);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, encounters[0]),
             records, PW_EEPROM_POKEMON_SLOT_BYTES);
  EepromRead(
      (((s8)g_ui.view.discard.selectedSlot * PW_EEPROM_ENCOUNTER_RECORD_BYTES) +
       PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon)),
      records + PW_EEPROM_POKEMON_SLOT_BYTES, PW_EEPROM_ENCOUNTER_RECORD_BYTES);
  i = 0;
  do {
    if (*(u16 *)(records + PW_EEPROM_POKEMON_SLOT_BYTES) ==
        *(u16 *)(records + i * PW_EEPROM_ENCOUNTER_RECORD_BYTES)) {
      RenderEnemyName(0, 0x30, i, 7);
      break;
    }
    i++;
  } while (i < 3);
}

void SelectedItem(void)
{
  u8 *records;
  Item selected;
  u8 i;

  ScratchReset();
  records = ScratchAlloc(0x14);
  EepromRead((((s8)g_ui.view.discard.selectedSlot * 4) +
              PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items)),
             &selected, 4);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, itemId[0]),
             records, 0x14);
  i = 0;
  do {
    if (selected.id == *(u16 *)(records + i * 2)) {
      RenderCourseItem(0, 0x30, i, 0x0f);
      break;
    }
    i++;
  } while (i < 10);
}

void DiscardRender(void)
{
  u8 *raster;
  UiResources *image;

  ScratchReset();
  raster = ScratchAlloc(0x180);
  image = (UiResources *)EEPROM_UI;
  EepromRead((u16)(image->largeArrows + 0x40), raster, 0x20);
  DisplayBlit(0, 0, 8, 0x10, raster);
  EepromRead((u16)(image->messages + 68 * 384), raster, 0x140);
  DisplayBlit(8, 0, 0x50, 0x10, raster);
  EepromRead((u16)(image->arrows + 0x30), raster, 0x20);
  DisplayBlit((g_ui.view.discard.selectedSlot * 0x14 + 0x18), 0x18, 8, 8,
              raster + (g_state.uiFrame & 1) * 0x10);
  if (g_ui.view.discard.inventoryKind == 0) {
    EepromRead((u16)image->ball, raster, 0x10);
  } else {
    EepromRead((u16)image->treasure, raster, 0x10);
  }
  DisplayBlit(0x18, 0x20, 8, 8, raster);
  DisplayBlit(0x2c, 0x20, 8, 8, raster);
  DisplayBlit(0x40, 0x20, 8, 8, raster);
  if (((s8)g_ui.view.discard.selectedSlot >= 0) &&
      ((s8)g_ui.view.discard.selectedSlot <= PW_DISCARD_PICKER_LAST_SLOT)) {
    if (g_ui.view.discard.inventoryKind == 0) {
      SelectedPokemon();
    } else {
      SelectedItem();
    }
    RenderBattery(0x58, 0);
  }
}
