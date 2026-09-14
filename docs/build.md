# Building

Python 3.11+ and Ninja 1.10+ are ordinary host dependencies. The firmware uses
the user-provided Renesas H8 toolchain; Python and Ninja do not compile it.

The initial qualification targets are macOS Arm64 and Windows x86_64. The current
private tree is still undergoing qualification; other host combinations are
unverified.

## Host setup

On macOS, install Python and Ninja with Homebrew:

```sh
brew install python ninja
softwareupdate --install-rosetta
```

Follow Apple's Rosetta license prompt if Rosetta is not installed. Configuration
downloads Wibo 1.2.0 from its upstream release and checks the digest recorded in
`config/host-tools.json`. Its x86_64 Mac executable runs through Rosetta.

On Windows, install [Python](https://www.python.org/downloads/windows/) and enable
its PATH option. In a new PowerShell window:

```powershell
python -m pip install ninja
python --version
ninja --version
```

The compiler runs directly on Windows. No Unix shell is required.

## User inputs

The retail firmware image must be 49,152 bytes with SHA-256:

```text
f9e210a3b74afbbd12c5a66a51cc05cb9fbac986805ff0a3bfb4be6074d15607
```

Configuration checks this identity before extracting graphics and the font.
It accepts these compiler inputs:

| Suite | Compiler | Linker | Updater |
|---|---|---|---|
| 6.02.01 | CH38 6.02.01 | OPTLNK 9.04.01 | `h8v6201u.exe` |
| 6.02.02 | CH38 6.02.02 | OPTLNK 9.05.00 | `h8v6202u.exe` |

Updater SHA-256 values:

```text
h8v6201u.exe
18c9b7005322c48cbd250d87e4716d75785f3501de43dabc094d1cb9a8fd4fcb

h8v6202u.exe
1156344fc66ea831a250f50801096741ddc58625778b6f1a24a47a2435601349

h8v6202u-doc-e.zip (contains h8v6202u.exe)
d2f8b82335ea075df3385a3e77b7a210422559c9f1f8e9a8e3e1308b29ce5de3
```

The updater is unpacked without running its installer. No archive utility is
needed. An existing installation is also accepted: pass its suite directory,
`bin` directory, `ch38.exe` path, or enclosing HEW directory. The importer checks all 105 required
files against `config/toolchains.json`; compiler, linker, headers and runtime
must belong to one qualified suite. A lone compiler executable is insufficient.

## Configure and build

```sh
python configure.py --rom "path/to/retail.bin" --compiler "path/to/h8v6202u.exe"
ninja
```

After the first configuration, omit inputs already imported:

```sh
python configure.py --compiler "path/to/h8v6201u.exe"
ninja
python configure.py --toolchain 6.02.02 --offline
ninja
```

Both suites remain installed. Output goes to `build/<version>/pw.bin`; the
symbol map, producer logs, build identity and verification result are alongside it.
Ninja preserves the link order and compiler settings in `config/build.json`.

The default target verifies the full image every time. Incremental builds
recompile changed sources; header changes conservatively rebuild all firmware
units. For a complete runtime regeneration and rebuild of all 40 units:

```sh
ninja -t clean
ninja
```

`ninja firmware` builds without demanding byte identity, for deliberate
experiments. `ninja verify` builds as needed and performs the normal strict check.

## Runtime and paths

For an existing Wibo executable, use `--wibo PATH` or `PW_WIBO`. Its actual version
and hash are recorded; it need not have the managed download's hash. A different
launcher is not a verified combination merely because it starts successfully.
Use `--managed-wibo` to return to the tested default, or `--offline` to prohibit
downloads during configuration. The build itself never downloads tools.

Checkout and input paths may contain spaces. Old compiler processes receive
private, short ASCII scratch paths; each uses its own temporary files. If the
system temporary directory contains non-ASCII characters, pass `--work-dir PATH`
to an existing writable ASCII directory.

`.local/` contains imported inputs and the host configuration. `build/` contains
generated files. Both are ignored by Git and can be removed to start over.
Reconfigure after moving a checkout or moving it to another host.

## Developer checks

Install clang-format 21.1.8 in your Python environment
(`python -m pip install clang-format==21.1.8`) and install
[Cppcheck](https://cppcheck.sourceforge.io/). Homebrew provides Cppcheck on macOS;
its upstream site provides the Windows installer. Put both executables on PATH.

```sh
python -m tools.check
python -m tools.check --format
```

The first command checks formatting, runs Cppcheck, and runs the eight utility
tests. The second applies formatting only. An existing formatter can be selected
with `--clang-format PATH`. These checks do not need a ROM or compiler input.

Cppcheck is configured for C89/C90 and the H8 normal-mode data widths in
`config/h8.xml`. Correctness, performance and portability diagnostics are enabled;
formatting is handled separately. Cppcheck 2.21.0 was used during qualification.

Four line-specific suppressions cover its incorrect promotion of 16-bit
`unsigned short` to signed `int` on this custom platform. The affected EEPROM
address calculations and capped watt subtraction use unsigned arithmetic on
CH38. The analyzer issue also reproduces with an authored example assigning
`0xE544` to an unsigned short and subtracting `0x224`. The suppressions do not
disable overflow diagnostics elsewhere.

The utility tests cover input identities, extraction bounds, block decoding,
installation discovery, atomic output replacement and complete-image comparison.
The firmware's decisive check remains a fresh build and literal ROM comparison.
