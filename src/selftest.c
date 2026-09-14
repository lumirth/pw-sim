#include "selftest_data.h"
#include "project.h"
#include "types.h"
#include "iodefine.h"
#include "beep.h"
#include "accelerometer.h"
#include "battery.h"
#include "display.h"
#include "eeprom.h"
#include "pad.h"
#include "common.h"
#include "rtc.h"
#include "scratch.h"
#include "selftest.h"
#include "user.h"

#define PW_RTC_STARTUP_DELAY_UNITS 10000
#define PW_SAVE_DEFAULT_VOLUME 2
#define PW_SAVE_DEFAULT_CONTRAST 4
#define PW_ACCEL_REG_CONTROL 0x0A
#define PW_ACCEL_REG_RANGE_BANDWIDTH 0x14
#define PW_ACCEL_REG_X_LSB 0x02
#define PW_ACCEL_SAMPLE_DELAY_UNITS 500
#define PW_ACCEL_RANGE_BANDWIDTH_MASK 0xe0
#define PW_ACCEL_SELFTEST_RANGE_BANDWIDTH 8
#define PW_EEPROM_PATTERN_BLOCK 0x100
#define PW_EEPROM_PAGE_SIZE 0x80
#define PW_EEPROM_FILL_BYTE 0xff
#define PW_INTERACTIVE_IDLE_SECONDS 60
#define PW_DIAGNOSTICS_SESSION_IDLE_SECONDS 0x1e
#define PW_DIAGNOSTICS_NAV_HOLD_SECONDS 4
#define PW_DIAGNOSTICS_INTER_TEST_SECONDS 2
#define PW_EEPROM_SELFTEST_DIAGNOSTICS_ORIGIN 0x300
#define PW_DISPLAY_INIT_TRAILER_A6 0xa6
#define PW_DISPLAY_COMMAND_A7 0xa7
#define PW_DIAGNOSTICS_GLYPH_X 0x20
#define PW_DIAGNOSTICS_GLYPH_Y 8
#define PW_DIAGNOSTICS_MARKER_Y 0x38
#define PW_DIAGNOSTICS_MARKER_X_LEFT 6
#define PW_DIAGNOSTICS_MARKER_X_MID 0x2d
#define PW_DIAGNOSTICS_MARKER_X_RIGHT 0x55
#define PW_DIAGNOSTICS_OK_Y 0
#define PW_DIAGNOSTICS_HEX_Y 0x18
#define PW_DIAGNOSTICS_ACCEL_OK_X 0x08
#define PW_DIAGNOSTICS_ACCEL_OK_Y 0x20
#define PW_DIAGNOSTICS_FAILURE_X 0x08
#define PW_DIAGNOSTICS_FAILURE_Y 0x08

/* Test EEPROM with 256-byte incrementing patterns, verify each block, then fill
 * pages with 0xFF. Return zero on the first mismatch. */
u8 EepromSelfTest(uint value)
{
  u16 length;
  u8 *source;
  volatile struct testCursor {
    u8 pattern;
    u16 address;
  } cursor;
  s16 i;

  length = PW_EEPROM_PATTERN_BLOCK;
  ScratchReset();
  source = ScratchAlloc(length);
  cursor.address = value;
  cursor.pattern = 0;
  do {
    WatchdogService();
    i = 0;
    do {

      ((volatile u8 *)source)[i] = cursor.pattern++;
      i++;
    } while (i < PW_EEPROM_PATTERN_BLOCK);
    EepromWrite(cursor.address, source, length);
    cursor.address += length;
    cursor.pattern++;
  } while (cursor.address != 0);

  cursor.address = value;
  cursor.pattern = 0;
  do {
    WatchdogService();
    EepromRead(cursor.address, source, length);
    i = 0;
    while (i < PW_EEPROM_PATTERN_BLOCK) {
      if (source[i] != cursor.pattern++) {
        return 0;
      }
      i++;
    }
    cursor.address += PW_EEPROM_PATTERN_BLOCK;
    cursor.pattern++;
  } while (cursor.address != 0);

  cursor.address = value;
  do {
    WatchdogService();
    EepromFillPage(cursor.address, PW_EEPROM_FILL_BYTE);
    cursor.address += PW_EEPROM_PAGE_SIZE;
  } while (cursor.address != 0);
  return 1;
}

