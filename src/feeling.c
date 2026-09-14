#include "feeling_present.h"
#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "diary.h"
#include "eeprom.h"
#include "feeling.h"
#include "home.h"
#include "clear.h"
#include "common.h"
#include "scratch.h"

extern const SocialFrame *const g_socialSequences[7];

#define PW_DIARY_ACTION_EVENT_BASE 16
#define PW_ITEM_SLOT_COUNT 3
#define PW_MOTION_EVENT_TIER_STEP_UNITS 500
#define PW_COURSE_ITEM_INDEX_SPAN 0x1194
#define PW_COURSE_ITEM_INDEX_BASE 9
#define PW_WATT_REWARD_EVENT_2 50
#define PW_WATT_REWARD_EVENT_3 20
#define PW_WATT_REWARD_EVENT_4 10
#define PW_FRIENDLY_AFTER_AUTOGENERATE 0x46

/* Register the third encounter as the accompanying Pokemon, including its
 * artwork, and reset its nickname and friendship. */
void AutoGeneratePokemon(void)
{
  u8 *buf;
  u16 courseBytes;

  courseBytes = sizeof(Course);
  g_state.hourSteps = 0;
  g_state.flags.byte |= SYSTEM_HAS_POKEMON;
  ScratchReset();
  buf = ScratchAlloc(sizeof(DeviceStatus));
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, buf,
                   sizeof(DeviceStatus));
  if (((DeviceStatus *)buf)->hasPokemon == 0) {
    ((DeviceStatus *)buf)->hasPokemon = 1;
    ((DeviceStatus *)buf)->pokemonCompatibilityLe =
        ((DeviceStatus *)buf)->consoleCompatibilityLe;
    ((DeviceStatus *)buf)->pokemonGameVersionLe =
        ((DeviceStatus *)buf)->gameVersionLe;
    ((DeviceStatus *)buf)->generatedPokemon = 1;
    EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, buf,
                      sizeof(DeviceStatus));

    ScratchReset();
    buf = ScratchAlloc(POKEMON_ANIMATION_BYTES);
    EepromRead(
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                 encounterImages[2 * POKEMON_ANIMATION_BYTES]),
        buf, POKEMON_ANIMATION_BYTES);
    EepromWrite(
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, pokemonImage),
        buf, POKEMON_ANIMATION_BYTES);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                        encounterImageLarge),
               buf, POKEMON_LARGE_ANIMATION_BYTES);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                         pokemonImageLarge),
                buf, POKEMON_LARGE_ANIMATION_BYTES);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                        encounterNames[2 * TEXT_RASTER_BYTES]),
               buf, TEXT_RASTER_BYTES);
    EepromWrite(
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, pokemonName),
        buf, TEXT_RASTER_BYTES);

    ScratchReset();
    buf = ScratchAlloc(courseBytes);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values),
               buf, courseBytes);
    {
      u16 i;

      ((Course *)buf)->pokemon = ((Course *)buf)->encounters[2];
      ((Course *)buf)->pokemon.reservedAppearance = 0;
      ((Course *)buf)->friendship = PW_FRIENDLY_AFTER_AUTOGENERATE;
      i = 0;
      do {
        ((Course *)buf)->nickname[i] = 0;
        i++;
      } while (i < sizeof(((Course *)0)->nickname));
      EepromWrite(
          PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values), buf,
          courseBytes);
    }

    g_state.save.pokemonMinutes = 0;
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    ClearDiaryActions();
  }
}

/* Apply one periodic social reward and enter controller state 0x0c. Event 1
 * awards a course item if a slot is free; events 2/3/4 award 50/20/10 Watts;
 * event 7 generates the held Pokemon. Append diary action event+16 and select
 * the message and animation. */
void SocialApply(u8 eventType)
{
  u16 itemNumber;
  u8 emptySlot;
  u16 slotBytes;
  u8 *slots;

  itemNumber = 0;
  g_ui.view.social.eventType = eventType;
  g_ui.view.social.frame = g_socialSequences[eventType - 1];
  SetView(VIEW_SOCIAL);
  g_state.socialElapsedSeconds = 0;

  switch (eventType) {
  case 1:
    if (g_state.hourSteps < PW_COURSE_ITEM_INDEX_SPAN) {
      g_ui.view.social.rewardValue =
          (PW_COURSE_ITEM_INDEX_BASE -
           (g_state.hourSteps / PW_MOTION_EVENT_TIER_STEP_UNITS));
    } else {
      g_ui.view.social.rewardValue = 0;
    }
    itemNumber = CourseItemNumber(g_ui.view.social.rewardValue);
    ScratchReset();
    slots = ScratchAlloc(slotBytes = sizeof(Item) * PW_ITEM_SLOT_COUNT);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), slots,
               slotBytes);
    if ((emptySlot = ItemSlotFindEmpty((Item *)slots)) < PW_ITEM_SLOT_COUNT) {
      ((Item *)slots)[emptySlot].id = itemNumber;
      EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), slots,
                  slotBytes);
    }
    break;
  case 2:
    g_ui.view.social.rewardValue = PW_WATT_REWARD_EVENT_2;
    WattsAdd(PW_WATT_REWARD_EVENT_2);
    break;
  case 3:
    g_ui.view.social.rewardValue = PW_WATT_REWARD_EVENT_3;
    WattsAdd(PW_WATT_REWARD_EVENT_3);
    break;
  case 4:
    g_ui.view.social.rewardValue = PW_WATT_REWARD_EVENT_4;
    WattsAdd(PW_WATT_REWARD_EVENT_4);
    break;
  case 7:
    AutoGeneratePokemon();
    break;
  default:
    g_ui.view.social.rewardValue = 0;
    break;
  }

  ScratchReset();
  slots = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, slots, sizeof(Course));
  {
    u8 encounter;

    encounter = 0;
    DiaryAppend((Course *)slots, ScratchAlloc(sizeof(DiaryEntry)),
                (eventType + PW_DIARY_ACTION_EVENT_BASE),
                g_state.save.bonusCourse, encounter, itemNumber);
  }

  switch (eventType) {
  case 2:
  case 3:
  case 4:
  case 5: {
    u32 prng;

    prng = RandomNext() >> 3;
    g_ui.view.social.messageVariant = (prng % 3UL);
  } break;
  default:
    g_ui.view.social.messageVariant = 0;
    break;
  }
}

/* Seven sequence pointers indexed by one-based social event IDs 1..7. */
const SocialFrame *const g_socialSequences[7] = {
    g_socialItemSequence,      g_socialWatts50Sequence, g_socialWatts20Sequence,
    g_socialWatts10Sequence,   g_socialBoredSequence,   0,
    g_socialNewPokemonSequence};
