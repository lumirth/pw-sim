# Pokéwalker browser port

This experimental source fork compiles the firmware C directly to WebAssembly.
There is no H8 instruction interpreter. Browser code supplies hardware boundaries,
physical input, rendering, audio output, and inspection controls.

## Run

From the repository root:

```sh
npm run setup --prefix web
npm run build --prefix web
npm test --prefix web
npm start --prefix web
```

Open <http://127.0.0.1:8765/>. The server binds to loopback. To use another port:

```sh
npm start --prefix web -- --port 8767
```

Requirements: Python 3.11+, Node.js 22.12+, and LLVM Clang with the WebAssembly
linker. This build was verified with LLVM Clang 23.1.1. On this Mac the compiler
is `/opt/homebrew/opt/llvm/bin/clang`. Set `PW_WASM_CC` to select another compiler.
The browser port does not need the original Windows compiler or Rosetta.

For UI development, run `npm run dev --prefix web`; Vite uses port 8766. After a
C change, run the complete build. After a worker or physics change, run
`npm run build:ui --prefix web` so those static modules reach the served site.
The complete build generates register storage, EEPROM conversion tables,
instrumented C copies, source views, WebAssembly, and UI assets from this tree.

## Local inputs

Eight 64 KiB EEPROM images were extracted from the supplied `rom-and-eeprom.zip`
into `web/fixtures/`. The supplied GLB is `web/assets/pokewalker.glb`. These inputs
remain local and are excluded from Git. A fresh checkout needs these inputs to
reproduce all checks. Fixture names are file labels and do not identify the
Pokémon stored in an image.

The expected fixture names are listed in `web/verify.mjs`. A blank EEPROM can
also be selected in the UI. It follows the firmware's unpaired-device path;
local assets are needed for a populated walk.

Select **Import** to load an exact 65,536-byte EEPROM image. **Export** runs the
native save routine, which updates both save mirrors and their checksums, then
provides **Save EEPROM**. **Memory → Export stored EEPROM** preserves the current
stored bytes without committing the live save first. Files remain in the
browser. The save link contains the complete file as a data URL; hosts that
restrict file downloads may require opening the site in a normal browser.

## Controls

- Left drag moves the device. **Firm hold** maintains its pickup orientation.
  **Point grip** applies force at the shell point selected and allows it to turn.
  **Grip response** changes the hand target's spring strength. Release retains
  velocity. Leaving the scene, losing pointer capture, or switching windows
  releases the grip.
- Right drag orbits the camera. The wheel controls zoom. The corresponding
  numeric controls remain available in **Scene → View**.
- **Pendulum** uses a slack-capable string. **Free motion** allows lifting,
  dropping, and full three-axis rotation. **Fixed** provides a stationary view.
  **Keep screen forward** is an optional rotational constraint.
- **Screen up**, **Turn upright**, and **Reset device position** recover the view.
  **Twist one turn** stores elastic twist in the string. It unwinds under torque.
- The fan has air speed, direction, height, gust strength, sweep angle, and
  oscillation rate. The fan head and the simulated flow use the same direction.
- The three device buttons use the native debounce logic. Keyboard controls are
  Left, Enter, and Right. Hold the centre button when the native LCD is asleep.
  Select **Enable audio** to permit Web Audio output.
- **Reset settings** restores scene, physics, and playback defaults. It leaves
  firmware progress intact. Fan, view, and light also have separate reset buttons.
  **Reset firmware** reloads the last imported or selected EEPROM.

## Time and inspection

At 1×, the cooperative foreground runs at nominal 16 Hz, LCD requests occur at
4 Hz, and the RTC advances at 1 Hz. The fixed physics step is 1/256 second.
The worker sends poses at 60 Hz; rendering follows the display refresh rate.
The browser is not a cycle-accurate model of H8 instruction execution. Timer W
note transitions use a separate microsecond timeline with a fractional 32,768 Hz
clock remainder. The audio scheduler preserves sub-frame rests and note changes;
a synthetic piezo response filters the output. The 1× audio lead is 92.5 ms and
increases as needed for slow playback.

