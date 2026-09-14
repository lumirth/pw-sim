#include "flags.h"
#include "types.h"
#include "eeprom_address.h"
#include <stddef.h>
#include "project.h"
#include "display.h"
#include <machine.h>
#include "beep.h"
#include "eeprom.h"
#include "feeling.h"
#include "ir.h"
#include "home.h"
#include "interface.h"
#include "option.h"
#include "pad.h"
#include "common.h"
#include "scratch.h"

/* Home view of the shared UI state. */

#define PW_DISPLAY_FULL_RASTER_BIT_SPAN 0x40
#define PW_MENU_SELECTION_IRC 2
#define PW_MENU_SELECTION_LAST 5
#define PW_MENU_SELECTION_FIRST 0
#define PW_DEFAULT_UI_INITIAL_ORIGIN_LOW 0x20
#define PW_HOME_POKEMON_ORIGIN_MIN 0x20
#define PW_HOME_POKEMON_ORIGIN_ENTRY_MAX 0x60
#define PW_HOME_POKEMON_ORIGIN_OSC_MAX 0x40
#define PW_HOME_POKEMON_ORIGIN_RESET 0x68
#define PW_HOME_POKEMON_STEP 4
#define PW_HOME_ICON_ORIGIN_HIGH 0x18

/* Snapshot the current foreground target before installing its replacement. */
void InstallTask(void (*nextTarget)(void))
{
  g_previousTask = g_task;
  g_task = nextTarget;
}

/* Enter the IR foreground only from the main application tick. The unregistered
 * walker shows ROM smile/signal frames; a registered walker shows the uploaded
 * composition plus a battery icon. Interrupts are masked before IrBegin.
 */
void TryBeginIr(void)
{
  SystemFlags flags;

  if (g_task == MainTick) {
    flags.byte = g_state.flags.byte;
    if (flags.bits.registered == 0) {
      DisplayClear(PW_DISPLAY_FULL_RASTER_BIT_SPAN);
      RenderIrRomFrame(0);
      DisplayToggleBank();
      DisplayClear(PW_DISPLAY_FULL_RASTER_BIT_SPAN);
      RenderIrRomFrame(1);
    } else {
      DisplayClear(PW_DISPLAY_FULL_RASTER_BIT_SPAN);
      RenderIrUploadedFrame(0);
      RenderBattery(0, 0);
      DisplayToggleBank();
      DisplayClear(PW_DISPLAY_FULL_RASTER_BIT_SPAN);
      RenderIrUploadedFrame(1);
      RenderBattery(0, 0);
    }
    DisplayToggleBank();
    set_ccr(0x80);
    IrBegin();
    InstallTask(IrProtocolTick);
  }
}

void SetView(u8 nextState)
{
  g_state.view = nextState;
}

/* Read the course-view header and draw the large held-Pokemon frame. An idle
 * offer selects frame 0 or 1 from the UI phase; a live offer always selects
 * frame 0. */
void RenderLargePokemon(u8 x, u8 y)
{
  u8 *raster;
  u16 length;

  length = 0x300;
  ScratchReset();
  raster = ScratchAlloc(length);
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values.pokemon),
      raster, sizeof(Pokemon));
  if (g_ui.view.home.eventOfferCountdown != 0) {
    EepromRead((u16)((CourseResources *)EEPROM_COURSE)->pokemonImageLarge,
               raster, length);
  } else {
    EepromRead((u16)(((CourseResources *)EEPROM_COURSE)->pokemonImageLarge +
                     ((g_state.uiFrame >> 1) & 1) * 0x300),
               raster, length);
  }
  DisplayBlit(x, y, 0x40, 0x30, raster);
}

void HomeInit(void)
{
  g_ui.view.home.pendingEventId = 0;
  g_ui.view.home.eventOfferCountdown = 0;
  g_ui.view.home.animatedRasterOriginLow = PW_DEFAULT_UI_INITIAL_ORIGIN_LOW;
  g_ui.view.home.animationControl.bits.entered = 0;
  g_ui.view.home.animationControl.bits.smallFrame = 0;
  g_ui.view.home.animationControl.bits.movingRight = 0;
}

