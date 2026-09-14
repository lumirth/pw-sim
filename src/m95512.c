#include "types.h"
#include "iodefine.h"
#include "project.h"
#include "eeprom.h"
#include "common.h"

/* One SSU readiness operation, expanded at each transfer site. */
#pragma inline(EepromWaitReady)
static void EepromWaitReady(void)
{
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
}

/* Ungate the SSU and disable it while selecting mode 0x86 or 0x87 from
 * shared-event flag bit 4. */
void EepromConfigure(void)
{
  CKSTPR2.BYTE |= 0x10;
  SSU.SSER.BYTE = 0;
  if (g_state.events.bits.lowPowerClock) {
    SSU.SSMR.BYTE = 0x86;
  } else {
    SSU.SSMR.BYTE = 0x87;
  }
}

/* Idle the EEPROM SSU with its transmitter enabled. */
void EepromIdle(void)
{
  SSU.SSER.BYTE = 0x80;
  SSU.SSMR.BYTE = 0x86;
}

/* Wait for a receive byte or overrun. On overrun, clear SSSR bit 6 and latch
 * shared-event bit 6 before returning the received byte. */
u8 EepromReceive(void)
{
  u8 received;

  for (;;) {
    if (SSU.SSSR.BIT.ORER) {
      SSU.SSSR.BIT.ORER = 0;
      g_state.events.bits.eepromError = 1;
      break;
    }
    if (SSU.SSSR.BIT.RDRF) {
      break;
    }
  }
  received = SSU.SSRDR;
  return received;
}

#define PW_EEPROM_TRANSFER_ATTEMPTS 3
#define PW_EEPROM_CMD_READ_STATUS 5
#define PW_EEPROM_CMD_WRITE_ENABLE 6
#define PW_EEPROM_CMD_PAGE_PROGRAM 2
#define PW_EEPROM_CMD_READ 3
#define PW_EEPROM_PAGE_SIZE 128

/* Retry a one-byte page program: poll WIP with command 5, issue write-enable 6
 * and page program 2, and retry while the SSU overrun flag stays set. */
void EepromWriteByte(u16 address, u8 value)
{
  u8 attempts;
  u8 status;
  u8 addrHi;

  attempts = PW_EEPROM_TRANSFER_ATTEMPTS;
  g_state.events.bits.eepromError = 0;
  addrHi = (address >> 8);
  while (attempts != 0) {
    WatchdogService();
    EepromConfigure();
    SSU.SSER.BYTE &= 0x3f;
    SSU.SSSR.BYTE = 0;
    SSU.SSER.BYTE |= 0xc0;
    IO.PDR1.BIT.B2 = 0;
    EepromWaitReady();
    SSU.SSTDR = PW_EEPROM_CMD_READ_STATUS;
    EepromReceive();
    do {
      EepromWaitReady();
      SSU.SSTDR = 0xff;
      status = EepromReceive();
      status &= 1;
    } while (status == 1);
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B2 = 1;
    SSU.SSER.BYTE &= 0x3f;
    SSU.SSSR.BYTE = 0;
    SSU.SSER.BIT.TE = 1;
    IO.PDR1.BIT.B2 = 0;
    EepromWaitReady();
    SSU.SSTDR = PW_EEPROM_CMD_WRITE_ENABLE;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B2 = 1;
    IO.PDR1.BIT.B2 = 0;
    EepromWaitReady();
    SSU.SSTDR = PW_EEPROM_CMD_PAGE_PROGRAM;
    status = addrHi;
    EepromWaitReady();
    SSU.SSTDR = status;
    status = address;
    EepromWaitReady();
    SSU.SSTDR = status;
    status = value;
    EepromWaitReady();
    SSU.SSTDR = status;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B2 = 1;
    EepromIdle();
    if (g_state.events.bits.eepromError == 0) {
      break;
    }
    attempts--;
  }
}
