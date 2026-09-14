#include "rominfo.h"
#include "types.h"
#include "iodefine.h"
#include <machine.h>
#include "eeprom.h"
#include "factory.h"
#include "common.h"
#include "selftest.h"
#include "user.h"

/* Factory metadata contains the 12-byte build-date string followed by two
 * version bytes. */

#define PW_DISPLAY_HANDSHAKE_COMMAND_E2 0xe2u
#define PW_DISPLAY_HANDSHAKE_RESPONSE_AA 0xaau
#define PW_DISPLAY_HANDSHAKE_REJECT_B0 0xb0u
#define PW_BOOT_SELF_TEST_STAGE_EEPROM 4
#define PW_BOOT_SELF_TEST_STAGE_RTC 3
#define PW_BOOT_SELF_TEST_STAGE_ACCEL 2
#define PW_BOOT_SELF_TEST_STAGE_ADC 1
#define PW_BOOT_SELF_TEST_STAGE_DONE 0
#define PW_BOOT_SELF_TEST_FAILURE_F4 0xf4u
#define PW_BOOT_SELF_TEST_FAILURE_F3 0xf3u
#define PW_BOOT_SELF_TEST_FAILURE_F1 0xf1u
#define PW_BUILD_DATE_BYTES 12
#define PW_ACCEL_SLEEP_COMMAND 10
#define PW_ACCEL_SLEEP_PAYLOAD 1

/* Factory boot self-test: perform the display handshake, stream the metadata,
 * disable the watchdog, then test EEPROM, RTC, accelerometer and ADC. A failed
 * handshake sends 0xb0 and returns; a failed check sends its error byte and
 * hangs. Success parks the sensors and sleeps. */
void BootSelfTest(void)
{
  u8 handshake;
  u8 txByte;
  s16 i;

  set_ccr(0x80);

  SSU.SSER.BYTE = 0xc0;
  SSU.SSMR.BYTE = ((SSU.SSMR.BYTE & 0xf8) | 6);
  if (SSU.SSSR.BIT.ORER) {
    SSU.SSSR.BIT.ORER = 0;
  }

  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = PW_DISPLAY_HANDSHAKE_COMMAND_E2;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  while (SSU.SSSR.BIT.RDRF == 0) {
  }
  handshake = SSU.SSRDR;
  SSU.SSER.BYTE = 0x80;
  LowClockDelay();

  if (handshake != PW_DISPLAY_HANDSHAKE_RESPONSE_AA) {
    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_DISPLAY_HANDSHAKE_REJECT_B0;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    LowClockDelay();
  } else {
    txByte = g_firmwareId[0];
    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = txByte;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    LowClockDelay();

    txByte = g_firmwareId[1];
    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = txByte;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    LowClockDelay();

    i = 0;
    do {
      txByte = g_buildDate[i];
      IO.PDR1.BIT.B0 = 0;
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = txByte;
      while (SSU.SSSR.BIT.TEND == 0) {
      }
      IO.PDR1.BIT.B0 = 1;
      LowClockDelay();
      i++;
    } while (i < PW_BUILD_DATE_BYTES);

    WDT.TCSRWD1.BYTE = 0x9e;
    WDT.TCSRWD1.BYTE = 0xa2;
    WDT.TCSRWD1.BYTE = 0x8e;

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_EEPROM;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    if (EepromSelfTest(0) == 0) {
      IO.PDR1.BIT.B0 = 0;
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = PW_BOOT_SELF_TEST_FAILURE_F4;
      while (SSU.SSSR.BIT.TEND == 0) {
      }
      IO.PDR1.BIT.B0 = 1;
      for (;;) {
      }
    }

    PersistentReset(1, 1);

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_RTC;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    if (RtcStartupCheck() == 0) {
      IO.PDR1.BIT.B0 = 0;
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = PW_BOOT_SELF_TEST_FAILURE_F3;
      while (SSU.SSSR.BIT.TEND == 0) {
      }
      IO.PDR1.BIT.B0 = 1;
      for (;;) {
      }
    }

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_ACCEL;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    FactoryAccelDump();

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_ADC;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    if (FactoryAdcCalibrate() == 0) {
      IO.PDR1.BIT.B0 = 0;
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = PW_BOOT_SELF_TEST_FAILURE_F1;
      while (SSU.SSSR.BIT.TEND == 0) {
      }
      IO.PDR1.BIT.B0 = 1;
      for (;;) {
      }
    }

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_DONE;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;

    IO.PDR9.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_ACCEL_SLEEP_COMMAND;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_ACCEL_SLEEP_PAYLOAD;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR9.BIT.B0 = 1;

    set_ccr(0x80);
    CKSTPR1.BIT.S3CKSTP = 0;
    CKSTPR1.BIT.ADCKSTP = 0;
    CKSTPR1.BIT.TB1CKSTP = 0;
    CKSTPR1.BIT.RTCCKSTP = 0;
    CKSTPR2.BYTE = 0;
    IO.PDR3.BYTE = 1;
    ClockSleep(1);
  }

  IO.PDR1.BIT.B0 = 1;
  set_ccr(0);
  return;
}
