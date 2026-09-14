#include "iodefine.h"
#include "setup.h"

/* Initialize the display SSU and its GPIO levels before switching PMRB to the
 * SSU pin function. The pins remain in GPIO mode until the peripheral and idle
 * levels are ready. */
void HardwareSetup(void)
{
  CKSTPR2.BYTE |= 0x10;   /* ungate the SSU peripheral clock */
  SSU.SSCRL.BYTE |= 0x40; /* SSU mode rather than I2C */
  SSU.SSMR.BYTE = 0x86;   /* transfer mode and clock rate */
  SSU.SSCRH.BYTE = 0x8c;  /* master, plus select/clock polarity */
  IO.PUCR9.BYTE = 8;      /* port 9 pull-up on the input lane */
  IO.PCR9 = 1;            /* port 9 direction */
  IO.PCR1 = 7;            /* port 1 direction: select, clock, data */
  IO.PDR1.BYTE = 5;       /* initial deselected/idle levels */
  IO.PDR9.BYTE |= 1;      /* raise the port 9 output */
  IO.PMRB.BYTE = 1;       /* hand the pins to the SSU last */
}
