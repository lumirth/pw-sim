#include "types.h"
#include "iodefine.h"
#include "project.h"
#include "diary.h"
#include "accelerometer.h"
#include "battery.h"
#include "beep.h"
#include "display.h"
#include "eeprom.h"
#include "factory.h"
#include "ir.h"
#include "friend.h"
#include "home.h"
#include "interface.h"
#include "motion.h"
#include "pad.h"
#include "clear.h"
#include "common.h"
#include "rominfo.h"
#include "rtc.h"
#include "scratch.h"
#include "selftest.h"
#include "setup.h"
#include "startup.h"
#include "user.h"

/* The stack starts at the end of section S, above the static on-chip RAM
 * allocations. */
#pragma stacksize 0x8C
#include <machine.h>
#include <stddef.h>

/* Timer B1 wakes the foreground task from sleep. */
void TimerB1Init(void)
{
  CKSTPR1.BYTE |= 4; /* TB1CKSTP */
  TB1.TMB1.BYTE = 0xbf;
  TB1.TCB1 = 0xf8;
  IRR2.BYTE &= 0xfb;     /* clear IRRTB1 */
  IENR2.BYTE |= 4;       /* enable IENTB1 */
  TB1.TMB1.BYTE |= 0x40; /* start */
}

void WalkStartCommit(void);
void WalkEndClear(void);

void ClearWattsInventory(void);

/* Runtime section initialization copies initialized data and clears B. */
void _INITSCT(void);

#define PW_RTC_ONE_HOUR_SECONDS 3600
#define PW_IRC_RESULT_NONE 0
#define PW_EVENT_REWARD_POKEMON 0
#define PW_EVENT_REWARD_COURSE 1
#define PW_EVENT_REWARD_ITEM 2
#define PW_EVENT_REWARD_MAP 3
#define PW_EVENT_REWARD_STAMP0 4
#define PW_EVENT_REWARD_STAMP1 5
#define PW_EVENT_REWARD_STAMP2 6
#define PW_EVENT_REWARD_STAMP3 7
#define PW_STARTUP_RAM_CLEAR_BYTES 0x3e
#define PW_EEPROM_WATCHDOG_RESET_COUNT (EEPROM_DIAGNOSTIC_LOG + 2)
#define PW_PRNG_SEED_CROSS_RECORD 0x153
#define PW_PRNG_SEED_BYTES 4
#define PW_INTERACTIVE_IDLE_SECONDS 60
#define PW_MOTION_SESSION_UI_IDLE_SECONDS 90
#define PW_BATTERY_BOOT_SCALE 0x13

/* Dispatch the deferred application action: apply received settings, commit
 * or clear persistent records, transition walk sessions, and resume the
 * foreground application. */
