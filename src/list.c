#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "display.h"
#include "beep.h"
#include "eeprom.h"
#include "home.h"
#include "list.h"
#include "option.h"
#include "pad.h"
#include "common.h"
#include "scratch.h"

u8 InventoryNext(uint selectionMask);
void InventoryNextWrap(uint selectionMask);
u8 InventoryPrevious(uint selectionMask);
void InventoryPreviousWrap(uint selectionMask);

#define PW_MENU_SLOT_COUNT 10
#define PW_MENU_LAST_INDEX 9

/* Pokémon and item occupancy masks. */
void InventoryMasks(u8 *presenceMasksOut)
{
  Pokemon *pokemon;
  Item *item;
  u8 i;
  u8 extra;

  ScratchReset();
  pokemon = ScratchAlloc((sizeof(Pokemon) * 3));
  item = ScratchAlloc((sizeof(Item) * 13));

  i = 0;
  do {
    *(u16 *)(presenceMasksOut + i * 2) = 0;
    i++;
  } while (i < 2);

  if (g_state.flags.bits.hasPokemon) {
    *(u16 *)presenceMasksOut |= 1;
  }

  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon), pokemon,
             (sizeof(Pokemon) * 3));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), item,
             (sizeof(Item) * 13));

  i = 0;
  do {
    if (pokemon[i].id != 0) {
      *(u16 *)presenceMasksOut |= (2 << i);
    }
    i++;
  } while (i < 3);

  i = 0;
  do {
    if (item[i].id != 0) {
      *(u16 *)presenceMasksOut |= (0x40 << i);
    }
    i++;
  } while (i < 3);

  item += 3;
  i = 0;
  do {
    if (item[i].id != 0) {
      *(u16 *)(presenceMasksOut + 2) |= (1 << i);
    }
    i++;
  } while (i < 10);

  extra = EepromReadByte(EEPROM_EVENTS);
  if ((extra & 0x20) != 0) {
    *(u16 *)presenceMasksOut |= 0x10;
  }
  if ((extra & 0x10) != 0) {
    *(u16 *)presenceMasksOut |= 0x20;
  }
  if ((extra & 0x40) != 0) {
    *(u16 *)presenceMasksOut |= 0x200;
  }
}

/* Forward scan that stops at index 9. */
u8 InventoryNext(uint selectionMask)
{
  u8 index;
  u8 steps;

  index = g_ui.view.inventory.cursor;
  if (index == PW_MENU_LAST_INDEX) {
    return 1;
  }
  index++;
  steps = 0;
  while (steps < PW_MENU_SLOT_COUNT) {
    if ((selectionMask & (1 << index)) != 0) {
      g_ui.view.inventory.cursor = index;
      return 0;
    }
    if (index == PW_MENU_LAST_INDEX) {
      return 1;
    }
    index++;
    steps++;
  }
  return 0;
}

/* Forward scan that wraps modulo 10. */
void InventoryNextWrap(uint selectionMask)
{
  u8 steps;

  g_ui.view.inventory.cursor = ((g_ui.view.inventory.cursor + 1) % 10);
  steps = 0;
  do {
    if ((selectionMask & (1 << g_ui.view.inventory.cursor)) != 0) {
      return;
    }
    g_ui.view.inventory.cursor = ((g_ui.view.inventory.cursor + 1) % 10);
    steps++;
  } while (steps < PW_MENU_SLOT_COUNT);
}

/* Backward scan that stops at index 0. */
u8 InventoryPrevious(uint selectionMask)
{
  u8 steps;

  if (g_ui.view.inventory.cursor == 0) {
    return 1;
  }
  g_ui.view.inventory.cursor--;
  steps = 0;
  while (steps < PW_MENU_SLOT_COUNT) {
    if ((selectionMask & (1 << g_ui.view.inventory.cursor)) != 0) {
      return 0;
    }
    if (g_ui.view.inventory.cursor == 0) {
      return 1;
    }
    g_ui.view.inventory.cursor--;
    steps++;
  }
  return 0;
}

/* Backward scan that wraps modulo 10. */
void InventoryPreviousWrap(uint selectionMask)
{
  u8 steps;

  g_ui.view.inventory.cursor = ((g_ui.view.inventory.cursor + 9) % 10);
  steps = 0;
  do {
    if ((selectionMask & (1 << g_ui.view.inventory.cursor)) != 0) {
      return;
    }
    g_ui.view.inventory.cursor = ((g_ui.view.inventory.cursor + 9) % 10);
    steps++;
  } while (steps < PW_MENU_SLOT_COUNT);
}

void PokemonSelectFirst(void)
{
  g_ui.view.inventory.cursor = PW_MENU_LAST_INDEX;
  InventoryNextWrap(g_ui.view.inventory.pokemonPresent);
}