/* Unregistered walker: a center press or IR request starts communication.
 * Registered walker: consume a pending social-event offer, or route HOME / LEFT
 * / RIGHT into the main menu. */
void HomeUpdate(void)
{
  {
    SystemFlags registered;

    registered.byte = g_state.flags.byte;
    if (registered.bits.registered == 0) {
      if (InputPressed(BUTTON_CENTER) == 0) {
        SystemEvents event;

        event.byte = g_state.events.byte;
        if (event.bits.irRequested == 0) {
          return;
        }
      }
      TryBeginIr();
      return;
    }
  }

  if (g_ui.view.home.eventOfferCountdown != 0) {
    if (InputPressed(BUTTON_ANY) != 0) {
      g_ui.view.home.eventOfferCountdown = 0;
      SocialApply(g_ui.view.home.pendingEventId);
      return;
    }
    g_ui.view.home.eventOfferCountdown--;
  }

  if (InputPressed(BUTTON_CENTER) != 0) {
    BeepLoadScore(0);
    MenuReset();
    g_state.menuSelection = PW_MENU_SELECTION_IRC;
  } else {
    if (InputPressed(BUTTON_LEFT) != 0) {
      g_state.menuSelection = PW_MENU_SELECTION_LAST;
      BeepLoadScore(0);
      MenuReset();
    } else {
      if (InputPressed(BUTTON_RIGHT) == 0) {
        return;
      }
      g_state.menuSelection = PW_MENU_SELECTION_FIRST;
      BeepLoadScore(0);
      MenuReset();
    }
  }
  SetView(VIEW_MAIN_MENU);
}

/* Read one 0x60-byte feeling-bubble raster and blit it at 0x0404 / 0x1018. */
void RenderFeeling(u8 recordIndex)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->feelings) / 7);
  source = ((UiResources *)EEPROM_UI)->feelings +
           recordIndex * (sizeof(((UiResources *)0)->feelings) / 7);
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->feelings) / 7);
  DisplayBlit(0x04, 0x04, 0x18, 0x10, raster);
}

/* Use the distributed-course background when bonusCourseFlag is set; otherwise
 * use the ordinary course background. */
void RenderCourseBackground(void)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((CourseResources *)0)->course));
  if (g_state.save.bonusCourse) {
    source = ((BonusResources *)EEPROM_BONUS_COURSE)->course;
  } else {
    source = ((CourseResources *)EEPROM_COURSE)->course;
  }
  EepromRead((u16)source, raster, sizeof(((CourseResources *)0)->course));
  DisplayBlit(0x00, 0x18, 0x20, 0x18, raster);
}

/* Motion at the left boundary starts the entry animation. Switch to the small
 * sprite after reaching the far boundary. */
void HomeEntryAdvance(void)
{
  {
    HomeMotionFlags dir;

    dir.byte = g_ui.view.home.animationControl.byte;
    if (dir.bits.movingRight == 0) {
      g_ui.view.home.animatedRasterOriginLow =
          (g_ui.view.home.animatedRasterOriginLow - PW_HOME_POKEMON_STEP);
      if (g_ui.view.home.animatedRasterOriginLow <=
          PW_HOME_POKEMON_ORIGIN_MIN) {
        g_ui.view.home.animatedRasterOriginLow = PW_HOME_POKEMON_ORIGIN_MIN;
      }
    } else {
      g_ui.view.home.animatedRasterOriginLow =
          (g_ui.view.home.animatedRasterOriginLow + PW_HOME_POKEMON_STEP);
      if (g_ui.view.home.animatedRasterOriginLow >=
          PW_HOME_POKEMON_ORIGIN_ENTRY_MAX) {
        g_ui.view.home.animationControl.bits.smallFrame = 1;
        g_ui.view.home.animationControl.bits.movingRight = 0;
      }
    }
  }

  {
    SystemEvents event;

    event.byte = g_state.events.byte;
    if (event.bits.motionDetected != 0) {
      if (g_ui.view.home.animatedRasterOriginLow <=
          PW_HOME_POKEMON_ORIGIN_MIN) {
        g_ui.view.home.animationControl.bits.movingRight = 1;
        g_ui.view.home.animationControl.bits.entered = 1;
      }
    }
  }
}

