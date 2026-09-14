#include "fft.h"
#include "types.h"
#include "raster_column.h"
#include "eeprom_address.h"
#include "project.h"
#include "diary.h"
#include "display.h"
#include <machine.h>
#include "battle.h"
#include "beep.h"
#include "discard.h"
#include "eeprom.h"
#include "home.h"
#include "pad.h"
#include "common.h"
#include "scratch.h"

typedef struct {
  u8 hitPercent;
  u8 criticalPercent;
  u8 avoidPercent;
} BattleOutcomeWeightRow;

/* Battle view of the shared UI state. The player's travelling Pokemon enters at
 * x=56; the encounter enters from the left. Capture uses the opponent's HP and
 * x position. */

extern const u8 g_battleOutcomeWeights5x3[];
extern const u8 g_battleCaptureThresholdPercent5[];
extern const s8 g_battleOpeningSpriteXS84[];

typedef struct {
  s8 playerX;
  s8 opponentX;
} BattleParticipantPositions;

extern const BattleParticipantPositions g_battleActionAnimationAXy9[];
extern const BattleParticipantPositions g_battleActionAnimationBXy9[];

void ProcessTurn(void);
u8 CaptureRoll(void);
void CommitCapture(void);

#define PW_EEPROM_SAVEDATA_PRIMARY EEPROM_SAVE_PRIMARY
#define PW_EEPROM_SAVEDATA_BACKUP EEPROM_SAVE_BACKUP
#define PW_EEPROM_MIRROR_WRITE_LENGTH sizeof(SaveData)
#define PW_EEPROM_ACTIVE_COURSE_VIEW_BASE EEPROM_COURSE
#define PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH sizeof(Course)
#define PW_EEPROM_DAILY_EVENT_FLAGS EEPROM_EVENTS
#define PW_DIARY_RECORD_SIZE sizeof(DiaryEntry)
#define BATTLE_FRAME_READ_BYTES 384
#define PW_BATTLE_DRAW_MODULUS 100
#define PW_BATTLE_WATT_LOSS_CAP 10
#define PW_DIARY_ACTION_CAPTURED_ROUTE_POKEMON 0x0d
#define PW_DIARY_ACTION_CAPTURED_EVENT_POKEMON 0x0e
#define PW_DIARY_ACTION_BATTLE_FLED 0x0f
#define PW_DIARY_ACTION_BATTLE_DEFEATED 0x10
#define PW_BATTLE_EVENT_SPRITE_BYTES 0x170

/* Reset battle substates and start the encounter beeper. */
void BattleInit(void)
{
  u8 zero;
  u8 four;

  zero = 0;
  g_ui.view.battle.substate = zero;
  four = 4;
  g_ui.view.battle.playerHp = four;
  g_ui.view.battle.opponentHp = four;
  g_ui.view.battle.frame = zero;
  g_ui.view.battle.limit = 6;
  g_ui.view.battle.playerX = 0x38;
  g_ui.view.battle.opponentX = 0xe0;
  g_ui.view.battle.flags.byte &= 0x1e;
  g_ui.view.battle.captureCount = zero;
  BeepLoadScore(10);
}

