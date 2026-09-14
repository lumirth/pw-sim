#include "types.h"
#include "iodefine.h"
#include "accelerometer.h"

/* The BMA150 uses the H8 synchronous serial unit in four-wire mode. Chip select
 * is active low on PDR9 bit 0. SSSR bits 2, 1 and 3 indicate transmit-empty,
 * receive-full and transfer-end. */

/* BMA150 register addresses, named from the component's register map. */
#define PW_ACCEL_REG_CHIP_ID 0x00
/* Register 0x0A controls EEPROM write enable (bit 4). Register 0x1E is writable
 * only while that bit is set. */
#define PW_ACCEL_REG_CONTROL 0x0A
#define PW_ACCEL_REG_CONF1 0x0B
#define PW_ACCEL_REG_RANGE_BANDWIDTH 0x14
#define PW_ACCEL_REG_CONF2 0x15
#define PW_ACCEL_REG_EEPROM_WINDOW_1E 0x1E

/* SSU enable patterns. 0x80 is transmit only; 0xC0 adds the receiver for the
 * read phase; 0x80 is restored on the way out. */
#define PW_ACCEL_SSU_TRANSMIT_ENABLE 0x80
#define PW_ACCEL_SSU_TRANSFER_ENABLE 0xC0

/* Read consecutive registers: send the address with bit 7 set to mark the read
 * phase, then clock out 0xFF once per byte and latch each received byte.
 * Returns zero; the value is status residue that no caller consumes. */
u8 AccelRead(u8 registerAddress, u8 *destination, u8 byteCount)
{
  u8 received;

  SSU.SSSR.BYTE &= 4;
  SSU.SSER.BYTE = PW_ACCEL_SSU_TRANSFER_ENABLE;
  registerAddress |= 0x80;         /* mark the read phase */
  IO.PDR9.BYTE &= 0xfe;            /* assert chip select */
  while (SSU.SSSR.BIT.TDRE == 0) { /* wait transmit-empty */
  }
  SSU.SSTDR = registerAddress;
  while (SSU.SSSR.BIT.RDRF == 0) { /* wait receive-full */
  }
  /* Discard the byte received during address transmission so the following
   * reads start with the requested register. */
  (void)SSU.SSRDR;
  do {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff; /* clock the next byte in */
    while (SSU.SSSR.BIT.RDRF == 0) {
    }
    received = SSU.SSRDR;
    *destination = received;
    destination = destination + 1;
  } while (--byteCount != 0);
  while (SSU.SSSR.BIT.TEND == 0) { /* wait transfer-end */
  }
  IO.PDR9.BYTE |= 1; /* release chip select */
  SSU.SSER.BYTE = PW_ACCEL_SSU_TRANSMIT_ENABLE;
  return 0;
}

/* Write one register: address byte then value byte, both on the transmit path.
 * SSER = 0x80 enables the H8 transmitter and is not a wire-protocol prefix. */
void AccelWrite(u8 registerAddress, u8 value)
{
  SSU.SSER.BYTE = PW_ACCEL_SSU_TRANSMIT_ENABLE;
  IO.PDR9.BYTE &= 0xfe;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = registerAddress;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = value;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR9.BYTE |= 1;
  return;
}

/* Identify the BMA150 and configure it only when chip-id bits 2..0 equal 2.
 * Select four-wire SPI, preserve the upper range bits while setting bandwidth
 * 6, and bracket the EEPROM-window write with write enable. Release chip select
 * on both paths. */
u8 AccelInit(void)
{
  u8 scratch[2];

  SSU.SSSR.BYTE &= 4;
  AccelWrite(PW_ACCEL_REG_CONF2, 0x80); /* four-wire SPI */
  AccelRead(PW_ACCEL_REG_CHIP_ID, scratch, 2);

  if ((scratch[0] & 7) != 2) {
    IO.PDR9.BYTE |= 1;
    return 0;
  }
  AccelRead(PW_ACCEL_REG_RANGE_BANDWIDTH, scratch, 1);
  scratch[0] = ((scratch[0] & 0xe0) | 6);
  AccelWrite(PW_ACCEL_REG_RANGE_BANDWIDTH, scratch[0]);
  AccelWrite(PW_ACCEL_REG_CONF1, 0);
  AccelWrite(PW_ACCEL_REG_CONTROL, 0x10); /* enable EEPROM write */
  AccelRead(PW_ACCEL_REG_EEPROM_WINDOW_1E, scratch, 1);
  scratch[0] = (scratch[0] | 0x80);
  AccelWrite(PW_ACCEL_REG_EEPROM_WINDOW_1E, scratch[0]);
  AccelWrite(PW_ACCEL_REG_CONTROL, 0);
  IO.PDR9.BYTE |= 1;
  return 1;
}