Playback supports 0.125× through 16×. Audio tempo and pitch scale with playback.
Pause stops device time; single-step advances to the next 62.5 ms boundary.
Hidden tabs pause instead of discarding a backlog of hardware intervals.

**Rewind** retains up to 32 full-memory states at one-second intervals. A state
includes native memory, input state, physical state, and timer phase. Restore
pauses the instance. Resume starts a new history branch. Manual checkpoints
retain up to eight states within the tab.

**History → Run six branches** creates six independent Wasm instances from one
native memory image. They each advance 12 device seconds with different button
or motion input. Their LCD frames, view paths, and step results are compared at
one-second intervals. The live instance continues separately.

**Motion** shows the actual native accelerometer ring, FFT spectrum, cadence,
step fraction, and conversion to Watts. **Memory** shows the shared workspace
and EEPROM accesses. **Source** shows C entry counters and the compiled source
locations, including calls that LLVM inlines. These are measurements from the
running compiled program, not prerecorded UI animations.

## Port boundary

`web/build.py` compiles 35 C translation units with a freestanding Clang target.
The resulting module imports no JavaScript functions and has 4 MiB of linear
memory. The original application controllers, rendering, save code, FFT, step
logic, score decoder, and cooperative task functions execute in that module.

`web/backend.c` replaces memory-mapped hardware and supplies LCD capture,
EEPROM storage, button lines, accelerometer samples, battery status, timer
scheduling, and an offline IR timeout. `web/runtime-worker.js` owns pacing,
input, snapshots, audio event transport, and UI readouts. `web/futures-worker.js`
owns comparison instances. `web/physics.js` supplies physical input using
Cannon-es. `web/client/src/` contains the React/shadcn UI, Three.js renderer, and
Web Audio scheduler.

Port changes use explicit integer widths, host register storage, corrected
bitfield layout, and native scratch offsets in place of truncated H8 pointers.
Typed EEPROM reads and writes convert numerical fields while preserving the raw
big-endian storage format and byte assets. The LLVM build intentionally changes
the ABI and binary layout; it is not a matching H8 build.

## Physical model and appearance

