#include "graphics.h"
#include "flags.h"
#include "types.h"
#include "eeprom_address.h"
#include "iodefine.h"
#include "project.h"
#include "display.h"
#include <machine.h>
#include "accelerometer.h"
#include "battery.h"
#include "battle.h"
#include "beep.h"
#include "discard.h"
#include "demo.h"
#include "dowsing.h"
#include "eeprom.h"
#include "feeling_present.h"
#include "ir.h"
#include "friend.h"
#include "home.h"
#include "home_setting.h"
#include "interface.h"
#include "list.h"
#include "motion.h"
#include "option.h"
#include "pad.h"
#include "common.h"
#include "poketrace.h"
#include "power.h"
#include "rtc.h"
#include "scratch.h"
#include "selftest.h"
#include "viewer.h"

void RenderUnregisteredHome(void);
void RenderUnregisteredResult(void);
void RenderIrResult(void);
void RenderStepCount(void);

void CaptureSample(void);

void ViewUpdate(void);
void ViewRender(void);

void BeepTick(void);

#define PW_ACCEL_RING_MASK 0x3f
#define PW_ACCEL_RING_LAST 0x3f
#define PW_ACCEL_REG_X_LSB 0x02

/* Unregistered home walker plus blinking up-arrow. */
void RenderUnregisteredHome(void)
{
  u8 *raster;
  const u8 *shell;
  const u8 *overlay;
  u8 *cell;
  uint i;

  ScratchReset();
  raster = ScratchAlloc(0x100);
  shell = g_walkerImage;
  i = 0;
  do {
    raster[i] = shell[i];
    i++;
  } while (i < 0x100);
  overlay = g_neutralFace;
  cell = raster + 0x50;
  i = 0;
  do {
    cell[i] = (cell[i] | (overlay[i] * 8));
    cell[i + 0x40] = (cell[i + 0x40] | (overlay[i] / 0x20));
    i++;
  } while (i < 0x20);
  if (((g_state.uiFrame >> 2) & 1) != 0) {
    overlay = g_buttonArrow;
    cell = raster + 0xd8;
    i = 0;
    do {
      cell[i] = (cell[i] | (overlay[i] * 0x10));
      i++;
    } while (i < 0x10);
    DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
    i = 0;
    do {
      raster[i] = (overlay[i] / 0x10);
      i++;
    } while (i < 0x10);
    DisplayBlit(0x2c, 0x30, 8, 8, raster);
  } else {
    DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  }
}

/* Frown walker, then auto-return after eight frames. */
void RenderUnregisteredResult(void)
{
  u8 *raster;
  const u8 *shell;
  const u8 *overlay;
  u8 *cell;
  uint i;

  ScratchReset();
  raster = ScratchAlloc(0x100);
  shell = g_walkerImage;
  i = 0;
  do {
    raster[i] = shell[i];
    i++;
  } while (i < 0x100);
  overlay = g_frownFace;
  cell = raster + 0x50;
  i = 0;
  do {
    cell[i] = (cell[i] | (overlay[i] * 8));
    cell[i + 0x40] = (cell[i + 0x40] | (overlay[i] / 0x20));
    i++;
  } while (i < 0x20);
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  g_ui.view.presentation.frame++;
  if (g_ui.view.presentation.frame > 8) {
    HomeInit();
    SetView(VIEW_HOME);
  }
}

/* Smile walker, optional IR-activity icon. */
void RenderIrRomFrame(u8 signalIconRequested)
{
  u8 *raster;
  const u8 *shell;
  const u8 *overlay;
  u8 *cell;
  uint i;

  ScratchReset();
  raster = ScratchAlloc(0x100);
  if (signalIconRequested != 0) {
    DisplayBlit(0x2c, 0x00, 0x08, 0x08, g_irSignal);
  }
  shell = g_walkerImage;
  i = 0;
  do {
    raster[i] = shell[i];
    i++;
  } while (i < 0x100);
  overlay = g_smileFace;
  cell = raster + 0x50;
  i = 0;
  do {
    cell[i] = (cell[i] | (overlay[i] * 8));
    cell[i + 0x40] = (cell[i + 0x40] | (overlay[i] / 0x20));
    i++;
  } while (i < 0x20);
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
}

/* Uploaded walker image with an optional radio icon. */
void RenderIrUploadedFrame(u8 optionalPrefixRequested)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(0x180);
  if (optionalPrefixRequested != 0) {
    source = ((UiResources *)EEPROM_UI)->radio;
    EepromRead((u16)source, raster, sizeof(((UiResources *)0)->radio));
    DisplayBlit(0x2c, 0x00, 0x08, 0x10, raster);
  }
  source = ((UiResources *)EEPROM_UI)->walker;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->walker));
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  RenderMessage(0x30, 0, 0x0f, 0);
}

