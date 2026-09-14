#include "flags.h"
#include "types.h"
#include "iodefine.h"
#include "project.h"
#include "diary.h"
#include "eeprom.h"
#include "rtc.h"
#include "scratch.h"

void PokemonMinuteTick(void);
void RtcHourUpdate(void);
void RtcDayRollover(void);

#define PW_STEP_COUNTER_DISPLAY_MAX 9999999ul
#define PW_ELAPSED_DAYS_MAX 9999
#define PW_SECONDS_PER_MINUTE 60
#define PW_MINUTES_PER_HOUR 60
#define PW_HOURS_PER_DAY 24
#define PW_RTC_ONE_HOUR_SECONDS 3600
#define PW_EEPROM_SAVE_DATA_PRIMARY EEPROM_SAVE_PRIMARY
#define PW_EEPROM_SAVE_DATA_BACKUP EEPROM_SAVE_BACKUP
#define PW_EEPROM_SAVE_DATA_LENGTH 0x18
#define PW_EEPROM_ACTIVE_COURSE_VIEW_BASE EEPROM_COURSE
#define PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH 0xBE
#define PW_EEPROM_DAILY_STEP_HISTORY_BASE 0xCEF0
#define PW_EEPROM_DAILY_STEP_HISTORY_LENGTH 0x1C
#define PW_EEPROM_DIARY_ENTRY_LENGTH 0x88
#define PW_EEPROM_PEER_HISTORY_BASE EEPROM_PEER_RECORDS
#define PW_EEPROM_PEER_HISTORY_STRIDE 0x224
#define PW_EEPROM_PEER_HISTORY_FILL_OFFSET 8
#define PW_EEPROM_PEER_HISTORY_FILL_LENGTH 0x28
#define PW_EEPROM_PEER_HISTORY_SLOT_COUNT 10
#define PW_DIARY_ACTION_PERIODIC_STEPS 0x1B
#define PW_RTC_PENDING_MINUTE 1
#define PW_RTC_PENDING_HOUR 2
#define PW_RTC_PENDING_DAY 4

/* Defer queued minute, hour, and day work while an IR request is pending.
 * Flags are reread after each handler so the hour path can queue day rollover.
 */
void RtcDispatch(void)
{
  SystemEvents flags;

  flags.byte = g_state.events.byte;
  if (!flags.bits.irRequested) {
    if ((g_state.time.pendingUpdates & PW_RTC_PENDING_MINUTE) != 0) {
      PokemonMinuteTick();
    }
    if ((g_state.time.pendingUpdates & PW_RTC_PENDING_HOUR) != 0) {
      RtcHourUpdate();
    }
    if ((g_state.time.pendingUpdates & PW_RTC_PENDING_DAY) != 0) {
      RtcDayRollover();
    }
    g_state.time.pendingUpdates &= 0xf8;
  }
}

/* Count a minute tick, saturating the Pokemon timer at 0xffff. */
void PokemonMinuteTick(void)
{
  if (g_state.save.pokemonMinutes != 0xffffu) {
    g_state.save.pokemonMinutes++;
  }
}

/* Raise the low-battery sample request, count a past hour when both totals are
 * still below the 7-digit display cap, persist the save record, optionally
 * write a periodic-steps diary entry, clear the hour step counter, and queue
 * day rollover when the live hour matches the configured reset hour. */