void PokemonInventoryUpdate(void)
{
  u8 blocked;

  if (InputPressed(BUTTON_LEFT) != 0) {
    blocked = InventoryPrevious(g_ui.view.inventory.pokemonPresent);
    if (blocked != 0) {
      MenuReset();
      SetView(VIEW_MAIN_MENU);
      BeepLoadScore(1);
      return;
    }
    BeepLoadScore(2);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    blocked = InventoryNext(g_ui.view.inventory.pokemonPresent);
    if (blocked != 0) {
      if (g_ui.view.inventory.itemPresent != 0) {
        ItemSelectFirst();
        SetView(VIEW_ITEM_LIST);
        BeepLoadScore(2);
        return;
      }
      BeepLoadScore(1);
      return;
    }
    BeepLoadScore(2);
  }
  if (InputPressed(BUTTON_CENTER) == 0) {
    return;
  }
  if (g_ui.view.inventory.itemPresent != 0) {
    ItemSelectFirst();
    SetView(VIEW_ITEM_LIST);
  } else {
    HomeInit();
    SetView(VIEW_HOME);
  }
  BeepLoadScore(0);
}

/* Pokémon list: selected slot plus occupancy dots. */
void PokemonInventoryRender(void)
{
  UiResources *image;
  u8 *raster;
  u8 *source;
  u8 selected;
  u8 x;
  u8 y;
  s16 i;
  Item chosen;
  u16 length;

  image = (UiResources *)EEPROM_UI;
  length = 0x10;
  ScratchReset();
  raster = ScratchAlloc(0x180);
  source = image->menuLabels + 4 * (sizeof(image->menuLabels) / 6);
  EepromRead((u16)source, raster, sizeof(image->menuLabels) / 6);
  DisplayBlit(8, 0, 0x50, 0x10, raster);

  selected = g_ui.view.inventory.cursor;
  switch (selected) {
  case 0:
    RenderHeldPokemon(0x3c, 0x18);
    RenderHeldName(0, 0x30, 7);
    break;
  case 1:
  case 2:
  case 3:
    selected = (selected - 1);
    if (selected > 2) {
      selected = 0;
    }
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, encounters[0]),
               raster, (sizeof(Pokemon) * 3));
    EepromRead(
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon[selected]),
        raster + sizeof(Pokemon) * 3, sizeof(Pokemon));
    i = 0;
    do {
      if (*(u16 *)(raster + sizeof(Pokemon) * 3) ==
          *(u16 *)(raster + i * sizeof(Pokemon))) {
        RenderEnemyPokemon(0x3c, 0x18, i);
        RenderEnemyName(0, 0x30, i, 7);
        break;
      }
      i++;
    } while (i < 3);
    break;
  case 4:
    EepromRead((u16)(((EventPokemon *)EEPROM_EVENT_POKEMON)->pokemonImage +
                     (g_state.uiFrame & 1) * 0xc0),
               raster, 0x180);
    DisplayBlit(0x3c, 0x18, 0x20, 0x18, raster);
    RenderDistributionName(0, 0x30, 7);
    break;
  case 5:
    source = image->itemMap;
    EepromRead((u16)source, raster, sizeof(image->itemMap));
    DisplayBlit(0x3c, 0x18, 0x20, 0x18, raster);
    RenderMessage(0x30, MESSAGE_SPECIAL_MAP, 0x0f, 0);
    break;
  case 6:
  case 7:
  case 8:
    RenderItemCheckTreasure(0x3c, 0x18);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData,
                                        items[g_ui.view.inventory.cursor - 6]),
               &chosen, sizeof(Item));
    {
      u8 *numbers;
      numbers = ScratchAlloc(0x14);
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, itemId[0]),
                 numbers, 0x14);
      i = 0;
      do {
        if (chosen.id == *(u16 *)(numbers + i * 2)) {
          RenderCourseItem(0, 0x30, i, 0x0f);
          break;
        }
        i++;
      } while (i < 10);
    }
    break;
  case 9:
    source = image->itemTreasure;
    EepromRead((u16)source, raster, sizeof(image->itemTreasure));
    DisplayBlit(0x3c, 0x18, 0x20, 0x18, raster);
    RenderDistributionItem(0, 0x30, 0x0f);
    break;
  }

  source = image->arrows + ((g_state.uiFrame & 1) + 3) * length;
  EepromRead((u16)source, raster, length);
  x = ((g_ui.view.inventory.cursor % 5) * 8 + 0x10);
  if (g_ui.view.inventory.cursor == 0) {
    x = (x - 8);
  }
  y = ((g_ui.view.inventory.cursor / 5) * 0x10 + 0x10);
  DisplayBlit(x, y, 8, 8, raster);

  source = image->largeArrows + 0x20;
  EepromRead((u16)source, raster, 0x40);
  DisplayBlit(0, 0, 8, 0x10, raster + 0x20);
  if (g_ui.view.inventory.itemPresent != 0) {
    DisplayBlit(0x58, 0, 8, 0x10, raster);
  }

  source = image->ball;
  EepromRead((u16)source, raster, length);
  if ((g_ui.view.inventory.pokemonPresent & 1) != 0) {
    DisplayBlit(8, 0x18, 8, 8, raster);
  }
  i = 0;
  do {
    if ((g_ui.view.inventory.pokemonPresent & (2 << i)) != 0) {
      DisplayBlit((i * 8 + 0x18), 0x18, 8, 8, raster);
    }
    i++;
  } while (i < 3);

  source = image->treasure;
  EepromRead((u16)source, raster, length);
  i = 0;
  do {
    if ((g_ui.view.inventory.pokemonPresent & (0x40 << i)) != 0) {
      DisplayBlit((i * 8 + 0x18), 0x28, 8, 8, raster);
    }
    i++;
  } while (i < 3);

  if ((g_ui.view.inventory.pokemonPresent & 0x10) != 0) {
    source = image->eventBall;
    EepromRead((u16)source, raster, length);
    DisplayBlit(0x30, 0x18, 8, 8, raster);
  }
  if ((g_ui.view.inventory.pokemonPresent & 0x200) != 0) {
    source = image->eventTreasure;
    EepromRead((u16)source, raster, length);
    DisplayBlit(0x30, 0x28, 8, 8, raster);
  }
  if ((g_ui.view.inventory.pokemonPresent & 0x20) != 0) {
    source = image->eventMap;
    EepromRead((u16)source, raster, length);
    DisplayBlit(0x10, 0x28, 8, 8, raster);
  }

  RenderBattery(0x58, 0);
}