/* Render the completed IR session's result. */
void RenderIrResult(void)
{
  u8 *raster;
  u8 *source;

  ScratchReset();
  raster = ScratchAlloc(0x180);
  source = ((UiResources *)EEPROM_UI)->walker;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->walker));
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  switch (g_state.irResult) {
  case 1:
    RenderMessage(0x30, 1, 0x0f, 1);
    break;
  case 2:
  case 8:
    RenderMessage(0x30, 4, 0x0f, 1);
    break;
  case 3:
    RenderMessage(0x20, 2, 0x0d, 0);
    RenderMessage(0x30, 3, 0x0e, 1);
    break;
  case 5:
    RenderMessage(0x20, 9, 0x0d, 0);
    RenderMessage(0x30, 0x0a, 0x0e, 1);
    break;
  case 6:
    RenderMessage(0x20, 7, 0x0d, 0);
    RenderMessage(0x30, 8, 0x0e, 1);
    break;
  case 7:
    RenderMessage(0x20, 0x0b, 0x0d, 0);
    RenderMessage(0x30, 0x0c, 0x0e, 1);
    break;
  case 4:
    RenderMessage(0x30, 0x15, 0x0f, 1);
    break;
  }

  RenderBattery(0x58, 0x00);
}

/* Dispatch the view update for controller states 0x00-0x17. */
void ViewUpdate(void)
{
  SystemFlags registered;

  RandomNext();
  switch (g_state.view) {
  case VIEW_HOME:
    HomeUpdate();
    break;
  case VIEW_SOCIAL:
    SocialUpdate();
    break;
  case VIEW_PEER:
    PeerUpdate();
    break;
  case VIEW_MAIN_MENU:
    MainMenuUpdate();
    break;
  case VIEW_DOWSING:
    DowsingUpdate();
    break;
  case VIEW_RADAR:
    RadarUpdate();
    break;
  case VIEW_BATTLE:
    BattleUpdate();
    break;
  case VIEW_RADAR_FAILURE:
    RadarFailureUpdate();
    break;
  case VIEW_DISCARD:
    DiscardUpdate();
    break;
  case VIEW_TRAINER:
    TrainerUpdate();
    break;
  case VIEW_POKEMON_LIST:
    PokemonInventoryUpdate();
    break;
  case VIEW_SETTINGS:
    SettingsUpdate();
    break;
  case VIEW_ITEM_LIST:
    ItemInventoryUpdate();
    break;
  case VIEW_IR_RESULT:
    registered.byte = g_state.flags.byte;
    if (registered.bits.registered != 0) {
      if (InputPressed(BUTTON_ANY) != 0) {
        HomeInit();
        SetView(VIEW_HOME);
        BeepLoadScore(0);
      }
    }
    break;
  case VIEW_WALK_START:
    WalkStartUpdate();
    break;
  case VIEW_WALK_END:
    WalkEndUpdate();
    break;
  case VIEW_EVENT_REWARD:
    EventRewardUpdate();
    break;
  case VIEW_DIAGNOSTICS:
    DiagnosticsUpdate();
    break;
  case VIEW_THRESHOLD_TEST:
    ThresholdUpdate();
    break;
  default:
    break;
  }
  g_state.viewUpdates++;
}

/* View-render dispatch for states 0x00-0x18. */
void ViewRender(void)
{
  SystemFlags registered;

  switch (g_state.view) {
  case VIEW_HOME:
    registered.byte = g_state.flags.byte;
    if (registered.bits.registered == 0) {
      RenderUnregisteredHome();
    } else {
      HomeRender();
      RenderStepCount();
    }
    break;
  case VIEW_SOCIAL:
    SocialRender();
    break;
  case VIEW_PEER:
    PeerRender();
    break;
  case VIEW_MAIN_MENU:
    MainMenuRender();
    break;
  case VIEW_DOWSING:
    DowsingRender();
    break;
  case VIEW_RADAR:
    RadarRender();
    break;
  case VIEW_BATTLE:
    BattleRender();
    break;
  case VIEW_RADAR_FAILURE:
    RadarFailureRender();
    break;
  case VIEW_DISCARD:
    DiscardRender();
    break;
  case VIEW_TRAINER:
    TrainerRender();
    break;
  case VIEW_POKEMON_LIST:
    PokemonInventoryRender();
    break;
  case VIEW_SETTINGS:
    SettingsRender();
    break;
  case VIEW_ITEM_LIST:
    ItemInventoryRender();
    break;
  case VIEW_IR_RESULT:
    registered.byte = g_state.flags.byte;
    if (registered.bits.registered == 0) {
      RenderUnregisteredResult();
    } else {
      RenderIrResult();
    }
    break;
  case VIEW_WALK_START:
    WalkStartRender();
    break;
  case VIEW_WALK_END:
    WalkEndRender();
    break;
  case VIEW_EVENT_REWARD:
    EventRewardRender();
    break;
  case VIEW_DIAGNOSTICS:
    DiagnosticsRender();
    break;
  case VIEW_THRESHOLD_TEST:
    ThresholdRender();
    break;
  case VIEW_THRESHOLD_FAILURE:
    ThresholdFailureRender();
    break;
  default:
    break;
  }
}