void RtcHourUpdate(void)
{
  u8 *course;

  g_state.events.byte |= EVENT_BATTERY_CHECK;
  if ((g_state.save.totalSteps < PW_STEP_COUNTER_DISPLAY_MAX) &&
      (g_state.save.elapsedHours < PW_STEP_COUNTER_DISPLAY_MAX)) {
    g_state.save.elapsedHours++;
  }
  EepromMirrorWrite(PW_EEPROM_SAVE_DATA_PRIMARY, PW_EEPROM_SAVE_DATA_BACKUP,
                    (u8 *)&g_state.save, PW_EEPROM_SAVE_DATA_LENGTH);

  if (g_state.flags.bits.hasPokemon) {
    ScratchReset();
    course = ScratchAlloc(PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
    EepromRead(PW_EEPROM_ACTIVE_COURSE_VIEW_BASE, course,
               PW_EEPROM_ACTIVE_COURSE_VIEW_LENGTH);
    DiaryAppend((Course *)course, ScratchAlloc(PW_EEPROM_DIARY_ENTRY_LENGTH),
                PW_DIARY_ACTION_PERIODIC_STEPS, g_state.save.bonusCourse, 0, 0);
  }

  g_state.hourSteps = 0;
  if (g_state.rolloverHourBcd == g_state.time.hourBcd24h) {
    g_state.time.pendingUpdates |= PW_RTC_PENDING_DAY;
  }
}

/* Advance the elapsed-day count, persist it, rotate the seven-day step history,
 * clear today's counter, and wipe ten peer-history slots. */
void RtcDayRollover(void)
{
  WalkData *data;
  u16 historyLength;
  u8 *history;
  u32 *source;
  u32 *destination;
  u8 shifted;
  PeerRecords *record;
  u8 remaining;

  data = (WalkData *)EEPROM_WALK;
  historyLength = sizeof(data->dailySteps);
  if (g_state.save.days < PW_ELAPSED_DAYS_MAX) {
    g_state.save.days++;
  }
  EepromMirrorWrite(PW_EEPROM_SAVE_DATA_PRIMARY, PW_EEPROM_SAVE_DATA_BACKUP,
                    (u8 *)&g_state.save, PW_EEPROM_SAVE_DATA_LENGTH);

  ScratchReset();
  history = ScratchAlloc(historyLength);
  EepromRead((u16)&data->dailySteps, history, historyLength);

  source = (u32 *)(history + 0x14);
  destination = (u32 *)(history + 0x18);
  shifted = 0;
  do {
    *destination = *source;
    shifted++;
    source--;
    destination--;
  } while (shifted < 6);
  *(u32 *)history = g_state.dailySteps;

  EepromWrite((u16)&data->dailySteps, history, historyLength);
  g_state.dailySteps = 0;

  record = (PeerRecords *)EEPROM_PEER_RECORDS;
  remaining = PW_EEPROM_PEER_HISTORY_SLOT_COUNT;
  do {
    record++;
    EepromFill((u16)&record->deviceId, sizeof(record->deviceId), 0xff);
    remaining--;
  } while (remaining != 0);
}

/* Convert seconds to packed 24-hour BCD. Write the RTC while it is stopped and
 * reset, then enable second, minute, hour and quarter-second interrupts. */
#pragma inline(BinaryToBcd)
static u8 BinaryToBcd(u8 value)
{
  u8 result = ((value / 10) * (u16)16);
  result |= value % 10;
  return result;
}

void RtcSetTime(u32 seconds)
{
  const u32 sixty = PW_SECONDS_PER_MINUTE;
  u8 secondBcd;
  u8 minuteBcd;
  u8 hourBcd;
  u8 rem;

  rem = (seconds % sixty);
  secondBcd = BinaryToBcd(rem);
  seconds = seconds / sixty;
  rem = (seconds % sixty);
  minuteBcd = BinaryToBcd(rem);
  seconds = seconds / sixty;
  rem = (seconds % PW_HOURS_PER_DAY);
  hourBcd = BinaryToBcd(rem);

  g_state.time.hourBcd24h = hourBcd;
  g_state.time.minuteBcd = minuteBcd;
  g_state.time.secondBcd = secondBcd;

  CKSTPR1.BIT.RTCCKSTP = 1;
  RTC.RTCCR1.BIT.RUN = 0;
  RTC.RTCCR1.BIT.RST = 1;
  RTC.RTCCR1.BIT.RST = 0;
  RTC.RSECDR.BYTE = secondBcd;
  RTC.RMINDR.BYTE = minuteBcd;
  RTC.RHRDR.BYTE = hourBcd;
  RTC.RTCCR1.BIT.HR24 = 1;
  RTC.RTCCR1.BIT.INT = 1;
  RTC.RTCCR2.BYTE = 0x1c;
  RTC.RTCCR2.BIT._025SEIE = 1;
  RTC.RTCCR1.BIT.RUN = 1;
}

/* Read H:M:S twice, reject a busy or mismatched pair, and store the stable
 * packed-BCD snapshot. */
void RtcReadStable(u8 *secondOut, u8 *minuteOut, u8 *hourOut)
{
  u8 snapshot[6];
  u8 sample;

  do {
    sample = 0;
    do {
      while (RTC.RSECDR.BIT.BSY) {
      }
      snapshot[sample * 3] = RTC.RSECDR.BYTE;
      while (RTC.RMINDR.BIT.BSY) {
      }
      snapshot[sample * 3 + 1] = RTC.RMINDR.BYTE;
      while (RTC.RHRDR.BIT.BSY) {
      }
      snapshot[sample * 3 + 2] = RTC.RHRDR.BYTE;
      sample++;
    } while (sample < 2);
  } while ((snapshot[0] != snapshot[3]) || (snapshot[1] != snapshot[4]) ||
           (snapshot[2] != snapshot[5]));

  *secondOut = snapshot[0];
  *minuteOut = snapshot[1];
  *hourOut = snapshot[2];
}

#pragma interrupt(RtcQuarterSecondInterrupt(vect = 23))
void RtcQuarterSecondInterrupt(void)
{
  g_state.events.byte |= EVENT_SECOND_TICK;
  RTC.RTCFLG.BYTE &= 0xfe;
}

#pragma interrupt(RtcHalfSecondInterrupt(vect = 24))
void RtcHalfSecondInterrupt(void)
{
  RTC.RTCFLG.BYTE &= 0xfd;
}

#pragma interrupt(RtcSecondInterrupt(vect = 25))
void RtcSecondInterrupt(void)
{
  u8 second;
  u16 gate;

  second = RTC.RSECDR.BYTE;
  if ((second & 0x80) == 0) {
    g_state.time.secondBcd = second;
  }
  g_state.save.rtcSeconds++;
  gate = (g_state.socialElapsedSeconds + 1);
  if (gate > PW_RTC_ONE_HOUR_SECONDS) {
    gate = PW_RTC_ONE_HOUR_SECONDS;
  }
  g_state.socialElapsedSeconds = gate;
  if (g_state.idleSeconds[0] != 0) {
    g_state.idleSeconds[0]--;
  }
  if (g_state.idleSeconds[1] != 0) {
    g_state.idleSeconds[1]--;
  }
  RTC.RTCFLG.BYTE &= 0xfb;
}

#pragma interrupt(RtcMinuteInterrupt(vect = 26))
void RtcMinuteInterrupt(void)
{
  u8 minute;

  minute = RTC.RMINDR.BYTE;
  if ((minute & 0x80) == 0) {
    g_state.time.minuteBcd = minute;
  }
  g_state.time.pendingUpdates |= PW_RTC_PENDING_MINUTE;
  RTC.RTCFLG.BYTE &= 0xf7;
}

#pragma interrupt(RtcHourInterrupt(vect = 27))
void RtcHourInterrupt(void)
{
  u8 hour;

  hour = RTC.RHRDR.BYTE;
  if ((hour & 0x80) == 0) {
    g_state.time.hourBcd24h = RTC.RHRDR.BYTE;
  }
  g_state.time.pendingUpdates |= PW_RTC_PENDING_HOUR;
  RTC.RTCFLG.BYTE &= 0xef;
}
