#include "rominfo.h"
#include "types.h"

/* Fixed build stamp and firmware-identification bytes. */

const char g_buildDate[] = "Jun 26 2009";
const u8 g_firmwareId[2] = {FIRMWARE_COMPATIBILITY, FIRMWARE_REVISION};