/* Home stamp/item/Pokémon icons and daily steps. */
void RenderStepCount(void)
{
  u8 flags;
  u8 *raster;
  u8 *source;
  UiResources *image;
  Pokemon *pokemon;
  Item *item;
  s16 i;

  image = (UiResources *)EEPROM_UI;
  DisplayRule();
  flags = EepromReadByte(EEPROM_EVENTS);
  ScratchReset();
  raster = ScratchAlloc(0x180);
  if ((flags & 0x20) != 0) {
    source = image->eventBall;
    EepromRead((u16)source, raster, sizeof(((UiResources *)0)->eventBall));
    i = 0;
    do {
      raster[i] |= 1;
      i++;
    } while (i < (s16)sizeof(((UiResources *)0)->eventBall));
    DisplayBlit(0x00, 0x30, 0x08, 0x08, raster);
  }
  if ((flags & 0x40) != 0) {
    source = (u8 *)&((EventItem *)EEPROM_EVENT_ITEM)->header;
    EepromRead((u16)source, raster, sizeof(EventItemHeader));
    if (((EventItemHeader *)raster)->itemIdLe != 0) {
      source = image->eventTreasure;
      EepromRead((u16)source, raster,
                 sizeof(((UiResources *)0)->eventTreasure));
      i = 0;
      do {
        raster[i] |= 1;
        i++;
      } while (i < (s16)sizeof(((UiResources *)0)->eventTreasure));
      DisplayBlit(0x08, 0x30, 0x08, 0x08, raster);
    }
  }
  source = image->stamps;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->stamps));
  i = 0;
  do {
    raster[i] |= 1;
    i++;
  } while (i < (s16)sizeof(((UiResources *)0)->stamps));
  if ((flags & 1) != 0) {
    DisplayBlit(0x10, 0x30, 0x08, 0x08, raster);
  }
  if ((flags & 2) != 0) {
    DisplayBlit(0x18, 0x30, 0x08, 0x08, raster + 0x10);
  }
  if ((flags & 4) != 0) {
    DisplayBlit(0x20, 0x30, 0x08, 0x08, raster + 0x20);
  }
  if ((flags & 8) != 0) {
    DisplayBlit(0x28, 0x30, 0x08, 0x08, raster + 0x30);
  }
  source = image->ball;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->ball));
  pokemon = (Pokemon *)(raster + sizeof(((UiResources *)0)->ball));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon), pokemon,
             (sizeof(Pokemon) * 3));
  i = 0;
  do {
    if (pokemon[i].id != 0) {
      DisplayBlit((i * 8), 0x38, 0x08, 0x08, raster);
    }
    i++;
  } while (i < 3);
  source = image->treasure;
  EepromRead((u16)source, raster, sizeof(((UiResources *)0)->treasure));
  item = (Item *)(raster + sizeof(((UiResources *)0)->ball));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), item,
             (sizeof(Item) * 3));
  i = 0;
  do {
    if (item[i].id != 0) {
      DisplayBlit((i * 8 + 0x18), 0x38, 0x08, 0x08, raster);
    }
    i++;
  } while (i < 3);
  if ((flags & 0x10) != 0) {
    source = image->eventMap;
    EepromRead((u16)source, raster, sizeof(((UiResources *)0)->eventMap));
    DisplayBlit(0x30, 0x38, 0x08, 0x08, raster);
  }
  RenderDecimal(0x58, 0x30, g_state.dailySteps, 1);
  RenderBattery(0, 0);
}

/* Reevaluate the volatile sample difference in the selected arm. */
#define PW_ABS(value) ((value) >= 0 ? (value) : -(value))

