#include "project.h"
#include "types.h"
#include "eeprom_address.h"
#include "iodefine.h"
#include "beep.h"
#include "eeprom.h"

/* Four-byte EEPROM directory entry. The offset is stored little-endian and
 * names a score relative to EEPROM_SOUND_DATA. */
typedef struct {
  u16 offsetLe;
  u8 byteLength;
  u8 checksum;
} SoundEntry;

typedef char SoundEntrySize[sizeof(SoundEntry) == 4 ? 1 : -1];

enum { SOUND_DIRECTORY_ENTRIES = 16, SOUND_STORAGE_BYTES = 480 };

typedef struct {
  SoundEntry entries[SOUND_DIRECTORY_ENTRIES];
  u8 scores[SOUND_STORAGE_BYTES];
} SoundArchive;

typedef char SoundArchiveData[offsetof(SoundArchive, scores) ==
                                      EEPROM_SOUND_DATA - EEPROM_SOUND_DIRECTORY
                                  ? 1
                                  : -1];
typedef char SoundArchiveSize[sizeof(SoundArchive) == 544 ? 1 : -1];

extern const u8 g_noteCompareValues[42];

u8 BeepHasScore(void)
{
  if (g_note == 0) {
    return 0;
  }
  return 1;
}

void BeepInit(void)
{
  g_ui.durationDivisor = 0x78;
  g_ui.outputMode = 0;
  IO.PCR8 |= 0x0c;
  IO.PDR8.BIT.B2 = 0;
  IO.PDR8.BIT.B3 = 0;
  CKSTPR2.BYTE |= 0x40;
  TW.TCRW.BYTE = 0xc0;
  TW.TIOR0.BYTE = 0x10;
  TW.TIOR1.BYTE = 1;
  TW.GRA = 0;
  TW.GRB = 0;
  TW.GRC = 0;
  CKSTPR2.BYTE &= 0xbf;
  g_note = 0;
}

#define PW_BEEPER_SEQUENCE_RAM g_work.beeper.score
#define PW_BEEPER_SEQUENCE_MAX_BYTES sizeof(g_work.beeper.score)
#define PW_BEEPER_CONTROL_VALUE_MASK 0x7f
#define PW_BEEPER_CONTROL_RESTART_MIN 0x7e

/* Load the selected score into shared RAM. Check the sum of complete byte
 * pairs and require the final pair to repeat or end playback. */
void BeepLoadScore(u8 sequenceId)
{
  SoundEntry descriptor;
  u16 raw;
  u8 id;
  u8 index;
  u8 sum;
  SoundEntry *header;

  if (g_ui.outputMode == 0) {
    return;
  }
  TW.TIERW.BIT.IMIEA = 0;
  id = sequenceId;
  header = (SoundEntry *)EEPROM_SOUND_DIRECTORY;
  header += id;
  EepromRead((u16)header, &descriptor, sizeof(SoundEntry));
  raw = descriptor.offsetLe;
  raw = ((raw >> 8) | (raw << 8));
  raw = (raw + (u16) & ((SoundArchive *)EEPROM_SOUND_DIRECTORY)->scores[0]);
  if (descriptor.byteLength <= PW_BEEPER_SEQUENCE_MAX_BYTES) {
    g_note = PW_BEEPER_SEQUENCE_RAM;
    EepromRead(raw, g_note, descriptor.byteLength);
    index = 0;
    sum = 0;
    while (index < ((u16)descriptor.byteLength >> 1)) {
      sum = (sum + g_note[index].duration);
      sum = (sum + g_note[index].pitch);
      index++;
    }
    if ((sum != descriptor.checksum) ||
        ((((u8 *)g_note)[(((u16)descriptor.byteLength >> 1) << 1) - 1] &
          PW_BEEPER_CONTROL_VALUE_MASK) < PW_BEEPER_CONTROL_RESTART_MIN)) {
      g_note = 0;
    } else {
      g_ui.periodsRemaining = 0;
      g_ui.separatorPeriodsRemaining = 0;
    }
  }
  TW.TIERW.BIT.IMIEA = 1;
}

void BeepSelectScore(const Note *step)
{
  g_note = step;
  g_ui.periodsRemaining = 0;
  g_ui.separatorPeriodsRemaining = 0;
}

void BeepEnableTimer(void)
{
  CKSTPR2.BYTE |= 0x40;
  TW.TIERW.BIT.IMIEA = 0;
  TW.TCRW.BYTE = 0xc0;
  TW.TIOR0.BYTE = 0x10;
  TW.TIOR1.BYTE = 1;
  TW.TSRW.BIT.IMFA = 0;
  TW.TIERW.BIT.IMIEA = 1;
  TW.TCNT = 0;
  TW.TMRW.BYTE = 0x80;
}

void BeepDisableTimer(void)
{
  TW.TIERW.BIT.IMIEA = 0;
  TW.TMRW.BYTE = 0;
  TW.TCRW.BYTE = 0xc0;
  TW.TSRW.BIT.IMFA = 0;
  CKSTPR2.BYTE &= 0xbf;
}

void BeepSetOutputMode(u8 mode)
{
  g_ui.outputMode = mode;
}