void IrComplete(void)
{
  switch (g_work.irc.work.completionAction) {
  case IR_CMD_FACTORY_SETUP:
    DisplayInit();
    g_state.view = VIEW_DIAGNOSTICS;
    DiagnosticsInit();
    goto resumeForeground;
  case IR_CMD_MOTION_TEST_SETUP:
    g_state.view = VIEW_THRESHOLD_TEST;
    ThresholdInit();
    goto resumeForeground;
  case IR_ACTION_FACTORY_RESET:
    DisplayInit();
    g_state.socialElapsedSeconds = PW_RTC_ONE_HOUR_SECONDS;
    PersistentReset(1, 1);
    goto applyOutputs;
  case IR_CMD_RESET_ALL:
    g_state.socialElapsedSeconds = PW_RTC_ONE_HOUR_SECONDS;
    PersistentReset(0, 1);
    goto applyOutputs;
  case IR_CMD_RESET_KEEP_STEPS:
    g_state.socialElapsedSeconds = PW_RTC_ONE_HOUR_SECONDS;
    PersistentReset(0, 0);
  applyOutputs:
    BeepSetOutputMode(g_state.save.volume);
    DisplaySetContrast(g_state.save.contrast);
    goto returnHome;
  case IR_CMD_WALK_START_COMMIT:
    g_state.save.elapsedHours = 0;
    WalkStartCommit();
    goto enterWalkStart;
  case IR_CMD_WALK_END_COMMIT:
    WalkEndClear();
    SetView(VIEW_WALK_END);
    g_ui.view.presentation.stage = 5;
    goto clearPrimary;
  case IR_CMD_WALK_UPDATE_COMMIT:
    WalkStartCommit();
    EepromFill(EEPROM_EVENTS, 0x06c8, 0);
  enterWalkStart:
    SetView(VIEW_WALK_START);
    g_ui.view.presentation.stage = 0;
    g_ui.view.presentation.frame = 0;
    goto resumeForeground;
  case IR_CMD_GIFT_COLLECTION_COMMIT:
    ClearWattsInventory();
    SetView(VIEW_WALK_END);
    g_ui.view.presentation.stage = 6;
    goto clearPrimary;
  case IR_CMD_PEER_START:
    SetView(VIEW_PEER);
    PeerFinalize();
    goto resumeForeground;
  case IR_CMD_EVENT_MAP_DONE:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = 0;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_MAP;
    goto resumeForeground;
  case IR_CMD_EVENT_POKEMON_DONE:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = 0;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_POKEMON;
    goto resumeForeground;
  case IR_CMD_EVENT_ITEM_DONE:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = 0;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_ITEM;
    goto resumeForeground;
  case IR_CMD_EVENT_COURSE_DONE:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = 0;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_COURSE;
    goto resumeForeground;
  case IR_CMD_EVENT_STAMP0:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = 0;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_STAMP0;
    goto resumeForeground;
  case IR_CMD_EVENT_STAMP1:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = 0;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_STAMP1;
    goto resumeForeground;
  case IR_CMD_EVENT_STAMP2:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = 0;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_STAMP2;
    goto resumeForeground;
  case IR_CMD_EVENT_STAMP3:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = 0;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_STAMP3;
    goto resumeForeground;
  default:
    if (g_state.irResult == PW_IRC_RESULT_NONE)
      goto returnHome;
    SetView(VIEW_IR_RESULT);
    goto clearPrimary;
  }

clearPrimary:
  g_ui.view.presentation.frame = 0;
  goto resumeForeground;

returnHome:
  HomeInit();
  SetView(VIEW_HOME);
resumeForeground:
  g_state.sampleIndex = 0;
  MotionReset();
  InstallTask(MainTick);
  set_ccr(0);
  RtcReadStable(&g_state.time.secondBcd, &g_state.time.minuteBcd,
                &g_state.time.hourBcd24h);
}

/* Reset initializes the stack and runtime sections, brings up devices, updates
 * the watchdog reset count, initializes startup RAM and boot defaults, and
 * waits for a valid battery scale. After Timer B1 starts, repeatedly call the
 * installed foreground target. */
#pragma entry PowerOnReset(vect = 0)
void PowerOnReset(void)
{
  u32 seed;
  u8 *ram;
  u16 cleared;

  _INITSCT();
  HardwareSetup();
  AccelInit();

  if (WDT.TCSRWD1.BIT.WRST) {
    u8 watchdogResets;

    watchdogResets = EepromReadByte(PW_EEPROM_WATCHDOG_RESET_COUNT);
    watchdogResets++;
    EepromWriteByte(PW_EEPROM_WATCHDOG_RESET_COUNT, watchdogResets);
  }

  ram = (u8 *)&g_state.save;
  cleared = 0;
  do {
    *ram++ = 0;
    cleared++;
  } while (cleared < PW_STARTUP_RAM_CLEAR_BYTES);

  g_state.rolloverHourBcd = 0;
  g_state.events.byte |= EVENT_LOW_POWER_CLOCK;
  g_state.flags.byte =
      ((g_state.flags.byte & SYSTEM_MODE_CLEAR) | SYSTEM_MODE_INTERACTIVE);
  g_state.idleSeconds[0] = PW_INTERACTIVE_IDLE_SECONDS;
  g_state.idleSeconds[1] = PW_MOTION_SESSION_UI_IDLE_SECONDS;
  g_state.socialElapsedSeconds = PW_RTC_ONE_HOUR_SECONDS;
  MotionReset();
  BootSelfTest();
  WatchdogDisable();
  BootRestore();
  WatchdogStart();

  while (BatteryLow(PW_BATTERY_BOOT_SCALE) != 0)
    WatchdogService();

  BeepInit();
  BeepSetOutputMode(g_state.save.volume);
  DisplayInit();
  RtcRestore();
  EepromRead(PW_PRNG_SEED_CROSS_RECORD, &seed, PW_PRNG_SEED_BYTES);
  RandomSeed(seed);
  IrInit();
  InputInit();
  AccelInit();
  InstallTask(MainTick);
  HomeInit();
  g_state.view = VIEW_HOME;
  TimerB1Init();
  set_ccr(0);
  /* The browser schedules g_task at hardware event deadlines. */
}

