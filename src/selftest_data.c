#include "selftest_data.h"

#include "beep.h"

/* Eight signature characters followed by two zero bytes. */
const char g_eepromSignature[10] = "nintendo";

enum { TEST_HIGH_PITCH = 40, TEST_LOW_PITCH = 38 };

/* A short high tone followed by a longer low tone. */
const Note g_testScore[4] = {{100, NOTE_SET_TEMPO},
                             {NOTE_THIRTY_SECOND, TEST_HIGH_PITCH},
                             {NOTE_SIXTEENTH, TEST_LOW_PITCH},
                             {NOTE_WHOLE, NOTE_END}};

/* Hexadecimal digits, without a string terminator. */
const char g_hexDigits[16] = "0123456789ABCDEF";
