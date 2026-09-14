#include "types.h"
#include "eeprom_address.h"
#include <stddef.h>
#include "project.h"
#include "diary.h"
#include "display.h"
#include <machine.h>
#include "beep.h"
#include "discard.h"
#include "dowsing.h"
#include "eeprom.h"
#include "home.h"
#include "pad.h"
#include "common.h"
#include "scratch.h"

extern const s8 g_dowsingGrassVerticalShiftS84[];

void DistributeItemRead(u8 *destination);
void DistributeItemWrite(u8 *source);
void RollItem(void);

#define PW_EEPROM_DISTRIBUTE_ITEM_LENGTH sizeof(EventItem)
#define PW_EEPROM_ACTIVE_COURSE_VIEW_BASE EEPROM_COURSE
#define PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH sizeof(Course)
#define PW_EEPROM_ITEM_SLOTS                                                   \
  (PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items))
#define PW_EEPROM_DAILY_EVENT_FLAGS EEPROM_EVENTS
#define PW_DIARY_RECORD_SIZE 0x88
#define PW_DIARY_ACTION_DOWSING_ITEM 0x0b
#define PW_DIARY_ACTION_BONUS_COURSE_ITEM 0x0c
#define PW_DOWSING_SLOT_COUNT 6

/* Cursor, hidden slot, and attempt count. */
void DowsingInit(void)
{
  g_ui.view.dowsing.substate = 0;
  g_ui.view.dowsing.cursor = 0;
  g_ui.view.dowsing.attempts = 2;
  g_ui.view.dowsing.hiddenSlot =
      ((u8)(((u16)((u16)RandomNext() << 3)) >> 8) % PW_DOWSING_SLOT_COUNT);
  g_ui.view.dowsing.revealedSlot = 0xff;
  g_ui.view.dowsing.watts = 0;
}