/* Draw one enemy outcome and route attack/escape/ball. */
void ProcessTurn(void)
{
  u8 roll;

  if (BeepHasScore() != 0) {
    return;
  }

  roll = ((RandomNext() >> 3) % 100ul);

  if (roll <
      g_battleOutcomeWeights5x3[g_ui.view.battle.flags.bits.stage * 3u + 2]) {
    g_ui.view.battle.flags.bits.outcome = 2;
  } else if (roll < (s16)(g_battleOutcomeWeights5x3
                              [g_ui.view.battle.flags.bits.stage * 3u + 2] +
                          g_battleOutcomeWeights5x3
                              [g_ui.view.battle.flags.bits.stage * 3u + 1])) {
    g_ui.view.battle.flags.bits.outcome = 1;
  } else {
    g_ui.view.battle.flags.bits.outcome = 0;
  }

  if (InputPressed(BUTTON_LEFT) != 0) {
    g_ui.view.battle.flags.bits.action = 0;
    switch (g_ui.view.battle.flags.bits.outcome) {
    case 0:
    case 2:
    case 1:
      g_ui.view.battle.substate = 3;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 8;
      return;
    default:
      break;
    }
  }

  if (InputPressed(BUTTON_RIGHT) != 0) {
    g_ui.view.battle.flags.bits.action = 1;
    switch (g_ui.view.battle.flags.bits.outcome) {
    case 2:
      BeepLoadScore(0x0e);
      g_ui.view.battle.substate = 7;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 10;
      return;
    case 1:
      BeepLoadScore(0);
      g_ui.view.battle.substate = 8;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 6;
      return;
    case 0:
      g_ui.view.battle.substate = 4;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 8;
      return;
    }
  }

  if (InputPressed(BUTTON_CENTER) != 0) {
    BeepLoadScore(0x0f);
    g_ui.view.battle.substate = 10;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 6;
  }
}

/* Store a captured Pokémon or start discard. */
void CommitCapture(void)
{
  u8 encounter;
  Pokemon *pokemon;
  u8 freeSlot;
  u8 *course;
  DeviceStatus *record;
  u8 bitIndex;
  u8 dailyFlags;
  u8 *bundle;
  u16 spriteBytes;

  encounter = (g_ui.view.battle.encounter - 1);
  if (encounter > 3) {
    return;
  }

  if (encounter < 3) {
    ScratchReset();
    pokemon = ScratchAlloc((sizeof(Pokemon) * 3));
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon),
               pokemon, (sizeof(Pokemon) * 3));
    freeSlot = PokemonSlotFindEmpty(pokemon);
    if (freeSlot >= 3) {
      g_ui.view.discard.inventoryKind = PW_DISCARD_INVENTORY_POKEMON;
      DiscardInit();
      SetView(VIEW_DISCARD);
      return;
    }
    EepromRead(
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, encounters[encounter]),
        &pokemon[freeSlot], sizeof(Pokemon));
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon),
                pokemon, (sizeof(Pokemon) * 3));
    course = ScratchAlloc(sizeof(Course));
    EepromRead(EEPROM_COURSE, course, sizeof(Course));
    DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                PW_DIARY_ACTION_CAPTURED_ROUTE_POKEMON, 0,
                g_ui.view.battle.encounter, 0);
    return;
  }

  ScratchReset();
  record = ScratchAlloc(0x68);
  bitIndex = EepromReadByte(PW_EEPROM_MEMBER_ADDRESS(
      EEPROM_BONUS_COURSE, BonusResources, values.pokemonReceipt));
  if (StatusHasReceived(record, bitIndex) != 0) {
    return;
  }
  dailyFlags = EepromReadByte(PW_EEPROM_DAILY_EVENT_FLAGS);
  if ((dailyFlags & 0x20) != 0) {
    return;
  }
  spriteBytes = PW_BATTLE_EVENT_SPRITE_BYTES;
  bundle = ScratchAlloc(spriteBytes);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      values.pokemon),
             bundle, sizeof(Pokemon));
  EepromWrite(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, pokemon),
      bundle, sizeof(Pokemon));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      values.metadata),
             bundle, sizeof(PokemonMetadata));
  EepromWrite(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, metadata),
      bundle, sizeof(PokemonMetadata));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      pokemonImage),
             bundle, spriteBytes);
  EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon,
                                       pokemonImage),
              bundle, spriteBytes);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      pokemonName),
             bundle, 0x140);
  EepromWrite(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, pokemonName),
      bundle, 0x140);
  EepromWriteByte(PW_EEPROM_DAILY_EVENT_FLAGS, (dailyFlags | 0x20));
  StatusSetReceived(record, bitIndex);
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
              PW_DIARY_ACTION_CAPTURED_EVENT_POKEMON, 1,
              g_ui.view.battle.encounter, 0);
}

