#include "types.h"
/* Runtime section tables derive ROM and RAM bounds from __sectop and __secend.
 * Startup copies initialized D data into R and clears B. */
#pragma section $DSEC
static const struct {
  u8 *romS; /* Start address of the initialized data section in ROM */
  u8 *romE; /* End address of the initialized data section in ROM */
  u8 *ramS; /* Start address of the initialized data section in RAM */
} DTBL[] = {
    {__sectop("D"), __secend("D"), __sectop("R")},
};
#pragma section $BSEC
static const struct {
  u8 *bS; /* Start address of non-initialized data section */
  u8 *bE; /* End address of non-initialized data section */
} BTBL[] = {
    {__sectop("B"), __secend("B")},
};
#pragma section
