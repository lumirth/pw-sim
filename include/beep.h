#ifndef PW_BEEP_H
#define PW_BEEP_H

#include "types.h"
#include "data.h"

/* Score selection and timer ownership are separate. The foreground enables
 * playback while a score is selected and disables the timer when it ends. */
u8 BeepHasScore(void);
void BeepInit(void);
void BeepLoadScore(u8 sequenceId);
void BeepSelectScore(const Note *step);
void BeepEnableTimer(void);
void BeepDisableTimer(void);
void BeepSetOutputMode(u8 mode);

#endif