/* Advance every fourth UI frame. Park the Pokemon offscreen when motion stops.
 */
void HomeOscillationAdvance(void)
{
  HomeMotionFlags flags;

  if ((g_state.uiFrame & 3) != 0) {
    return;
  }

  flags.byte = g_ui.view.home.animationControl.byte;
  if (flags.bits.movingRight == 0) {
    g_ui.view.home.animatedRasterOriginLow =
        (g_ui.view.home.animatedRasterOriginLow - PW_HOME_POKEMON_STEP);
    if (g_ui.view.home.animatedRasterOriginLow <= PW_HOME_POKEMON_ORIGIN_MIN) {
      g_ui.view.home.animatedRasterOriginLow = PW_HOME_POKEMON_ORIGIN_MIN;
      g_ui.view.home.animationControl.bits.movingRight ^= 1;
    }
  } else {
    g_ui.view.home.animatedRasterOriginLow =
        (g_ui.view.home.animatedRasterOriginLow + PW_HOME_POKEMON_STEP);
    if (g_ui.view.home.animatedRasterOriginLow >=
        PW_HOME_POKEMON_ORIGIN_OSC_MAX) {
      g_ui.view.home.animatedRasterOriginLow = PW_HOME_POKEMON_ORIGIN_OSC_MAX;
      g_ui.view.home.animationControl.bits.movingRight ^= 1;
    }
  }

  if ((g_state.events.byte & EVENT_MOTION) != 0) {
    return;
  }
  g_ui.view.home.animatedRasterOriginLow = PW_HOME_POKEMON_ORIGIN_RESET;
  g_ui.view.home.animationControl.bits.smallFrame = 0;
  g_ui.view.home.animationControl.bits.movingRight = 0;
}

/* Home composition: optional feeling bubble, course background, then the large
 * or icon held Pokémon layer and its motion advance. */
void HomeRender(void)
{
  u8 origin;
  u8 *raster;
  Pokemon *pokemon;

  if (g_ui.view.home.eventOfferCountdown != 0) {
    RenderFeeling(g_socialBubbles[g_ui.view.home.pendingEventId - 1]);
  }
  RenderCourseBackground();

  if (g_state.flags.bits.hasPokemon == 0) {
    return;
  }

  origin = g_ui.view.home.animatedRasterOriginLow;
  if (g_ui.view.home.animationControl.bits.smallFrame == 0) {
    RenderLargePokemon(origin, 0);
  } else if (g_ui.view.home.animationControl.bits.movingRight == 0) {
    RenderHeldPokemon(origin, PW_HOME_ICON_ORIGIN_HIGH);
  } else {
    ScratchReset();
    raster = ScratchAlloc(sizeof(Pokemon));
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                        values.pokemon),
               raster, sizeof(Pokemon));
    pokemon = (Pokemon *)raster;
    if (pokemon->fixedFacing == 0) {
      RenderHeldPokemonMirrored(g_ui.view.home.animatedRasterOriginLow,
                                PW_HOME_ICON_ORIGIN_HIGH);
    } else {
      RenderHeldPokemon(g_ui.view.home.animatedRasterOriginLow,
                        PW_HOME_ICON_ORIGIN_HIGH);
    }
  }

  if (g_ui.view.home.animationControl.bits.smallFrame != 0) {
    HomeOscillationAdvance();
  } else {
    HomeEntryAdvance();
  }
}