[Nintendo's specification](https://www.nintendo.co.jp/ds/ipkj/qa/catC.html) gives
48.0 mm diameter, 14.7 mm thickness including buttons, 20.6 g including the
CR2032 battery, and 21.9 g with the belt clip. The reflective four-level LCD has
96 × 64 active pixels in a 24 × 16 mm area. Those values set the default scale,
collider, mass, and active display region. The original GLB screen geometry
carries the live LCD texture, including its inactive border.

The supplied shell and button meshes have vertex normals opposite to almost all
triangle faces (2,515/2,516 shell faces and 864/864 button faces). The renderer
corrects those normals before lighting. Plastic materials keep the original
albedo textures with nonmetallic reflection and a small clearcoat highlight.
A directional sun casts contact shadows; sky and ground provide ambient fill.

The default device has its belt clip attached and a total mass of 21.9 g.
Point grip is the default mouse mode. The initial string is 10 cm of 1 mm twine.
All physics defaults live in `web/physics-defaults.js`; startup and resets share
that definition. Surface presets also set estimated friction and rebound.

The collider is a 32-sided cylinder with an optional clip box. Inertia allocates
the 1.3 g clip contribution to an offset box and treats the main body as a uniform
cylinder. The rotation centre remains at the shell centre. The real device's measured mass does not
establish its mass distribution. Contact friction, restitution, damping,
aerodynamic coefficients, and string material properties are adjustable estimates.
Air forces use relative velocity and projected area at four body regions, with
1.225 kg/m³ air density. This is not a fluid solver or a calibrated fan.

The twine model is massless, with a tension-only distance limit. It can go slack and
stores continuous torsional displacement across full revolutions. A torsional
spring and internal damping torque unwind it. The default material estimate is
3 µN·m/rad at 10 cm and 1 mm diameter; stiffness scales with inverse length and
the fourth power of diameter. This is a flexible cotton-twine approximation,
not a measurement of a particular cord. Three matte, helical plies follow the slack curve at the selected twine diameter. Knots, rope self-contact, and bending stiffness are not simulated.

Swing loses energy where taut twine bends at its fixed support. The equivalent
rotational viscous coefficient is 200 µN·m·s/rad for 1 mm twine, scaled by diameter
to the fourth power. Tangential resistance at the body attachment is
`F = -C * v_tangent / length²`. An implicit impulse limits the resistance at each
physics step so high damping cannot reverse velocity and add energy. Slack cord
applies no bending resistance. This coefficient is a tunable material estimate;
it does not establish a match to a physical twine sample. The **Bend damping**
control adjusts it and **Twine loss** shows its instantaneous power.
Extra whole-body damping defaults to zero; aerodynamic drag is still active.
A 28.65° release on the default 10 cm cord has a peak displacement of 0.94 mm
during the sixteenth second, compared with 18.62 mm before the attachment loss
was added. This is a regression scenario, not hardware validation.

The sensor receives body-local specific force at the simulated centre of mass;
it is quantized to signed counts. Actual sensor mounting and transfer response
have not been measured. Walking mode supplies a repeatable synthetic gait with 36 mm vertical amplitude
and a default cadence of 2 Hz; the displayed displacement and acceleration use
the same amplitude and phase.

## Verification and provenance

The untouched source snapshot was built with CH38 6.02.02 / OPTLNK 9.05.00 before
this fork was changed. Its retail comparison was **49,152 / 49,152 bytes identical**,
with SHA-256:

```
f9e210a3b74afbbd12c5a66a51cc05cb9fbac986805ff0a3bfb4be6074d15607
```

The initial snapshot archive, manifest, and baseline producer evidence are retained
beside this temporary repository. `web/generated/baseline.json` records the result.
The browser build has its own compiler, source list, and Wasm hash in
`web/generated/build.json`.

`npm test --prefix web` checks:

- Eight EEPROM images boot, draw, and accept button input; blank EEPROM takes
  the offline connection path.
- Native gait processing emits steps and Watts; exported save mirrors keep the
  expected byte order and checksums and reimport correctly.
- Full-memory replay and independent instances are deterministic under the
  checked inputs. Worker pacing remains 16 native ticks and about 60 poses per
  second at 1×, with correct accelerated time and rewind behavior.
- Rigid-body toppling, ground contact, mass-dependent wind response, fixed mode,
  slack and tension, grip modes, settled holding, twist decay, and fan sweep.
  Twine release checks cover decay, absent damping during slack and free flight,
  and energy stability with the thickest cord and lightest body in the controls.
- All 16 supplied sound-bank sequences produce native tone events. A 9,796 µs
  rest survives transport into the actual audio scheduler. Normal speed, slow
  playback, and recovery from a clock stall are exercised.

Browser checks cover the live LCD, source and memory views, frame pacing,
import and the complete export-link payload, native checkpoint restore and
single-step, playback, branches, scene presets, and settings. A file-download
completion notification was not exposed by the embedded browser during testing.

This is an experimental port. IR communication is replaced with a three-second
offline timeout. Battery output is a healthy-battery stub. Sensor response,
piezo acoustics, exact execution timing, and every firmware path have not been
qualified against physical hardware.

Relevant implementation references: [Cannon-es](https://github.com/pmndrs/cannon-es),
[Three.js](https://threejs.org/docs/), [shadcn/ui](https://ui.shadcn.com/docs/installation/vite),
and the [Renesas H8/38602R family hardware manual](https://www.renesas.com/en/document/mah/h838602r-group-hardware-manual?r=1052456)
for Timer W clock selection. The family manual supports the timer interpretation;
it is not evidence of complete H8/38606 hardware qualification.
