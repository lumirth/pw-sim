#include "flags.h"
#include "types.h"
#include "iodefine.h"
#include "project.h"
#include <machine.h>
#include "pad.h"
#include "power.h"

#define PW_INTERACTIVE_INPUT_IDLE_SECONDS 90

/* Mask interrupts while initializing the four input-state bytes. */
void InputInit(void)
{
  g_state.buttons = 0;
  g_state.previousButtons = 0;
  g_state.pressedButtons = 0;
  g_state.centerHoldTicks = 0;

  set_ccr(0x80);

  PFCR.BYTE &= 0xfc;
  IEGR.BYTE |= 1;
  IRR1.BYTE &= 0xfe;
  IENR1.BYTE |= 1;

  PFCR.BYTE &= 0xf3;
  IEGR.BYTE |= 2;
  IRR1.BYTE &= 0xfd;
  IENR1.BYTE |= 2;

  IO.PDRB.BYTE |= 0x20;
  IO.PDR8.BYTE |= 0x10;
  IO.PCR8 &= 0xef;

  set_ccr(0);
}

/* Pressed edges are rising bits: current & (current ^ previous). */
void InputScan(void)
{
  SystemEvents flags;

  g_state.buttons = 0;

  if (IO.PDRB.BIT.B0 != 0) {
    g_state.buttons |= BUTTON_CENTER;
    if (g_state.buttonWake[0] != 0) {
      ++g_state.centerHoldTicks;
    }
  } else {
    g_state.centerHoldTicks = 0;
  }

  flags.byte = g_state.events.byte;
  if (flags.bits.centerPressed) {
    g_state.buttons |= BUTTON_CENTER;
    g_state.events.byte &= EVENT_CLEAR(EVENT_CENTER_PRESS);
  }

  if (IO.PDRB.BIT.B2) {
    g_state.buttons |= BUTTON_LEFT;
  }
  if (IO.PDRB.BIT.B4) {
    g_state.buttons |= BUTTON_RIGHT;
  }

  {
    union {
      u16 word;
      struct {
        u8 current;
        u8 changed;
      } b;
    } edge;

    edge.b.changed = g_state.buttons;
    edge.b.current = g_state.previousButtons;
    edge.b.changed ^= edge.b.current;
    edge.b.current = g_state.buttons;
    edge.b.current &= edge.b.changed;
    g_state.pressedButtons = edge.b.current;
  }
  g_state.previousButtons = g_state.buttons;

  if (g_state.pressedButtons != 0) {
    g_state.idleSeconds[0] = PW_INTERACTIVE_INPUT_IDLE_SECONDS;
    g_state.sampleIndex = 0;
    if ((g_state.flags.byte & SYSTEM_MODE_MASK) != SYSTEM_MODE_INTERACTIVE) {
      g_state.pressedButtons = 0;
    }
  }

  if ((g_state.centerHoldTicks >= 8) &&
      ((g_state.flags.byte & SYSTEM_MODE_MASK) != SYSTEM_MODE_INTERACTIVE)) {
    MotionSessionStart();
    g_state.flags.byte |= SYSTEM_SOCIAL_OFFER;
  }
}

u8 InputPressed(u8 requestedMask)
{
  return (requestedMask & g_state.pressedButtons);
}
