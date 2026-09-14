#ifndef PW_HARDWARE_H
#define PW_HARDWARE_H

/* Memory-mapped H8 registers. Evaluate these volatile lvalues only in the
 * target address space. */
#if defined(PW_RENESAS_H8) || defined(__H8__) || defined(__HITACHI__)
#include "iodefine.h"
#endif

#endif
