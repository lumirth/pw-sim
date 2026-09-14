# Source

The firmware runs on the H8/38606 in normal mode: 8-bit bytes, 16-bit integers
and pointers, and 32-bit longs. It uses C90 with the CH38 extensions needed for
interrupts, sections, bitfield order and machine instructions.

## Organization

The source and headers are flat. Start with these modules:

| Area | Files |
|---|---|
| Startup, interrupts, foreground dispatch | `resetprg.c`, `intprg.c`, `interface.c` |
| Runtime and shared memory | `state.c`, `runtime_state.h`, `state.h` |
| Motion and step counting | `accelerometer.c`, `fft.c`, `motion.c`, `power.c` |
| Serial EEPROM and stored records | `m95512.c`, `eeprom.c`, `save.h`, `records.h`, `resources.h`, `eeprom_map.h` |
| Infrared communication | `irc.c`, `ir.h`, `ir_state.h` |
| Display, buttons, sound and clock | `lcd.c`, `common.c`, `pad.c`, `beep.c`, `rtc.c` |
| Views and activities | `home.c`, `option.c`, `list.c`, `battle.c`, `dowsing.c`, `poketrace.c`, `feeling*.c`, `friend.c` |
| Factory and device diagnostics | `factory.c`, `selftest.c`, `selftest_data.c` |

`g_state` holds persistent application state and interrupt-shared values.
`g_ui.view` overlays the active view's state in one 18-byte bank.
`g_work` overlays motion, sound and IR workspace. These are shared storage,
so changing views does not imply clearing or reallocating memory.

EEPROM resource structures describe offsets in serial storage. Their pointer
expressions calculate addresses passed to the EEPROM driver; they do not make
EEPROM directly accessible through the CPU address space. Artwork and the font
are extracted from the supplied ROM into an ignored generated header.

The save record uses native H8 counters. Console-produced records keep their
serialized byte order, including the little-endian words in the 16-byte Pokémon
summary. Preserve those bytes when forwarding records; decode them before
numeric calculations. `fixedFacing` keeps a Pokémon's supplied sprite facing;
renderers request horizontal mirroring when it is clear.

A small Pokémon animation contains two 32×24 frames of 192 bytes each; a large
animation contains two 64×48 frames of 768 bytes each. Transfer lengths can
extend beyond a displayed frame. The battle renderer preserves its 384-byte
read even though it displays only one small frame.

Sound selection and playback have separate lifetimes. `BeepLoadScore` loads a
numbered EEPROM score; `BeepSelectScore` selects a resident score. The foreground
enables the timer while a score is selected and disables it after the score
ends. Score pitch bytes index timer compare values.

## Conventions

Use concise PascalCase functions and types, lowerCamelCase locals and fields,
`g_` globals, and uppercase constants. Indent by two spaces. Function braces
start on the next line; control braces stay on the same line. These are the
project's period-informed conventions, not recovered original formatting.

Names should communicate purpose and retain useful units. Use decimal for human
quantities, hexadecimal for register values and bit patterns, and named constants
or structural expressions for established sizes and offsets. Preserve literal
types and suffixes. Comments explain behavior that names cannot make clear.

Run the formatter and linter described in [the build instructions](build.md).
The supplied HEW device header keeps its original formatting.

## Preserving behavior

Matching takes priority over cleanup. CH38's integer promotions, signed shifts,
bitfield allocation, alignment, volatile accesses, argument passing and optimizer
decisions are part of this build. Keep declarations consistent with their callers.
Changes to local lifetimes, casts, expression grouping or `const` can affect the
output even when the intended algorithm is unchanged.

Target-specific assumptions are concentrated in scalar and layout declarations,
the device header, hardware drivers and build configuration. A future compiler
port would need to review those assumptions; no alternative compiler is currently
qualified.

## Remaining uncertainties

- The IR-request event bit starts communication and defers queued RTC work, but
  no ordinary setter is present in this firmware.
- The motion batch's `pendingStepsQ9` is consumed and cleared without an
  established nonzero producer. `reservedResetByte` is only cleared.
- The threshold-test `resetWord` receives its startup value but has no
  established subsequent consumer.
- Reserved bytes and bits preserve their storage, initialization and transfer
  behavior. These include the six-byte event-item prefix, the peer record's
  leading word, and the Pokémon summary's separately cleared appearance bit.
  Their particular meanings remain unresolved.

The complete image comparison establishes byte identity. It does not recover
unique original source spelling or establish how frequently a device encounters
each behavior.