/* Apply time configuration from the received DeviceStatus. Accept reset hours
 * below 24, convert them to BCD, and set RTC seconds only when supplied by the
 * DS. */
void StatusApplyTime(void)
{
  s16 hour;
  u32 seconds;
  u8 *payload = IrPayload();

  g_work.irc.statusA.status = *(DeviceStatus *)payload;
  if ((g_work.irc.statusA.bytes[STATUS_FLAGS_OFFSET] & 0xF8) < 0xC0) {
    hour = g_work.irc.statusA.status.rolloverHour;
    g_state.rolloverHourBcd = ((hour / 10) * (u16)16 | (hour % 10));
  }
  seconds = g_work.irc.statusA.status.rtcSeconds;
  if (seconds != 0) {
    g_state.save.rtcSeconds = seconds;
    RtcSetTime(g_work.irc.statusA.status.rtcSeconds);
  }
}

/* Scatter the staged walk package into the course view and following EEPROM
 * page run. */
void CommitStagedWalk(void)
{
  u16 page;
  u8 *buf;
  u16 source;
  union {
    u32 word32;
    struct {
      u16 pages;
      u16 destination;
    } w;
  } cursor;
  u16 remaining;

  page = 0x80;
  ScratchReset();
  {
    u16 allocSize;

    allocSize = page;
    buf = ScratchAlloc(allocSize);
  }
  source = EEPROM_COURSE_TEMP;
  cursor.w.pages = 0x52; cursor.w.destination = EEPROM_COURSE;
  remaining = cursor.w.pages;
  do {
    EepromRead(source, buf, page);
    EepromWrite(cursor.w.destination, buf, page);
    source = (source + page);
    cursor.w.destination = (cursor.w.destination + page);
    remaining--;
  } while (remaining != 0);
  source = EEPROM_RECORD_TEMP;
  cursor.w.pages = 5; cursor.w.destination = EEPROM_OWN_RECORDS;
  remaining = 0;
  while (remaining < cursor.w.pages) {
    EepromRead(source, buf, page);
    EepromWrite(cursor.w.destination, buf, page);
    source = (source + page);
    cursor.w.destination = (cursor.w.destination + page);
    remaining++;
  }
}

#define PW_WALK_COMMIT_MARKER 0xa5
#define PW_DIARY_ACTION_WALK_STARTED 0x19
#define PW_EEPROM_FILL_RECORD 0xde24
#define PW_EEPROM_FILL_RECORD_LEN 0x1568
#define PW_EEPROM_ACTIVE_COURSE_VIEW_BASE EEPROM_COURSE
#define PW_TRAINER_NAME_COPY_BYTES STATUS_NAME_BYTES
#define PW_EEPROM_DISTRIBUTION_BASE EEPROM_EVENTS
#define PW_EEPROM_DISTRIBUTION_LENGTH 0x06c8
#define PW_EEPROM_ACTIVE_COURSE_PREFIX 0x10

/* Commit a new walk session: course flags, diary reset, peer history, status,
 * and a walk-started diary entry. */