void ItemSelectFirst(void)
{
  g_ui.view.inventory.cursor = PW_MENU_LAST_INDEX;
  InventoryNextWrap(g_ui.view.inventory.itemPresent);
}

void ItemInventoryUpdate(void)
{
  u8 blocked;

  if (InputPressed(BUTTON_LEFT) != 0) {
    blocked = InventoryPrevious(g_ui.view.inventory.itemPresent);
    if (blocked != 0) {
      if (g_ui.view.inventory.pokemonPresent != 0) {
        g_ui.view.inventory.cursor = 0;
        InventoryPreviousWrap(g_ui.view.inventory.pokemonPresent);
        SetView(VIEW_POKEMON_LIST);
        BeepLoadScore(2);
        return;
      }
      BeepLoadScore(1);
      return;
    }
    BeepLoadScore(2);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    blocked = InventoryNext(g_ui.view.inventory.itemPresent);
    if (blocked != 0) {
      BeepLoadScore(1);
      return;
    }
    BeepLoadScore(2);
  }
  if (InputPressed(BUTTON_CENTER) == 0) {
    return;
  }
  HomeInit();
  SetView(VIEW_HOME);
  BeepLoadScore(0);
}

/* Item list: selected slot plus occupancy dots. */
void ItemInventoryRender(void)
{
  UiResources *image;
  u8 *raster;
  u8 *source;
  u8 *numbers;
  u8 x;
  s16 i;
  Item chosen;

  image = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(sizeof(image->menuLabels) / 6);
  source = image->largeArrows;
  EepromRead((u16)source, raster, sizeof(image->largeArrows) / 4);
  DisplayBlit(0, 0, 8, 0x10, raster);
  source = image->menuLabels + 4 * (sizeof(image->menuLabels) / 6);
  EepromRead((u16)source, raster, sizeof(image->menuLabels) / 6);
  DisplayBlit(8, 0, 0x50, 0x10, raster);
  source = image->itemPresent;
  EepromRead((u16)source, raster, sizeof(image->itemPresent));
  DisplayBlit(0x3c, 0x18, 0x20, 0x18, raster);

  source = image->arrows + ((g_state.uiFrame & 1) + 3) * 0x10;
  EepromRead((u16)source, raster, 0x10);
  x = ((g_ui.view.inventory.cursor % 5) * 8 + 0x10);
  DisplayBlit(x, ((g_ui.view.inventory.cursor / 5) * 0x10 + 0x10), 8, 8,
              raster);

  source = image->treasure;
  EepromRead((u16)source, raster, 0x10);
  i = 0;
  do {
    if ((g_ui.view.inventory.itemPresent & (1 << i)) != 0) {
      DisplayBlit((i * 8 + 0x10), 0x18, 8, 8, raster);
    }
    i++;
  } while (i < 5);
  i = 0;
  do {
    if ((g_ui.view.inventory.itemPresent & (0x20 << i)) != 0) {
      DisplayBlit((i * 8 + 0x10), 0x28, 8, 8, raster);
    }
    i++;
  } while (i < 5);

  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData,
                                      friendItems[g_ui.view.inventory.cursor]),
             &chosen, sizeof(Item));
  numbers = ScratchAlloc(0x14);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, itemId[0]),
             numbers, 0x14);
  i = 0;
  do {
    if (chosen.id == *(u16 *)(numbers + i * 2)) {
      RenderCourseItem(0, 0x30, i, 0x0f);
      break;
    }
    i++;
  } while (i < 10);

  RenderBattery(0x58, 0);
}