/* Dowsing input and result substates. */
void DowsingUpdate(void)
{
  switch (g_ui.view.dowsing.substate) {
  case 0:
    if (InputPressed(BUTTON_CENTER) != 0) {
      if (g_ui.view.dowsing.cursor == g_ui.view.dowsing.revealedSlot) {
        BeepLoadScore(1);
        return;
      }
      BeepLoadScore(0);
      g_ui.view.dowsing.substate = 1;
      g_ui.view.dowsing.animationFrame = 4;
      return;
    }
    if (InputPressed(BUTTON_LEFT) != 0) {
      g_ui.view.dowsing.cursor =
          ((g_ui.view.dowsing.cursor + 5) % PW_DOWSING_SLOT_COUNT);
      BeepLoadScore(2);
    }
    if (InputPressed(BUTTON_RIGHT) != 0) {
      g_ui.view.dowsing.cursor =
          ((g_ui.view.dowsing.cursor + 1) % PW_DOWSING_SLOT_COUNT);
      BeepLoadScore(2);
    }
    return;
  case 1:
    if (g_ui.view.dowsing.animationFrame != 0) {
      return;
    }
    if (BeepHasScore() != 0) {
      return;
    }
    if (g_ui.view.dowsing.cursor == g_ui.view.dowsing.hiddenSlot) {
      g_ui.view.dowsing.substate = 2;
      RollItem();
      BeepLoadScore(5);
      return;
    }
    g_ui.view.dowsing.substate = 3;
    BeepLoadScore(4);
    if (g_ui.view.dowsing.attempts == 2) {
      g_ui.view.dowsing.revealedSlot = g_ui.view.dowsing.cursor;
    }
    g_ui.view.dowsing.attempts--;
    return;
  case 2:
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    if (g_state.save.bonusCourse == 0) {
      u8 slot;

      slot = g_ui.view.dowsing.emptyItemSlot;
      if (slot == 3) {
        g_ui.view.discard.inventoryKind = PW_DISCARD_INVENTORY_ITEM;
        g_ui.view.discard.sourceIndex = g_ui.view.dowsing.rewardIndex;
        DiscardInit();
        SetView(VIEW_DISCARD);
        return;
      }
      EepromWrite((slot * 4 + PW_EEPROM_ITEM_SLOTS),
                  &g_ui.view.dowsing.itemNumber, 2);
      if (g_state.flags.bits.hasPokemon != 0) {
        u8 *course;

        course = ScratchAlloc(PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
        EepromRead(PW_EEPROM_ACTIVE_COURSE_VIEW_BASE, course,
                   PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
        DiaryAppend((Course *)course, ScratchAlloc(PW_DIARY_RECORD_SIZE),
                    PW_DIARY_ACTION_DOWSING_ITEM, 0, 0,
                    g_ui.view.dowsing.itemNumber);
      }
    }
    BeepLoadScore(0);
    HomeInit();
    SetView(VIEW_HOME);
    return;
  case 3:
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    if (g_ui.view.dowsing.attempts == 0) {
      BeepLoadScore(0);
      HomeInit();
      SetView(VIEW_HOME);
      return;
    }
    BeepLoadScore(0);
    g_ui.view.dowsing.substate = 4;
    return;
  case 4:
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    BeepLoadScore(0);
    g_ui.view.dowsing.substate = 0;
    return;
  }
}

void DistributeItemRead(u8 *destination)
{
  EventItemHeader *header;

  header = &((EventItem *)EEPROM_EVENT_ITEM)->header;
  EepromRead((u16)header, destination, sizeof(EventItem));
}

void DistributeItemWrite(u8 *source)
{
  EventItemHeader *header;

  header = &((EventItem *)EEPROM_EVENT_ITEM)->header;
  EepromWrite((u16)header, source, sizeof(EventItem));
}

/* Bonus-course distributed item, or watt consolation. */
void TryBonusItem(void)
{
  DeviceStatus *record;
  EventItem *item;
  BonusCourse *event;
  u8 *course;
  uint required;
  u8 encounter;
  u8 flags;

  ScratchReset();
  record = ScratchAlloc(0x68);
  item = ScratchAlloc(sizeof(EventItem));
  event = ScratchAlloc(sizeof(BonusCourse));
  EepromRead(EEPROM_BONUS_COURSE, event, sizeof(BonusCourse));
  required = event->itemStepsLe;
  required = (required >> 8) | (required << 8);
  if (g_state.dailySteps < required) {
    g_ui.view.dowsing.watts = (g_ui.view.dowsing.attempts * 4 + 2);
    WattsAdd(g_ui.view.dowsing.watts);
    return;
  }
  if ((u8)(RandomNext() >> 3) % 100 >= event->itemChance) {
    g_ui.view.dowsing.watts = (g_ui.view.dowsing.attempts * 4 + 2);
    WattsAdd(g_ui.view.dowsing.watts);
    return;
  }
  DistributeItemRead((u8 *)item);
  if (item->header.itemIdLe != 0) {
    g_ui.view.dowsing.watts = (g_ui.view.dowsing.attempts * 4 + 2);
    WattsAdd(g_ui.view.dowsing.watts);
    return;
  }
  if (StatusHasReceived(record, event->itemReceipt) != 0) {
    g_ui.view.dowsing.watts = (g_ui.view.dowsing.attempts * 4 + 2);
    WattsAdd(g_ui.view.dowsing.watts);
    return;
  }
  StatusSetReceived(record, event->itemReceipt);
  course = ScratchAlloc(PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
  EepromRead(PW_EEPROM_ACTIVE_COURSE_VIEW_BASE, course,
             PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
  g_ui.view.dowsing.rewardIndex = 10;
  item->header.prefix.transfer.word = event->itemPrefix.transfer.word;
  item->header.prefix.transfer.halfword = event->itemPrefix.transfer.halfword;
  item->header.itemIdLe = event->itemIdLe;
  EepromRead((u16)((BonusResources *)EEPROM_BONUS_COURSE)->itemName,
             item->itemName, sizeof(item->itemName));
  DistributeItemWrite((u8 *)item);
  flags = (EepromReadByte(PW_EEPROM_DAILY_EVENT_FLAGS) | 0x40);
  EepromWriteByte(PW_EEPROM_DAILY_EVENT_FLAGS, flags);
  if (g_state.flags.bits.hasPokemon == 0) {
    return;
  }
  encounter = 0;
  DiaryAppend((Course *)course, ScratchAlloc(PW_DIARY_RECORD_SIZE),
              PW_DIARY_ACTION_BONUS_COURSE_ITEM, 1, encounter,
              item->header.itemIdLe);
}

/* Pick a course item or the bonus-course path. */
void RollItem(void)
{
  Item *items;
  Course *course;
  u8 index;

  ScratchReset();
  items = ScratchAlloc(sizeof(((WalkData *)0)->items));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), items,
             sizeof(((WalkData *)0)->items));
  g_ui.view.dowsing.emptyItemSlot = ItemSlotFindEmpty(items);
  if (g_state.save.bonusCourse != 0) {
    TryBonusItem();
    return;
  }
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  index = 0;
  do {
    uint slot = index;

    if (g_state.dailySteps >=
        ((u16)((course->itemSteps[slot] >> 8) | (course->itemSteps[slot] << 8)))) {
      if ((u8)(RandomNext() >> 3) % 100 < course->itemChance[slot]) {
        break;
      }
    }
    index++;
  } while (index < 10);
  if (index > 9) {
    index = 9;
  }
  g_ui.view.dowsing.rewardIndex = index;
  g_ui.view.dowsing.itemNumber = course->itemId[index];
}

/* Six grass slots, cursor, and hint bob. */
void DrawGrassField(void)
{
  UiResources *image;
  u8 *raster;
  u16 courseGraphic;
  s16 i;
  u8 x;

  image = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(0x180);
  EepromRead((u16)(image->digits + g_ui.view.dowsing.attempts * 0x20), raster,
             0x20);
  DisplayBlit(0x40, 0x00, 0x08, 0x10, raster);
  EepromRead((u16)image->attemptsLabel, raster, sizeof(image->attemptsLabel));
  DisplayBlit(0x20, 0x00, 0x20, 0x10, raster);
  EepromRead((u16)image->timesLabel, raster, sizeof(image->timesLabel));
  DisplayBlit(0x48, 0x00, 0x18, 0x10, raster);
  if (g_state.save.bonusCourse != 0) {
    courseGraphic = (u16)((BonusResources *)EEPROM_BONUS_COURSE)->course;
  } else {
    courseGraphic = (u16)((CourseResources *)EEPROM_COURSE)->course;
  }
  EepromRead(courseGraphic, raster, 0xc0);
  DisplayBlit(0, 0, 0x20, 0x18, raster);
  if (g_ui.view.dowsing.animationFrame != 0) {
    g_ui.view.dowsing.animationFrame--;
  }
  EepromRead((u16)image->grass, raster,
             sizeof(image->grass) + sizeof(image->litGrass));
  i = 0;
  do {
    x = (i * 0x10);
    if (i == g_ui.view.dowsing.revealedSlot) {
      DisplayBlit(x, 0x18, 0x10, 0x10, raster + sizeof(image->grass));
    } else if (i != g_ui.view.dowsing.cursor) {
      DisplayBlit(x, 0x18, 0x10, 0x10, raster);
    }
    i++;
  } while (i < PW_DOWSING_SLOT_COUNT);
  RasterShift(0x10, 0x10, g_dowsingGrassVerticalShiftS84[g_state.uiFrame & 3],
              raster);
  DisplayBlit((g_ui.view.dowsing.cursor * 0x10), 0x18, 0x10, 0x10, raster);
  RenderMessage(0x30, MESSAGE_DOWSING_SEARCH, 0x0f, 0);
}

/* Field plus find/miss/hint result panels. */
void DowsingRender(void)
{
  UiResources *image;
  u8 *digits;
  u8 *raster;
  u8 *source;
  u8 *courseGraphic;
  s16 i;
  u8 index;
  u8 x;

  image = (UiResources *)EEPROM_UI;
  if (g_ui.view.dowsing.substate == 1) {
    DrawGrassField();
    RenderBattery(0, 0);
    return;
  }

  ScratchReset();
  digits = ScratchAlloc(0x140);
  raster = ScratchAlloc(0x180);
  source = image->digits;
  EepromRead((u16)source, digits, 0x140);
  DisplayBlit(0x40, 0x00, 0x08, 0x10,
              digits + g_ui.view.dowsing.attempts * 0x20);
  source = image->attemptsLabel;
  EepromRead((u16)source, raster, sizeof(image->attemptsLabel));
  DisplayBlit(0x20, 0x00, 0x20, 0x10, raster);
  source = image->timesLabel;
  EepromRead((u16)source, raster, sizeof(image->timesLabel));
  DisplayBlit(0x48, 0x00, 0x18, 0x10, raster);
  if (g_state.save.bonusCourse != 0) {
    courseGraphic = ((BonusResources *)EEPROM_BONUS_COURSE)->course;
  } else {
    courseGraphic = ((CourseResources *)EEPROM_COURSE)->course;
  }
  EepromRead((u16)courseGraphic, raster, 0xc0);
  DisplayBlit(0, 0, 0x20, 0x18, raster);
  if (g_ui.view.dowsing.animationFrame != 0) {
    g_ui.view.dowsing.animationFrame--;
  }
  source = image->grass;
  EepromRead((u16)source, raster,
             sizeof(image->grass) + sizeof(image->litGrass));
  i = 0;
  do {
    if ((g_ui.view.dowsing.substate != 2) || (i != g_ui.view.dowsing.cursor)) {
      x = (i * 0x10);
      if (i == g_ui.view.dowsing.revealedSlot) {
        DisplayBlit(x, 0x18, 0x10, 0x10, raster + sizeof(image->grass));
      } else {
        DisplayBlit(x, 0x18, 0x10, 0x10, raster);
      }
    }
    i++;
  } while (i < PW_DOWSING_SLOT_COUNT);

  switch (g_ui.view.dowsing.substate) {
  case 0:
    source = image->arrows + (g_state.uiFrame & 1) * 0x10;
    EepromRead((u16)source, raster, 0x10);
    DisplayBlit((g_ui.view.dowsing.cursor * 0x10 + 4), 0x28, 0x08, 0x08,
                raster);
    RenderMessage(0x30, MESSAGE_DOWSING_SEARCH, 0x0f, 0);
    break;
  case 2:
    source = image->treasure;
    EepromRead((u16)source, raster, 0x10);
    DisplayBlit((g_ui.view.dowsing.cursor * 0x10 + 4), 0x18, 0x08, 0x08,
                raster);
    if (g_ui.view.dowsing.watts != 0) {
      RenderDecoratedNumber(0x02, 0x20, g_ui.view.dowsing.watts, 0x0d);
      RenderMessage(0x30, MESSAGE_POKEMON_GIVEN, 0x0e, 1);
    } else {
      index = g_ui.view.dowsing.rewardIndex;
      if (index >= 10) {
        RenderDistributionItem(0x00, 0x20, 0x0d);
      } else {
        RenderCourseItem(0x00, 0x20, index, 0x0d);
      }
      RenderMessage(0x30, MESSAGE_DOWSING_FOUND, 0x0e, 1);
    }
    break;
  case 3:
    RenderMessage(0x30, MESSAGE_DOWSING_EMPTY, 0x0f, 1);
    if (g_ui.view.dowsing.attempts == 0) {
      source = image->treasure;
      EepromRead((u16)source, raster, 0x10);
      i = 3;
      do {
        DisplayBlit((g_ui.view.dowsing.hiddenSlot * 0x10 + 4), 0x16, 0x08, 0x08,
                    raster);
        i--;
      } while (i != 0);
    }
    break;
  case 4:
    if (((g_ui.view.dowsing.cursor - g_ui.view.dowsing.hiddenSlot) >= 0
             ? (g_ui.view.dowsing.cursor - g_ui.view.dowsing.hiddenSlot)
             : -(g_ui.view.dowsing.cursor - g_ui.view.dowsing.hiddenSlot)) <
        2) {
      RenderMessage(0x30, MESSAGE_DOWSING_NEAR, 0x0f, 1);
    } else {
      RenderMessage(0x30, MESSAGE_DOWSING_FAR, 0x0f, 1);
    }
    break;
  }
  RenderBattery(0, 0);
}

/* four signed shifts consumed by phase & 3. */
const s8 g_dowsingGrassVerticalShiftS84[4] = {2, -2, 2, -2};