/* PRNG remainder versus the remaining-meter threshold. */

u8 CaptureRoll(void)
{
  u8 roll;
  u8 meter;

  roll = ((RandomNext() >> 3) % 100ul);
  meter = g_ui.view.battle.opponentHp;
  if (meter != 0) {
    if (roll < g_battleCaptureThresholdPercent5[(u16)meter - 1]) {
      return 1;
    }
  }
  return 0;
}

/* Eighteen-substate battle update. */
void BattleUpdate(void)
{
  u8 *course;

  if (g_ui.view.battle.substate == 2) {
    ProcessTurn();
    return;
  }

  switch (g_ui.view.battle.substate) {
  case 0:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.substate = 1;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 3;
    break;
  case 1:
    g_ui.view.battle.opponentX =
        g_battleOpeningSpriteXS84[g_ui.view.battle.frame];
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.flags.bits.hpVisible = 1;
    if (BeepHasScore() != 0) {
      return;
    }
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    g_ui.view.battle.substate = 2;
    break;
  case 3:
    g_ui.view.battle.playerX =
        g_battleActionAnimationAXy9[g_ui.view.battle.frame].playerX;
    g_ui.view.battle.opponentX =
        g_battleActionAnimationAXy9[g_ui.view.battle.frame].opponentX;
    if (BeepHasScore() != 0) {
      return;
    }
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.playerX = 0x38;
    g_ui.view.battle.opponentX = 8;
    switch (g_ui.view.battle.flags.bits.outcome) {
    case 0:
      if (g_ui.view.battle.opponentHp <= 1) {
        goto enemyDefeated;
      }
      g_ui.view.battle.opponentHp--;
      switch (g_ui.view.battle.flags.bits.action) {
      case 0:
        break;
      case 1:
        g_ui.view.battle.substate = 2;
        g_ui.view.battle.flags.bits.stage = 1;
        return;
      default:
        return;
      }
      break;
    case 1:
      break;
    case 2:
      if (g_ui.view.battle.opponentHp <= 2) {
        goto enemyDefeated;
      }
      g_ui.view.battle.opponentHp -= 2;
      break;
    default:
      return;
    }
    g_ui.view.battle.substate = 4;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 8;
    break;
  enemyDefeated:
    g_ui.view.battle.opponentHp = 0;
    BeepLoadScore(0x0e);
    g_ui.view.battle.substate = 7;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 10;
    return;
  case 4:
    g_ui.view.battle.playerX =
        g_battleActionAnimationBXy9[g_ui.view.battle.frame].playerX;
    g_ui.view.battle.opponentX =
        g_battleActionAnimationBXy9[g_ui.view.battle.frame].opponentX;
    if (BeepHasScore() != 0) {
      return;
    }
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.playerX = 0x38;
    g_ui.view.battle.opponentX = 8;
    switch (g_ui.view.battle.flags.bits.action) {
    case 0:
      g_ui.view.battle.playerHp--;
      if (g_ui.view.battle.playerHp != 0) {
        switch (g_ui.view.battle.flags.bits.outcome) {
        case 0:
          g_ui.view.battle.flags.bits.stage = 1;
          break;
        case 1:
          g_ui.view.battle.flags.bits.stage = 3;
          break;
        case 2:
          g_ui.view.battle.flags.bits.stage = 2;
          break;
        default:
          break;
        }
        g_ui.view.battle.substate = 2;
        break;
      }
      g_ui.view.battle.substate = 5;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 6;
      return;
    case 1:
      g_ui.view.battle.substate = 3;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 8;
      return;
    default:
      return;
    }
    break;
  case 5:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    ScratchReset();
    course = ScratchAlloc(sizeof(Course));
    EepromRead(EEPROM_COURSE, course, sizeof(Course));
    if (g_ui.view.battle.encounter < 4) {
      DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                  PW_DIARY_ACTION_BATTLE_DEFEATED, 0,
                  g_ui.view.battle.encounter, 0);
    } else {
      DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                  PW_DIARY_ACTION_BATTLE_DEFEATED, 1,
                  g_ui.view.battle.encounter, 0);
    }
    if (g_state.save.watts < PW_BATTLE_WATT_LOSS_CAP) {
      g_ui.view.battle.wattLoss = g_state.save.watts;
    } else {
      g_ui.view.battle.wattLoss = PW_BATTLE_WATT_LOSS_CAP;
    }
    g_state.save.watts = (g_state.save.watts - g_ui.view.battle.wattLoss);
    EepromMirrorWrite(PW_EEPROM_SAVEDATA_PRIMARY, PW_EEPROM_SAVEDATA_BACKUP,
                      (u8 *)&g_state.save, PW_EEPROM_MIRROR_WRITE_LENGTH);
    g_ui.view.battle.substate = 6;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 8;
    break;
  case 6:
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    HomeInit();
    SetView(VIEW_HOME);
    break;
  case 8:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.substate = 2;
    g_ui.view.battle.flags.bits.stage = 4;
    break;
  case 7:
    if (g_ui.view.battle.frame <= 3) {
      g_ui.view.battle.opponentX =
          g_battleOpeningSpriteXS84[3 - g_ui.view.battle.frame];
    } else {
      g_ui.view.battle.opponentX = 0xe0;
    }
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    ScratchReset();
    course = ScratchAlloc(sizeof(Course));
    EepromRead(EEPROM_COURSE, course, sizeof(Course));
    if (g_ui.view.battle.encounter < 4) {
      DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                  PW_DIARY_ACTION_BATTLE_FLED, 0, g_ui.view.battle.encounter,
                  0);
    } else {
      DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                  PW_DIARY_ACTION_BATTLE_FLED, 1, g_ui.view.battle.encounter,
                  0);
    }
    BeepLoadScore(4);
    HomeInit();
    SetView(VIEW_HOME);
    break;
  case 10:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.flags.bits.hpVisible = 0;
    g_ui.view.battle.substate = 0x0b;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 2;
    break;
  case 0x0b:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.substate = 0x0c;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 3;
    break;
  case 0x0c:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.substate = 0x0d;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 4;
    break;
  case 0x0d:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    if (CaptureRoll() != 0) {
      g_ui.view.battle.captureCount++;
      g_ui.view.battle.substate = 0x0e;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 4;
    } else {
      g_ui.view.battle.substate = 0x11;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 1;
      return;
    }
    break;
  case 0x0e:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    if (g_ui.view.battle.captureCount < 3) {
      g_ui.view.battle.substate = 0x0d;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 4;
    } else {
      g_ui.view.battle.substate = 0x0f;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 6;
      BeepLoadScore(0);
    }
    break;
  case 0x0f:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.substate = 0x10;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 6;
    BeepLoadScore(7);
    break;
  case 0x10:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    CommitCapture();
    if (g_state.view != VIEW_BATTLE) {
      return;
    }
    HomeInit();
    SetView(VIEW_HOME);
    break;
  case 0x11:
    if (g_ui.view.battle.frame < g_ui.view.battle.limit) {
      return;
    }
    g_ui.view.battle.substate = 9;
    g_ui.view.battle.frame = 0;
    g_ui.view.battle.limit = 6;
    break;
  case 9:
    if (g_ui.view.battle.frame >= g_ui.view.battle.limit) {
      BeepLoadScore(0x0e);
      g_ui.view.battle.substate = 7;
      g_ui.view.battle.frame = 0;
      g_ui.view.battle.limit = 10;
    }
    break;
  default:
    break;
  }
}