/* Zero the RTC, wait through the startup delay, then return whether two stable
 * seconds-register reads agree and are nonzero. */
u8 RtcStartupCheck(void)
{
  u16 remaining;
  u8 first;
  u8 second;

  RtcSetTime(0);
  remaining = PW_RTC_STARTUP_DELAY_UNITS;
  do {
    LowClockDelay();
  } while (--remaining != 0);
  do {
    while (RTC.RSECDR.BIT.BSY) {
    }
    first = RTC.RSECDR.BYTE;
    second = RTC.RSECDR.BYTE;
  } while (first != second);
  if (first == 0) {
    return 0;
  }
  return 1;
}

/* Probe the accelerometer, program +/-4 g at 25 Hz, burst-read XYZ, and stream
 * the three MSB sample bytes over the display SSU. */
u8 FactoryAccelDump(void)
{
  u8 sample[6];
  u8 *buf;
  u16 remaining;
  u8 txByte;

  if (AccelInit() == 0) {
    return 0;
  }
  buf = sample;
  AccelRead(PW_ACCEL_REG_RANGE_BANDWIDTH, buf, 1);
  sample[0] = (sample[0] & PW_ACCEL_RANGE_BANDWIDTH_MASK);
  sample[0] |= PW_ACCEL_SELFTEST_RANGE_BANDWIDTH;
  AccelWrite(PW_ACCEL_REG_RANGE_BANDWIDTH, sample[0]);
  AccelWrite(PW_ACCEL_REG_CONTROL, 0);
  remaining = PW_ACCEL_SAMPLE_DELAY_UNITS;
  while (remaining != 0) {
    LowClockDelay();
    remaining--;
  }
  AccelRead(PW_ACCEL_REG_X_LSB, buf, 6);
  txByte = sample[1];
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  txByte = sample[3];
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  txByte = sample[5];
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  return 1;
}

/* Sample the ADC, stream the high and low result bytes to the display SSU,
 * checksum-protect the reading, and persist it at the battery-threshold EEPROM
 * pair. */
u8 FactoryAdcCalibrate(void)
{
  uint sample;
  u8 txByte;

  sample = BatterySample();
  txByte = (sample >> 8);
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  txByte = sample;
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  sample = BatteryProtect(sample);
  EepromMirrorWrite(EEPROM_BATTERY_PRIMARY, EEPROM_BATTERY_BACKUP,
                    (u8 *)&sample, 2);
}

/* Stream four 24-column grayscale bars (00/00, 00/FF, FF/00, FF/FF) across all
 * eight pages of the drawing bank. */