/* One XYZ sample into the 64-entry rings. */
void CaptureSample(void)
{
  u8 sample[6];
  u8 previous;

  AccelRead(PW_ACCEL_REG_X_LSB, sample, 6);
  g_work.motion.x[g_state.sampleIndex] = sample[1];
  g_work.motion.y[g_state.sampleIndex] = sample[3];
  g_work.motion.z[g_state.sampleIndex] = sample[5];
  if (g_state.view == VIEW_THRESHOLD_TEST) {
    previous = ((g_state.sampleIndex + 0x3f) & 0x3f);
    if (g_state.sampleIndex == 0) {
      g_ui.view.accel.xActivity = 0;
      g_ui.view.accel.yActivity = 0;
      g_ui.view.accel.zActivity = 0;
    }
    g_ui.view.accel.xActivity += PW_ABS(g_work.motion.x[g_state.sampleIndex] -
                                        g_work.motion.x[previous]);
    g_ui.view.accel.yActivity += PW_ABS(g_work.motion.y[g_state.sampleIndex] -
                                        g_work.motion.y[previous]);
    g_ui.view.accel.zActivity += PW_ABS(g_work.motion.z[g_state.sampleIndex] -
                                        g_work.motion.z[previous]);
  }
}

/* Main cooperative foreground task. */
void MainTick(void)
{
  IENR2.BIT.IENTB1 = 1;
  ClockSleep(1);
  IENR2.BIT.IENTB1 = 0;
  IENR1.BIT.IENRTC = 0;
  CaptureSample();
  IENR1.BIT.IENRTC = 1;
  InputScan();
  if ((g_state.flags.byte & SYSTEM_MODE_MASK) == 0) {
    RtcDispatch();
    if ((MotionActivityCheck() != 0) ||
        (g_state.events.bits.centerPressed != 0)) {
      MotionSessionWake();
    }
  } else {
    if ((g_state.flags.byte & SYSTEM_MODE_MASK) == SYSTEM_MODE_INTERACTIVE) {
      SocialOfferCheck();
      ViewUpdate();
      /* The dispatch-target gate lives inside the 0x10 arm: any other mode runs
       * the motion/flag work below ungated, while a foreground poll handler
       * skips straight to the ring-index tail. */
      if (g_task == IrProtocolTick) {
        goto tickTail;
      }
    }
    if ((g_state.flags.bits.registered != 0) &&
        (g_state.sampleIndex == PW_ACCEL_RING_LAST)) {
      MotionProcess();
    } else {
      if (g_state.events.bits.secondTick != 0) {
        if ((g_state.flags.byte & SYSTEM_MODE_MASK) ==
            SYSTEM_MODE_INTERACTIVE) {
          DisplayClear(0x40);
          ViewRender();
          DisplayToggleBank();
          g_state.uiFrame++;
        }
        g_state.events.bits.secondTick = 0;
      } else {
        RtcDispatch();
        if ((g_state.flags.byte & SYSTEM_MODE_MASK) ==
            SYSTEM_MODE_INTERACTIVE) {
          if (g_state.idleSeconds[0] == 0) {
            DisplayEnterPowerSave();
            g_state.flags.byte =
                ((g_state.flags.byte & SYSTEM_MODE_CLEAR) | SYSTEM_MODE_MOTION);
            g_state.buttonWake[0] = 0;
            g_state.centerHoldTicks = 0;
          }
        } else {
          set_ccr(0x80);
          BatteryUpdate();
          set_ccr(0);
          MotionSessionIdleCheck();
        }
      }
    }
    StepPacingTick();
    if (BeepHasScore() != 0) {
      InstallTask(BeepTick);
      IENR2.BIT.IENTB1 = 0;
      BeepEnableTimer();
    }
  }
tickTail:
  g_state.sampleIndex = ((g_state.sampleIndex + 1) & PW_ACCEL_RING_MASK);
}

/* Reduced tick while the beeper owns the foreground. */
void BeepTick(void)
{
  SYSCR1.BYTE = 0x27;
  SYSCR2.BYTE = 0xe0;
  g_state.events.bits.lowPowerClock = 1;
  if (TW.GRA != 0) {
    sleep();
  }
  WatchdogService();
  InputScan();
  ViewUpdate();
  if (g_state.events.bits.secondTick != 0) {
    if ((g_state.flags.byte & SYSTEM_MODE_MASK) == SYSTEM_MODE_INTERACTIVE) {
      DisplayClear(0x40);
      ViewRender();
      DisplayToggleBank();
      g_state.uiFrame++;
    }
    g_state.events.bits.secondTick = 0;
  }
  if (BeepHasScore() == 0) {
    BeepDisableTimer();
    InstallTask(MainTick);
    g_state.sampleIndex = 0;
  }
}

/* Feeling bubble for each social offer. */
const u8 g_socialBubbles[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x05};
