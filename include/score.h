#ifndef PW_SCORE_H
#define PW_SCORE_H

#include "types.h"

/* A score alternates duration and pitch/command bytes. Pitch values index
 * timer compare values; they are not MIDI note numbers. Bit 7 joins notes. */
typedef struct {
  u8 duration;
  u8 pitch;
} Note;

#define NOTE_THIRTY_SECOND 3
#define NOTE_SIXTEENTH 6
#define NOTE_WHOLE 96
#define NOTE_PITCH_MASK 0x7F
#define NOTE_LEGATO 0x80
#define NOTE_SET_TEMPO 0x7B
#define NOTE_REST 0x7D
#define NOTE_REPEAT 0x7E
#define NOTE_END 0x7F

#endif
