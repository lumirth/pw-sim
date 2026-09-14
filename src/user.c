#include "types.h"
#include "project.h"
#include "beep.h"
#include "display.h"
#include "eeprom.h"
#include "ir.h"
#include "clear.h"
#include "common.h"
#include "rominfo.h"
#include "rtc.h"
#include "scratch.h"
#include "selftest.h"
#include "startup.h"
#include "user.h"

#define PW_SAVE_RTC_DEFAULT_SECONDS 0x0d2b0b80UL
#define PW_SAVE_DEFAULT_VOLUME 2
#define PW_SAVE_DEFAULT_CONTRAST 4
#define PW_SAVE_CONTRAST_MAX 9
#define PW_TRAINER_NAME_COPY_BYTES STATUS_NAME_BYTES
#define PW_WALK_COMMIT_MARKER 0xa5
#define PW_EEPROM_FILL_EVENT EEPROM_EVENTS
#define PW_EEPROM_FILL_EVENT_LEN 0x06c8
#define PW_EEPROM_FILL_RECORD 0xde24
#define PW_EEPROM_FILL_RECORD_LEN 0x1568

/* Reset lifetime fields only when requested. Always clear Watts, Pokemon
 * minutes and diary cursors, clear the bonus flag, restore volume and contrast
 * defaults, and commit both save mirrors. */
void SaveReset(u8 resetLifetimeFields)
{
  if (resetLifetimeFields != 0) {
    g_state.save.totalSteps = 0;
    g_state.save.days = 0;
    g_state.save.rtcSeconds = PW_SAVE_RTC_DEFAULT_SECONDS;
    g_state.save.elapsedHours = 0;
  }
  g_state.save.pokemonMinutes = 0;
  g_state.save.watts = 0;
  g_state.save.stepsTowardNextWatt = 0;
  g_state.save.bonusCourse = 0;
  g_state.save.volume = PW_SAVE_DEFAULT_VOLUME;
  g_state.save.contrast = PW_SAVE_DEFAULT_CONTRAST;
  g_state.save.diaryIndex = 0;
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
}

/* Restore the paired and Pokemon-present flags from persistent status. */
void StatusRestoreFlags(void)
{
  DeviceStatus *status;

  ScratchReset();
  status = ScratchAlloc(sizeof(DeviceStatus));
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                   sizeof(DeviceStatus));
  g_state.flags.bits.registered = status->registered;
  g_state.flags.bits.hasPokemon = status->hasPokemon;
}

/* The first reset flag clears event receipts and distribution data. The second
 * clears lifetime steps and WalkData. With both clear, preserve history and
 * clear only identity, session fields and diary action IDs. */
void PersistentReset(u8 factoryClearRequested,
                     u8 clearLifetimeProgressRequested)
{
  DeviceStatus *status;
  u16 i;

  g_state.dailySteps = 0;
  g_state.hourSteps = 0;
  g_state.flags.bits.registered = 0;
  g_state.flags.bits.hasPokemon = 0;
  status = (DeviceStatus *)IrPayload();
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                   sizeof(DeviceStatus));
  status->consoleCompatibilityLe = 0;
  status->pokemonCompatibilityLe = 0;
  status->gameVersionLe = 0;
  status->pokemonGameVersionLe = 0;
  status->trainerIdLe = 0;
  i = 0;
  do {
    status->trainerNameData[i] = 0;
    i++;
  } while (i < PW_TRAINER_NAME_COPY_BYTES);
  if (factoryClearRequested != 0) {
    i = 0;
    do {
      status->receivedEvents[i] = 0;
      i++;
    } while (i < 0x10);
  }
  status->registered = 0;
  status->hasPokemon = 0;
  status->generatedPokemon = 0;
  status->rolloverHour = 0;
  status->receiptIndex = 0;
  status->firmwareCompatibility = FIRMWARE_COMPATIBILITY;
  status->firmwareRevision = FIRMWARE_REVISION;
  status->rtcSeconds = 0;
  EepromMirrorRead(EEPROM_ID_PRIMARY, EEPROM_ID_BACKUP, status->deviceId,
                   DEVICE_ID_BYTES);
  if (clearLifetimeProgressRequested != 0) {
    status->totalSteps = 0;
  }
  EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                    sizeof(DeviceStatus));
  SaveReset(clearLifetimeProgressRequested);
  if (clearLifetimeProgressRequested != 0) {
    EepromFill(EEPROM_WALK, sizeof(WalkData), 0);
  } else {
    ClearReturnInventory();
    ClearDiaryActions();
    ClearWeeklySteps();
  }
  if (factoryClearRequested != 0) {
    EepromFill(PW_EEPROM_FILL_EVENT, PW_EEPROM_FILL_EVENT_LEN, 0);
  }
  EepromFill(PW_EEPROM_FILL_RECORD, PW_EEPROM_FILL_RECORD_LEN, 0);
}

/* Boot restore: missing Nintendo signature factory-resets and rewrites the
 * signature; otherwise reload save and clamp contrast. Then replay a staged
 * walk commit when the 0xA5 marker is still set. */
void BootRestore(void)
{
  u8 marker;

  if (BootSignatureValid() != 0) {
    EepromMirrorRead(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                     (u8 *)&g_state.save, sizeof(SaveData));
    if (g_state.save.contrast > PW_SAVE_CONTRAST_MAX) {
      g_state.save.contrast = PW_SAVE_DEFAULT_CONTRAST;
    }
    StatusRestoreFlags();
  } else {
    PersistentReset(1, 1);
    BeepSetOutputMode(g_state.save.volume);
    DisplaySetContrast(g_state.save.contrast);
    BootSignatureWrite();
  }
  EepromMirrorRead(EEPROM_COMMIT_PRIMARY, EEPROM_COMMIT_BACKUP, &marker, 1);
  if (marker == PW_WALK_COMMIT_MARKER) {
    CommitStagedWalk();
    marker = 0;
    EepromMirrorWrite(EEPROM_COMMIT_PRIMARY, EEPROM_COMMIT_BACKUP, &marker, 1);
  }
}

/* On reset, wait through 15000 watchdog/delay pairs before restoring the saved
 * RTC seconds. */
void RtcRestore(void)
{
  u16 n;

  n = 15000;
  do {
    WatchdogService();
    LowClockDelay();
  } while (--n != 0);
  RtcSetTime(g_state.save.rtcSeconds);
}
