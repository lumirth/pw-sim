#include "types.h"
#include "iodefine.h"
#include "project.h"
#include "beep.h"
#include "display.h"
#include "power.h"

/* Wake the motion session: ungate Timer B1, select clock mode 0x08 in the
 * shared state byte, enable the RTC quarter-second interrupt, refresh the idle
 * countdown, and clear shared-event flag bit 7. */
void MotionSessionWake(void)
{
  CKSTPR1.BYTE |= 4;
  g_state.flags.byte =
      ((g_state.flags.byte & SYSTEM_MODE_CLEAR) | SYSTEM_MODE_MOTION);
  RTC.RTCCR2.BYTE |= 1;
  g_state.idleSeconds[1] = 0x1e;
  g_state.events.byte &= EVENT_CLEAR(EVENT_MOTION);
}

#define PW_INTERACTIVE_IDLE_SECONDS 60
#define PW_MOTION_SESSION_UI_IDLE_SECONDS 90
#define PW_MOTION_ACTIVITY_THRESHOLD 0x1E

/* Absolute changes in the three signed sample axes. The conditional expression
 * reads its input again in the selected arm; the sample fields retain that
 * repeated-read behavior. */
#define PW_ABS(value) ((value) >= 0 ? (value) : -(value))

u8 MotionActivityCheck(void)
{
  u8 previous;
  s16 acc;

  previous = ((g_state.sampleIndex + 0x3f) & 0x3f);
  acc =
      PW_ABS(g_work.motion.x[g_state.sampleIndex] - g_work.motion.x[previous]);
  acc +=
      PW_ABS(g_work.motion.y[g_state.sampleIndex] - g_work.motion.y[previous]);
  acc +=
      PW_ABS(g_work.motion.z[g_state.sampleIndex] - g_work.motion.z[previous]);
  if ((uint)acc > PW_MOTION_ACTIVITY_THRESHOLD) {
    return 1;
  }
  return 0;
}

void MotionSessionStart(void)
{
  g_state.idleSeconds[0] = PW_INTERACTIVE_IDLE_SECONDS;
  g_state.idleSeconds[1] = PW_MOTION_SESSION_UI_IDLE_SECONDS;
  if ((g_state.flags.byte & SYSTEM_MODE_MASK) != SYSTEM_MODE_INTERACTIVE) {
    if ((g_state.flags.byte & SYSTEM_MODE_MASK) == 0) {
      g_state.sampleIndex = 0;
    }
    g_state.flags.byte =
        ((g_state.flags.byte & SYSTEM_MODE_CLEAR) | SYSTEM_MODE_INTERACTIVE);
    RTC.RTCCR2.BYTE |= 1;
    DisplayExitPowerSave();
  }
}

void MotionSessionEnd(void)
{
  BeepDisableTimer();
  CKSTPR1.BYTE &= 0xfb;
  RTC.RTCCR2.BYTE &= 0xfe;
  g_state.flags.byte &= SYSTEM_MODE_CLEAR;
}

void MotionSessionIdleCheck(void)
{
  if (g_state.idleSeconds[1] == 0) {
    MotionSessionEnd();
  }
}

#pragma interrupt(IRQ0Interrupt(vect = 16))
void IRQ0Interrupt(void)
{
  g_state.events.byte |= EVENT_CENTER_PRESS;
  g_state.buttonWake[0] = 1;
  CKSTPR1.BYTE |= 4;
  IRR1.BYTE &= 0xfe;
}

#pragma interrupt(IRQ1Interrupt(vect = 17))
void IRQ1Interrupt(void)
{
  IRR1.BYTE &= 0xfd;
}

#pragma interrupt(IRQAECInterrupt(vect = 18))
void IRQAECInterrupt(void)
{
  IRR1.BYTE &= 0xfb;
}

#pragma interrupt(ADCInterrupt(vect = 38))
void ADCInterrupt(void)
{
  IRR2.BYTE &= 0xbf;
}

void StoreTotalSteps(u32 value)
{
  u32 current;

  current = g_state.save.totalSteps;
  if (value >= 9999999ul) {
    value = 9999999ul;
  }
  g_state.save.totalSteps = value;
}