void WalkStartCommit(void)
{
  u8 marker;
  u8 courseClear;
  DeviceStatus *status;
  u8 *course;
  u8 encounter;

  marker = PW_WALK_COMMIT_MARKER;
  EepromMirrorWrite(EEPROM_COMMIT_PRIMARY, EEPROM_COMMIT_BACKUP, &marker, 1);
  CommitStagedWalk();
  courseClear = 0;
  EepromMirrorWrite(EEPROM_COMMIT_PRIMARY, EEPROM_COMMIT_BACKUP, &courseClear,
                    1);

  {
    enum { diaryEep = EEPROM_WALK + offsetof(WalkData, diary) };
    DiaryEntry *entry;
    u8 n;

    entry = (DiaryEntry *)diaryEep;
    n = 24;
    do {
      EepromWriteByte((u16)&entry->action, 0);
      entry++;
    } while (--n != 0);
  }

  EepromFill(PW_EEPROM_FILL_RECORD, PW_EEPROM_FILL_RECORD_LEN, 0);

  g_state.hourSteps = 0;
  g_state.save.pokemonMinutes = 0;
  g_state.save.diaryIndex = 0;
  g_state.save.watts = 0;
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));

  g_state.flags.bits.hasPokemon = 1;
  g_state.flags.bits.registered = 1;

  status = &g_work.irc.statusB.status;
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                   sizeof(DeviceStatus));
  status->registered = 1;
  status->hasPokemon = 1;
  status->generatedPokemon = 0;

  status->consoleCompatibilityLe =
      g_work.irc.statusA.status.consoleCompatibilityLe;
  status->pokemonCompatibilityLe =
      g_work.irc.statusA.status.pokemonCompatibilityLe;
  status->gameVersionLe = g_work.irc.statusA.status.gameVersionLe;
  status->pokemonGameVersionLe = g_work.irc.statusA.status.pokemonGameVersionLe;
  status->trainerIdLe = g_work.irc.statusA.status.trainerIdLe;
  {
    u8 i;

    i = 0;
    do {
      status->trainerNameData[i] = g_work.irc.statusA.status.trainerNameData[i];
      i++;
    } while (i < PW_TRAINER_NAME_COPY_BYTES);
  }
  status->peerProtocol = g_work.irc.statusA.status.peerProtocol;
  status->consoleProtocolLevel = g_work.irc.statusA.status.consoleProtocolLevel;
  status->firmwareCompatibility = FIRMWARE_COMPATIBILITY;
  status->firmwareRevision = FIRMWARE_REVISION;
  EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                    sizeof(DeviceStatus));

  ScratchReset();
  course = ScratchAlloc(sizeof(Course));
  EepromRead(PW_EEPROM_ACTIVE_COURSE_VIEW_BASE, course, sizeof(Course));
  encounter = 0;
  DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
              PW_DIARY_ACTION_WALK_STARTED, g_state.save.bonusCourse, encounter,
              0);
  ClearReturnInventory();
}

/* Tear down the walk session. */
void WalkEndClear(void)
{
  DeviceStatus *status;

  status = &g_work.irc.statusA.status;
  g_state.flags.bits.hasPokemon = 0;
  g_state.save.bonusCourse = 0;
  g_state.save.diaryIndex = 0;
  g_state.save.watts = 0;
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                   sizeof(DeviceStatus));
  status->pokemonCompatibilityLe = 0;
  status->pokemonGameVersionLe = 0;
  status->hasPokemon = 0;
  status->generatedPokemon = 0;
  EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                    sizeof(DeviceStatus));
  ClearReturnInventory();
  ClearDiaryActions();
  EepromFill(PW_EEPROM_DISTRIBUTION_BASE, PW_EEPROM_DISTRIBUTION_LENGTH, 0);
  EepromFill(PW_EEPROM_FILL_RECORD, PW_EEPROM_FILL_RECORD_LEN, 0);
  {
    CourseResources *course;

    course = (CourseResources *)EEPROM_COURSE;
    EepromFill((u16)&course->values.pokemon, sizeof(course->values.pokemon), 0);
  }
}

/* Return the walk inventory: zero Watts, persist both save mirrors, and clear
 * the Pokemon and item slots. */
void ClearWattsInventory(void)
{
  g_state.save.watts = 0;
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
  ClearReturnInventory();
}

/* Acknowledge the Timer B1 wake request. */
#pragma interrupt(TimerB1Interrupt(vect = 33))
void TimerB1Interrupt(void)
{
  IRR2.BIT.IRRTB1 = 0;
}