#define PW_BEEPER_OUTPUT_ALL_EQUAL 0
#define PW_BEEPER_OUTPUT_GRB_HALF 1
#define PW_BEEPER_OUTPUT_GRB_GRC_HALF 2
#define PW_BEEPER_SEPARATOR_INTERVAL 0x140
#define PW_BEEPER_PERIOD_SCALE 0x14000ul
#define PW_BEEPER_SEQUENCE_RAM_PTR g_work.beeper.score

/* Program GRA/GRB/GRC from the compare value and output mode, then reset TCNT.
 * Mode 3 leaves the compare registers unchanged; half-period calculations use
 * signed arithmetic. */
void BeepSetPeriod(u8 value)
{
  switch (g_ui.outputMode) {
  case PW_BEEPER_OUTPUT_ALL_EQUAL:
    TW.GRA = value;
    TW.GRB = value;
    TW.GRC = value;
    break;
  case PW_BEEPER_OUTPUT_GRB_HALF:
    TW.GRA = value;
    TW.GRB = value >> 1;
    TW.GRC = value;
    break;
  case PW_BEEPER_OUTPUT_GRB_GRC_HALF:
    TW.GRA = value;
    TW.GRB = value >> 1;
    TW.GRC = value >> 1;
    break;
  }
  TW.TCNT = 0;
}

/* Interpret the current score record, including end, tempo, note and rest
 * commands. */
void BeepAdvance(void)
{
  const u8 *periodTable;

  if (g_note == 0) {
    return;
  }
  if (g_ui.periodsRemaining != 0) {
    g_ui.periodsRemaining--;
    if ((g_ui.periodsRemaining == 1) &&
        ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_END)) {
      TW.TMRW.BYTE = 0x80;
      TW.TIOR0.BYTE = 0x10;
      TW.TIOR1.BYTE = 1;
    }
    if (g_ui.periodsRemaining != 0) {
      return;
    }
  }
  if (g_ui.periodsRemaining != 0) {
    goto playNote;
  } else if (g_ui.separatorPeriodsRemaining == 0) {
    goto playNote;
  } else {
    TW.GRA = PW_BEEPER_SEPARATOR_INTERVAL;
    TW.GRB = PW_BEEPER_SEPARATOR_INTERVAL;
    TW.GRC = PW_BEEPER_SEPARATOR_INTERVAL;
    TW.TCNT = 0;

    if (g_ui.separatorPeriodsRemaining != 0) {
      g_ui.separatorPeriodsRemaining--;
    }
    return;
  }
playNote:
  if ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_END) {
    g_note = 0;
    return;
  }
  if ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_SET_TEMPO) {
    g_ui.durationDivisor = g_note->duration;
    g_note++;
  }
  if ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_REPEAT) {
    g_note = PW_BEEPER_SEQUENCE_RAM_PTR;
    return;
  }
  periodTable = g_noteCompareValues;
  if ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_REST) {
    g_ui.periodsRemaining =
        ((uint)((PW_BEEPER_PERIOD_SCALE * g_note->duration) /
                g_ui.durationDivisor) /
         periodTable[g_note->pitch & NOTE_PITCH_MASK]);
    BeepSetPeriod(0);
  } else {
    if ((g_note->pitch & NOTE_LEGATO) != 0) {
      g_ui.periodsRemaining =
          ((uint)((PW_BEEPER_PERIOD_SCALE * g_note->duration) /
                  g_ui.durationDivisor) /
           periodTable[g_note->pitch & NOTE_PITCH_MASK]);
      g_ui.separatorPeriodsRemaining = 0;
    } else {
      g_ui.periodsRemaining =
          ((uint)(((PW_BEEPER_PERIOD_SCALE * g_note->duration) /
                   g_ui.durationDivisor) -
                  PW_BEEPER_SEPARATOR_INTERVAL) /
           periodTable[g_note->pitch & NOTE_PITCH_MASK]);
      g_ui.separatorPeriodsRemaining = 1;
    }
    if ((g_note != PW_BEEPER_SEQUENCE_RAM_PTR) &&
        ((g_note[-1].pitch & NOTE_LEGATO) != NOTE_LEGATO)) {
      TW.TMRW.BYTE = 0x83;
      TW.TCRW.BYTE = 0xc2;

      BeepSetPeriod(g_noteCompareValues[g_note->pitch & NOTE_PITCH_MASK]);
    }
  }
  g_note++;
}

#pragma interrupt(TimerWInterrupt(vect = 35))
void TimerWInterrupt(void)
{
  BeepAdvance();
  TW.TSRW.BYTE &= 0xfe;
}

/* Timer W compare values per note */
const u8 g_noteCompareValues[42] = {
    0xf4, 0xe6, 0xd9, 0xcd, 0xc2, 0xb7, 0xac, 0xa3, 0x9a, 0x91, 0x89,
    0x81, 0x7a, 0x73, 0x6c, 0x66, 0x61, 0x5b, 0x56, 0x51, 0x4d, 0x48,
    0x44, 0x40, 0x3d, 0x39, 0x36, 0x33, 0x30, 0x2d, 0x2b, 0x28, 0x26,
    0x24, 0x22, 0x20, 0x1e, 0x1c, 0x1a, 0x19, 0x17, 0x16};