/* Battle sprites, names, messages, and frame advance. */
void BattleRender(void)
{
  const UiResources *image;
  const u8 *picture;
  u8 *raster;
  const u8 *ball;
  u8 *ballStorage;
  u8 *mask;
  u8 i;
  u8 outcome;
  u8 action;
  u8 phase;

  image = (const UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(0x300);
  ballStorage = ScratchAlloc(0x18);
  /* Loading uses writable storage; the compositor consumes its read view. */
  ball = ballStorage;
  mask = ScratchAlloc(8);
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                               pokemonImage[(g_state.uiFrame & 1) * 0xc0]),
      raster, 0xc0);
  DisplayBlit(g_ui.view.battle.playerX, 0x08, 0x20, 0x18, raster);
  picture = image->battleMeter;
  EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->battleMeter));
  i = 0;
  while (i < g_ui.view.battle.playerHp) {
    DisplayBlit(i * 8 + 0x38, 0, 0x08, 0x08, raster);
    i++;
  }
  if (g_ui.view.battle.flags.bits.hpVisible) {
    i = 0;
    while (i < g_ui.view.battle.opponentHp) {
      DisplayBlit(i * 8 + 8, 0x18, 0x08, 0x08, raster);
      i++;
    }
  }

  if (g_ui.view.battle.substate <= 10) {

    u8 fixedFacing;
    u16 imageAddress;

    if (g_ui.view.battle.encounter < 4) {
      EepromRead(
          PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course,
                                   encounters[g_ui.view.battle.encounter - 1]),
          raster, sizeof(Pokemon));
      fixedFacing = ((Pokemon *)raster)->fixedFacing;
      imageAddress = PW_EEPROM_MEMBER_ADDRESS(
          EEPROM_COURSE, CourseResources,
          encounterImages[(g_ui.view.battle.encounter - 1) *
                              POKEMON_ANIMATION_BYTES +
                          (g_state.uiFrame & 1) * POKEMON_FRAME_BYTES]);
    } else {
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                          values.pokemon),
                 raster, sizeof(Pokemon));
      fixedFacing = ((Pokemon *)raster)->fixedFacing;
      imageAddress = PW_EEPROM_MEMBER_ADDRESS(
          EEPROM_BONUS_COURSE, BonusResources,
          pokemonImage[(g_state.uiFrame & 1) * POKEMON_FRAME_BYTES]);
    }
    /* The transfer extends beyond the displayed 32-by-24 frame. */
    EepromRead(imageAddress, raster, BATTLE_FRAME_READ_BYTES);
    if (fixedFacing == 0) {
      RasterMirror(0x20, 0x18, raster);
    }
    if (g_ui.view.battle.substate != 10) {
      DisplayWriteSpan(g_ui.view.battle.opponentX, 0, 0x20, 0x18,
                       (RasterColumn *)raster);
    } else {
      s16 verticalOffset;

      phase = (g_ui.view.battle.frame << 2);
      i = (0x2c - phase);
      /* Scale the signed Q11 coordinate as a word before narrowing it to a
       * pixel coordinate. */
      verticalOffset = g_sineQ11[phase] >> 7;
      action = (0x14 - verticalOffset);
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, ball),
                 ballStorage, sizeof(((UiResources *)0)->ball));
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, ballMask),
                 mask, sizeof(((UiResources *)0)->ballMask));
      DisplayBlit(i, action, 0x08, 0x08, ballStorage);
      RasterMasked(raster, 0x20, 0x18, ball, mask,
                   (i - g_ui.view.battle.opponentX), action, 8, 8);
      DisplayBlit(g_ui.view.battle.opponentX, 0, 0x20, 0x18, raster);
    }
  }

  switch (g_ui.view.battle.substate) {
  case 0: {
    u8 frame = g_ui.view.battle.frame;
    u8 height;

    if (frame < 3) {
      DisplayFillRect(0, 0, 0x60, height = (3 - frame) * 8, 3);
      DisplayFillRect(0, (0x40 - height), 0x60, height, 3);
    }
  } break;
  case 1:
    if (g_ui.view.battle.frame >= g_ui.view.battle.limit) {
      if (BeepHasScore() == 0) {
        if (g_ui.view.battle.encounter < 4) {
          RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1), 5);
        } else {
          RenderBonusName(0x00, 0x20, 5);
        }
        RenderMessage(0x30, MESSAGE_BATTLE_ENCOUNTER, 0x0e, 1);
      }
    }
    break;
  case 2:
    picture = image->battleMenu;
    EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->battleMenu));
    DisplayBlit(0x00, 0x20, 0x60, 0x20, raster);
    break;
  case 3:
    outcome = g_ui.view.battle.flags.bits.outcome;
    switch (outcome) {
    case 0:
      if (g_ui.view.battle.frame == 4) {
        BeepLoadScore(0x0b);
        EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, attack),
                   raster, sizeof(((UiResources *)0)->attack));
        DisplayBlit(0x28, 0x00, 0x10, 0x20, raster);
      }
      if (g_ui.view.battle.frame >= 4) {
        RenderHeldName(0x00, 0x20, 5);
        RenderMessage(0x30, MESSAGE_BATTLE_ATTACK, 0x0e, 0);
      }
      break;
    case 1:
      if (g_ui.view.battle.frame == 4) {
        BeepLoadScore(0x0c);
      }
      if (g_ui.view.battle.frame >= 4) {
        if (g_ui.view.battle.encounter < 4) {
          RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1), 5);
        } else {
          RenderBonusName(0x00, 0x20, 5);
        }
        RenderMessage(0x30, MESSAGE_BATTLE_DODGE, 0x0e, 0);
      }
      break;
    case 2:
      if (g_ui.view.battle.frame == 4) {
        BeepLoadScore(0x0d);
        EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, weakPoint),
                   raster, sizeof(((UiResources *)0)->weakPoint));
        DisplayBlit(0x28, 0x00, 0x10, 0x20, raster);
      }
      if (g_ui.view.battle.frame >= 4) {
        RenderMessage(0x20, MESSAGE_BATTLE_CRITICAL, 0x0d, 0);
        RenderMessage(0x30, MESSAGE_BATTLE_HIT, 0x0e, 0);
      }
      break;
    }
    break;
  case 4:
    action = g_ui.view.battle.flags.bits.action;
    switch (action) {
    case 0:
      if (g_ui.view.battle.frame == 4) {
        BeepLoadScore(0x0b);
        EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, attack),
                   raster, sizeof(((UiResources *)0)->attack));
        DisplayBlit(0x28, 0x00, 0x10, 0x20, raster);
      }
      if (g_ui.view.battle.frame < 4) {
        break;
      }
      if (g_ui.view.battle.encounter < 4) {
        RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1), 5);
      } else {
        RenderBonusName(0x00, 0x20, 5);
      }
      RenderMessage(0x30, MESSAGE_BATTLE_ATTACK, 0x0e, 0);
      break;
    case 1:
      if (g_ui.view.battle.frame == 4) {
        BeepLoadScore(0x0c);
      }
      if (g_ui.view.battle.frame < 4) {
        break;
      }
      RenderHeldName(0x00, 0x20, 5);
      RenderMessage(0x30, MESSAGE_BATTLE_DODGE, 0x0e, 0);
      break;
    }
    break;
  case 5:
    if (g_ui.view.battle.encounter < 4) {
      RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1), 5);
    } else {
      RenderBonusName(0x00, 0x20, 5);
    }
    RenderMessage(0x30, MESSAGE_BATTLE_DEFEAT, 0x0e, 0);
    break;
  case 6:
    RenderDecoratedNumber(0x02, 0x20, g_ui.view.battle.wattLoss, 0x0d);
    RenderMessage(0x30, MESSAGE_BATTLE_DROPPED, 0x0e, 1);
    break;
  case 8:
    RenderMessage(0x30, MESSAGE_BATTLE_STARE, 0x0f, 0);
    break;
  case 7:
    if (g_ui.view.battle.frame > 3) {
      if (g_ui.view.battle.encounter < 4) {
        RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1), 5);
      } else {
        RenderBonusName(0x00, 0x20, 5);
      }
      RenderMessage(0x30, MESSAGE_BATTLE_ESCAPE, 0x0e, 1);
    }
    break;
  case 10:
    RenderMessage(0x30, MESSAGE_BATTLE_THROW, 0x0f, 0);
    break;
  case 0x0b:
    picture = image->gas;
    EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->gas));
    DisplayBlit(8, 0, 0x20, 0x18, raster);
    break;
  case 0x0c:
    picture = image->ball;
    EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(0x14, 0x0c, 0x08, 0x08, raster);
    break;
  case 0x0d: {
    u8 x;

    switch (g_state.uiFrame & 3) {
    case 0:
      x = 0x13;
      break;
    case 1:
      x = 0x14;
      break;
    case 2:
      x = 0x15;
      break;
    case 3:
      x = 0x14;
      break;
    }
    picture = image->ball;
    EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(x, 0x0c, 0x08, 0x08, raster);
    break;
  }
  case 0x0e:
    picture = image->ball;
    EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(0x14, 0x0c, 0x08, 0x08, raster);
    break;
  case 0x0f:
    picture = image->ball;
    EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(0x14, 0x0c, 0x08, 0x08, raster);
    picture = image->star;
    EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->star));
    {
      s8 x, y;

      y = 0x0a - g_ui.view.battle.frame * 2;
      x = 0x0c - g_ui.view.battle.frame;
      DisplayBlit(x, y, 8, 8, raster);
      y = 0x0c - g_ui.view.battle.frame * 2;
      x = g_ui.view.battle.frame + 0x1c;
      DisplayBlit(x, y, 8, 8, raster);
    }
    break;
  case 0x10:
    picture = image->ball;
    EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(0x14, 0x0c, 0x08, 0x08, raster);
    if (g_ui.view.battle.encounter < 4) {
      RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1), 5);
    } else {
      RenderBonusName(0x00, 0x20, 5);
    }
    RenderMessage(0x30, MESSAGE_BATTLE_CAPTURED, 0x0e, 1);
    break;
  case 0x11:
    picture = image->gas;
    EepromRead((u16)picture, raster, sizeof(((UiResources *)0)->gas));
    DisplayBlit(8, 0, 0x20, 0x18, raster);
    break;
  case 9:
    RenderMessage(0x30, MESSAGE_BATTLE_ALMOST, 0x0f, 0);
    break;
  default:
    break;
  }

  RenderBattery(0, 0);
  g_ui.view.battle.frame++;

  if (g_ui.view.battle.frame >= g_ui.view.battle.limit) {
    g_ui.view.battle.frame = g_ui.view.battle.limit;
  }
}

/* opening sprite x positions */
const s8 g_battleOpeningSpriteXS84[4] = {-16, -4, 8, 8};

/* action animation A frame x,y */
const BattleParticipantPositions g_battleActionAnimationAXy9[9] = {
    {56, 8}, {56, 8}, {54, 8}, {52, 8}, {53, 0},
    {54, 0}, {55, 4}, {56, 8}, {56, 8}};

/* action animation B frame x,y */
const BattleParticipantPositions g_battleActionAnimationBXy9[9] = {
    {56, 8},  {56, 8},  {56, 10}, {56, 12}, {64, 12},
    {64, 11}, {64, 10}, {60, 9},  {56, 8}};

/* outcome weights per stage: hit, critical, avoid percent */
const u8 g_battleOutcomeWeights5x3[15] = {45, 35, 20, 40, 30, 30, 50, 40,
                                          10, 60, 30, 10, 20, 30, 50};

/* capture threshold percent per stage */
const u8 g_battleCaptureThresholdPercent5[5] = {0x61, 0x4f, 0x42, 0x38, 0x00};
