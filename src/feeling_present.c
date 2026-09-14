#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "display.h"
#include "beep.h"
#include "eeprom.h"
#include "feeling_present.h"
#include "home.h"
#include "pad.h"
#include "common.h"
#include "scratch.h"

#define PW_MOTION_HOURLY_STEP_THRESHOLD_100 100ul
#define PW_MOTION_HOURLY_STEP_THRESHOLD_40 40
#define PW_MOTION_HOURLY_STEP_THRESHOLD_300 300
#define PW_MOTION_HOURLY_STEP_THRESHOLD_80 80
#define PW_MOTION_HOURLY_STEP_THRESHOLD_250 250
#define PW_MOTION_HOURLY_STEP_THRESHOLD_200 200
#define PW_RTC_ONE_HOUR_SECONDS 3600
#define PW_MOTION_SESSION_UI_IDLE_SECONDS 90
#define PW_MOTION_EVENT_TIER_STEP_UNITS 500
#define PW_INTERACTIVE_IDLE_SECONDS 60
#define PW_MOTION_EVENT_OFFER_COUNTDOWN_INITIAL 0x30

/* HOME press on a terminal record returns to the home view; otherwise the
 * four-byte cursor advances and the next record's beeper runs unless that id is
 * the silent 0x10 marker. */
void SocialUpdate(void)
{
  const SocialFrame *record;

  if (InputPressed(BUTTON_CENTER) != 0) {
    record = g_ui.view.social.frame;
    if (record->flags.bits.terminal) {
      HomeInit();
      SetView(VIEW_HOME);
    } else {
      record++;
      g_ui.view.social.frame = record;
      if (record->beeper != SOCIAL_SILENT) {
        BeepLoadScore(g_ui.view.social.frame->beeper);
      }
    }
  }
}

/* Message selectors 0xfc/0xfd/0xfe/0xff request Pokemon name, item name, Watt
 * digits, or no message. Other values select a raster message. Overlay 0x10f is
 * used only with selector 0xff. */
void SocialRender(void)
{
  u8 bank1;

  ScratchReset();
  ScratchAlloc(sizeof(((CourseResources *)0)->course));

  if (g_ui.view.social.frame->flags.bits.showPokemon) {
    RenderHeldPokemon(0x20, 0x04);
  }

  if (g_ui.view.social.frame->flags.bits.bubble != SOCIAL_NO_BUBBLE) {
    RenderFeeling(g_ui.view.social.frame->flags.bits.bubble);
  }

  if (g_ui.view.social.frame->flags.bits.showTreasure) {
    RenderTreasure(0x14, 0x14);
  }

  bank1 = g_ui.view.social.rewardValue;
  switch (g_ui.view.social.frame->message) {
  case SOCIAL_POKEMON_NAME:
    RenderHeldName(0x00, 0x20, 5);
    break;
  case SOCIAL_ITEM_NAME:
    RenderCourseItem(0x00, 0x20, bank1, 0x0d);
    break;
  case SOCIAL_WATTS:
    RenderDecoratedNumber(0x02, 0x20, bank1, 0x0d);
    break;
  case SOCIAL_NO_MESSAGE:
    break;
  default:
    RenderMessage(0x20, g_ui.view.social.frame->message, 0x0d, 0);
    break;
  }

  if (g_ui.view.social.frame->flags.bits.panelMode > 1) {
    RenderMessage(
        0x30, (g_ui.view.social.frame->panel + g_ui.view.social.messageVariant),
        0x0e, 1);
  } else if (g_ui.view.social.frame->message == SOCIAL_NO_MESSAGE) {
    RenderMessage(0x30, g_ui.view.social.frame->panel, 0x0f, 1);
  } else {
    RenderMessage(0x30, g_ui.view.social.frame->panel, 0x0e, 1);
  }
  RenderBattery(0, 0);
}

/* Home-idle offer gate: 40-percent PRNG, then item / watt / boredom / autogen
 * from Pokemon presence, friendship, free item slots, steps and Pokemon
 * minutes. */
void SocialOfferCheck(void)
{
  volatile u16 hourSteps;
  Course *course;
  Item *items;
  u8 friendly;

  if ((g_state.flags.byte & SYSTEM_MODE_MASK) != SYSTEM_MODE_INTERACTIVE) {
    return;
  }
  if (g_state.view != VIEW_HOME) {
    return;
  }
  if (g_state.flags.bits.socialOfferPending == 0) {
    return;
  }

  g_state.flags.bits.socialOfferPending = 0;
  g_ui.view.home.eventOfferCountdown = g_ui.view.home.pendingEventId = 0;
  hourSteps = g_state.hourSteps;
  if ((u8)(RandomNext() % 100ul) >= 40) {
    return;
  }

  if (g_state.flags.bits.hasPokemon == 0) {
    if (g_state.hourSteps < 300) {
      return;
    }
    g_ui.view.home.pendingEventId = 7;
  } else {
    if (g_state.socialElapsedSeconds < PW_RTC_ONE_HOUR_SECONDS) {
      return;
    }

    ScratchReset();
    course = ScratchAlloc(sizeof(Course));
    EepromRead(EEPROM_COURSE, course, sizeof(Course));
    friendly = course->friendship;
    ScratchReset();
    items = ScratchAlloc(sizeof(Item) * 3);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), items,
               sizeof(Item) * 3);
    if ((ItemSlotFindEmpty(items) < 3) && (friendly >= 90) &&
        (hourSteps >= 500)) {
      g_ui.view.home.pendingEventId = 1;
    } else if ((friendly >= 80) && (hourSteps >= 250)) {
      g_ui.view.home.pendingEventId = 2;
    } else if (hourSteps >= 200) {
      g_ui.view.home.pendingEventId = 3;
    } else if (hourSteps >= 100) {
      g_ui.view.home.pendingEventId = 4;
    } else if ((g_state.save.pokemonMinutes >= 60) && (hourSteps <= 50)) {
      g_ui.view.home.pendingEventId = 5;
    } else {
      return;
    }
  }

  g_ui.view.home.eventOfferCountdown = PW_MOTION_EVENT_OFFER_COUNTDOWN_INITIAL;
}

const SocialFrame g_socialItemSequence[3] = {
    {{SOCIAL_FLAGS(0, 1, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     50},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, 1, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     63},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, 1,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON |
                       SOCIAL_SHOW_TREASURE)},
     5,
     SOCIAL_ITEM_NAME,
     24}};

const SocialFrame g_socialWatts50Sequence[3] = {
    {{SOCIAL_FLAGS(1, 3, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     51},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, 1, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     63},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, 1,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON |
                       SOCIAL_SHOW_TREASURE)},
     5,
     SOCIAL_WATTS,
     24}};

const SocialFrame g_socialWatts20Sequence[3] = {
    {{SOCIAL_FLAGS(2, 3, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     52},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, 1, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     63},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, 1,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON |
                       SOCIAL_SHOW_TREASURE)},
     5,
     SOCIAL_WATTS,
     24}};

const SocialFrame g_socialWatts10Sequence[3] = {
    {{SOCIAL_FLAGS(3, 3, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     57},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, 1, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     63},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, 1,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON |
                       SOCIAL_SHOW_TREASURE)},
     5,
     SOCIAL_WATTS,
     24}};

const SocialFrame g_socialBoredSequence[1] = {
    {{SOCIAL_FLAGS(4, 3, SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     60}};

const SocialFrame g_socialNewPokemonSequence[2] = {
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, 1, 0)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     64},
    {{SOCIAL_FLAGS(6, 1, SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON)},
     7,
     SOCIAL_POKEMON_NAME,
     65}};