void DiagnosticsGray(void)
{
  u8 row;
  u8 col;
  s16 band;

  SSU.SSER.BYTE = 0x80;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 1;
  row = 0;
  do {
    DisplayAddress(0, row);
    IO.PDR1.BIT.B1 = 1;
    col = 0;
    do {
      band = col / 24;
      switch (band) {
      case 0:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        break;
      case 1:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        break;
      case 2:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        break;
      case 3:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        break;
      }
      col++;
    } while (col < 0x60);
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    row++;
  } while (row < 8);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Seed diagnostics UI words, set save volume to the default, and apply beeper
 * output plus display contrast. */
void DiagnosticsInit(void)
{
  g_ui.view.diag.stage = 0;
  g_ui.view.diag.holdSeconds = 0;
  g_ui.view.diag.mirrorWord = 1;
  g_state.save.volume = PW_SAVE_DEFAULT_VOLUME;
  BeepSetOutputMode(PW_SAVE_DEFAULT_VOLUME);
  DisplaySetContrast(PW_SAVE_DEFAULT_CONTRAST);
}

/* Read a stable RTC seconds value before or after the diagnostics reset. */
#pragma inline(DiagnosticsSecond)
static u8 DiagnosticsSecond(void)
{
  u8 first;

  for (;;) {
    while (RTC.RSECDR.BIT.BSY) {
    }
    first = RTC.RSECDR.BYTE;
    if (first == RTC.RSECDR.BYTE) {
      break;
    }
  }
  return first;
}

/* Device-diagnostics controller: refresh idle timers and step the 19-stage
 * factory sequence (LCD patterns, input lanes, EEPROM/RTC/battery/accel checks)
 * from byte_f7ce, then hand off to the accelerometer threshold test. */
void DiagnosticsUpdate(void)
{
  u8 first;

  g_state.idleSeconds[0] = PW_INTERACTIVE_IDLE_SECONDS;
  g_state.idleSeconds[1] = PW_DIAGNOSTICS_SESSION_IDLE_SECONDS;
  switch (g_ui.view.diag.stage) {
  case 0:
    if (g_ui.view.diag.introReady == 0) {
      return;
    }
    g_ui.view.diag.stage++;
    return;
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    if (g_ui.view.diag.holdSeconds < PW_DIAGNOSTICS_NAV_HOLD_SECONDS) {
      return;
    }
    if (InputPressed(BUTTON_CENTER | BUTTON_RIGHT) != 0) {
      BeepSelectScore(g_testScore);
      g_ui.view.diag.stage++;
      g_ui.view.diag.holdSeconds = 0;
      return;
    }
    if (g_ui.view.diag.stage == 1) {
      return;
    }
    if (InputPressed(BUTTON_LEFT) == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage--;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 7:
    if (InputPressed(BUTTON_LEFT) == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 8:
    if (InputPressed(BUTTON_CENTER) == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 9:
    if (InputPressed(BUTTON_RIGHT) == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 10:
    if (g_ui.view.diag.holdSeconds < PW_DIAGNOSTICS_INTER_TEST_SECONDS) {
      return;
    }
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 11:
    first = DiagnosticsSecond();
    g_ui.view.diag.rtcFirstSample = first;
    g_ui.view.diag.probeResult =
        EepromSelfTest(PW_EEPROM_SELFTEST_DIAGNOSTICS_ORIGIN);
    PersistentReset(1, 1);
    first = DiagnosticsSecond();
    g_ui.view.diag.rtcSecondSample = first;
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 12:
    if (g_ui.view.diag.probeResult == 0) {
      return;
    }
    if (g_ui.view.diag.holdSeconds < PW_DIAGNOSTICS_NAV_HOLD_SECONDS) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 13:
    EepromMirrorRead(EEPROM_BATTERY_PRIMARY, EEPROM_BATTERY_BACKUP,
                     (u8 *)&g_ui.view.diag.mirrorWord, 2);
    g_ui.view.diag.probeResult = BatteryVerify(g_ui.view.diag.mirrorWord);
    if (g_ui.view.diag.mirrorWord == 0) {
      g_ui.view.diag.probeResult = 0;
    }
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 14:
    if (g_ui.view.diag.probeResult == 0) {
      return;
    }
    if (g_ui.view.diag.holdSeconds < PW_DIAGNOSTICS_NAV_HOLD_SECONDS) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 15:
    if (g_ui.view.diag.rtcFirstSample == g_ui.view.diag.rtcSecondSample) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 16:
    g_ui.view.diag.probeResult = AccelInit();
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 17:
    if (g_ui.view.diag.probeResult == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.holdSeconds = 0;
    return;
  case 18:
    if (g_ui.view.diag.holdSeconds < PW_DIAGNOSTICS_NAV_HOLD_SECONDS) {
      return;
    }
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    IO.PDR1.BIT.B0 = 0;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_DISPLAY_INIT_TRAILER_A6;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    g_state.view = VIEW_THRESHOLD_TEST;
    ThresholdInit();
    BeepSelectScore(g_testScore);
    return;
  }
}

/* Draw a one-pixel border: top bit, left and right full columns, bottom bit. */
void DiagnosticsBorder(void)
{
  u16 remaining;
  s16 page;

  SSU.SSER.BYTE = 0x80;
  IO.PDR1.BIT.B0 = 0;
  DisplayAddress(1, 0);
  IO.PDR1.BIT.B1 = 1;
  remaining = 0xbc;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 1;
    remaining--;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  page = 0;
  do {
    DisplayAddress(0, page);
    IO.PDR1.BIT.B1 = 1;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    page++;
  } while (page < 8);
  page = 0;
  do {
    DisplayAddress(0x5f, page);
    IO.PDR1.BIT.B1 = 1;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    page++;
  } while (page < 8);
  DisplayAddress(1, 7);
  IO.PDR1.BIT.B1 = 1;
  remaining = 0xbc;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0x80;
    remaining--;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Render LCD patterns, input markers, factory NG/OK text, and the
 * battery-threshold hex dump. Saturate the frame hold timer at four seconds. */
void DiagnosticsRender(void)
{
  char hex[5];

  switch (g_ui.view.diag.stage) {
  case 0:
    if (g_ui.view.diag.introReady != 0) {
      break;
    }
    DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG1");
    break;
  case 10:
  case 11:
    DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "EEP");
    break;
  case 12:
    if (g_ui.view.diag.probeResult == 0) {
      DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG2");
    }
    break;
  case 14:
    if (g_ui.view.diag.probeResult == 0) {
      DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG3");
    }
    break;
  case 15:
    if (g_ui.view.diag.rtcFirstSample == g_ui.view.diag.rtcSecondSample) {
      DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG4");
    }
    break;
  case 1:
    DisplayFill(3);
    break;
  case 2:
    DisplayFill(2);
    break;
  case 3:
    DisplayFill(1);
    break;
  case 4:
    DisplayFill(0);
    break;
  case 5:
    DiagnosticsBorder();
    break;
  case 6:
    DiagnosticsGray();
    break;
  case 7:
    if (((g_state.uiFrame >> 1) & 1) == 0) {
      DisplayText(PW_DIAGNOSTICS_MARKER_X_LEFT, PW_DIAGNOSTICS_MARKER_Y, "V");
    }
    break;
  case 8:
    if (((g_state.uiFrame >> 1) & 1) == 0) {
      DisplayText(PW_DIAGNOSTICS_MARKER_X_MID, PW_DIAGNOSTICS_MARKER_Y, "V");
    }
    break;
  case 9:
    if (((g_state.uiFrame >> 1) & 1) == 0) {
      DisplayText(PW_DIAGNOSTICS_MARKER_X_RIGHT, PW_DIAGNOSTICS_MARKER_Y, "V");
    }
    break;
  case 17:
    if (g_ui.view.diag.probeResult == 0) {
      DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG5");
    }
    break;
  case 18:
    IO.PDR1.BIT.B0 = 0;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_DISPLAY_COMMAND_A7;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_OK_Y, "OK");
    hex[4] = 0;
    hex[0] = g_hexDigits[(g_ui.view.diag.mirrorWord / 0x1000) & 0xf];
    /* The second hexadecimal digit contains bits 8..11. */
    hex[1] = g_hexDigits[(g_ui.view.diag.mirrorWord >> 8) & 0xf];
    hex[2] = g_hexDigits[(g_ui.view.diag.mirrorWord >> 4) & 0xf];
    hex[3] = g_hexDigits[g_ui.view.diag.mirrorWord & 0xf];
    DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_HEX_Y, hex);
    if (((g_state.uiFrame >> 1) & 1) != 0) {
      break;
    }
    DisplayText(PW_DIAGNOSTICS_MARKER_X_LEFT, PW_DIAGNOSTICS_MARKER_Y, "V");
    DisplayText(PW_DIAGNOSTICS_MARKER_X_MID, PW_DIAGNOSTICS_MARKER_Y, "V");
    DisplayText(PW_DIAGNOSTICS_MARKER_X_RIGHT, PW_DIAGNOSTICS_MARKER_Y, "V");
    break;
  }
  if (g_ui.view.diag.holdSeconds < PW_DIAGNOSTICS_NAV_HOLD_SECONDS) {
    g_ui.view.diag.holdSeconds++;
  }
}

/* Reset the accelerometer threshold-test session: clear the sample ring and
 * live XYZ words, load MotionThresholds from EEPROM, and set state bit 1. */
void ThresholdInit(void)
{
  g_state.sampleIndex = 0;
  g_ui.view.accel.resetWord = 0x10;
  g_ui.view.accel.walkingSamples = 0;
  g_ui.view.accel.stillSamples = 0;
  g_ui.view.accel.xActivity = 0;
  g_ui.view.accel.yActivity = 0;
  g_ui.view.accel.zActivity = 0;
  EepromRead(EEPROM_COUNTERS, &g_ui.view.accel.countParameters,
             sizeof(MotionThresholds));
  g_state.flags.byte |= SYSTEM_REGISTERED;
}

void ThresholdUpdate(void)
{
}

/* Render the accelerometer threshold-test session: ASCII phase digits,
 * hexadecimal threshold words, and "OK" once the live phase matches the
 * loaded limit. */
void ThresholdRender(void)
{
  char hex[5];

  g_state.idleSeconds[0] = PW_INTERACTIVE_IDLE_SECONDS;
  g_state.idleSeconds[1] = PW_DIAGNOSTICS_SESSION_IDLE_SECONDS;

  hex[0] = (g_ui.view.accel.walkingSamples + '0');
  hex[1] = 0;
  DisplayText(0x4c, 0x00, hex);

  hex[0] = (g_ui.view.accel.countParameters.stepSamples + '0');
  hex[1] = 0;
  DisplayText(0x54, 0x00, hex);

  hex[0] = g_hexDigits[g_ui.view.accel.countParameters.motionMinimum / 0x1000];
  hex[1] =
      g_hexDigits[(g_ui.view.accel.countParameters.motionMinimum >> 8) & 0xf];
  hex[2] =
      g_hexDigits[(g_ui.view.accel.countParameters.motionMinimum >> 4) & 0xf];
  hex[3] = g_hexDigits[g_ui.view.accel.countParameters.motionMinimum & 0xf];
  hex[4] = 0;
  DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, hex);

  hex[0] = g_hexDigits[g_ui.view.accel.countParameters.motionMaximum / 0x1000];
  hex[1] =
      g_hexDigits[(g_ui.view.accel.countParameters.motionMaximum >> 8) & 0xf];
  hex[2] =
      g_hexDigits[(g_ui.view.accel.countParameters.motionMaximum >> 4) & 0xf];
  hex[3] = g_hexDigits[g_ui.view.accel.countParameters.motionMaximum & 0xf];
  DisplayText(0x40, 0x08, hex);

  hex[0] = g_hexDigits[g_ui.view.accel.countParameters.stopLimit / 0x1000];
  hex[1] = g_hexDigits[(g_ui.view.accel.countParameters.stopLimit >> 8) & 0xf];
  hex[2] = g_hexDigits[(g_ui.view.accel.countParameters.stopLimit >> 4) & 0xf];
  hex[3] = g_hexDigits[g_ui.view.accel.countParameters.stopLimit & 0xf];
  DisplayText(0x20, 0x10, hex);

  hex[0] = (g_ui.view.accel.stillSamples + '0');
  hex[1] = 0;
  DisplayText(0x4c, 0x10, hex);

  hex[0] = (g_ui.view.accel.countParameters.stopSamples + '0');
  DisplayText(0x54, 0x10, hex);

  if (g_ui.view.accel.stillSamples ==
      g_ui.view.accel.countParameters.stopSamples) {
    IO.PDR1.BIT.B0 = 0;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_DISPLAY_COMMAND_A7;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    DisplayText(PW_DIAGNOSTICS_ACCEL_OK_X, PW_DIAGNOSTICS_ACCEL_OK_Y, "OK");
  }
}

/* Fixed "NG6" glyph stream after accelerometer threshold validation fails. */
void ThresholdFailureRender(void)
{
  DisplayText(PW_DIAGNOSTICS_FAILURE_X, PW_DIAGNOSTICS_FAILURE_Y, "NG6");
}

/* Write the eight-byte "nintendo" signature at the start of EEPROM. */

void BootSignatureWrite(void)
{
  u8 i;

  i = 0;
  do {
    EepromWriteByte(i, g_eepromSignature[i]);
    i++;
  } while (i < EEPROM_SIGNATURE_BYTES);
}

/* Return 1 when the signature at the start of EEPROM is valid. */

u8 BootSignatureValid(void)
{
  u8 i;

  i = 0;
  while (i < EEPROM_SIGNATURE_BYTES) {
    if (EepromReadByte(i) != g_eepromSignature[i]) {
      return 0;
    }
    i++;
  }
  return 1;
}
