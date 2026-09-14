# Pokéwalker

A C reconstruction of the Pokéwalker firmware, intended to reproduce the
complete 49,152-byte retail image while making its behavior readable.

The project is being prepared privately. Source and build qualification are
still in progress.

## Build

Install Python 3.11 or newer and Ninja. On macOS, install Rosetta 2 as well.
Supply your retail firmware image and a supported Renesas H8 compiler suite:

```sh
python configure.py --rom path/to/retail.bin --compiler path/to/h8v6202u.exe
ninja
```

Use `python3` if that is your Python command. Configuration accepts a complete
6.02.01 or 6.02.02 installation, the corresponding updater executable, or the
documented 6.02.02 ZIP. On macOS it downloads a tested Wibo release. Subsequent
builds work offline.

The default build compares every output byte with the supplied ROM and fails
on any difference. Success prints:

```text
IDENTICAL: 49,152 / 49,152 bytes
```

Retail firmware, extracted graphics and compiler files are not distributed here.
See [build instructions](docs/build.md) for host setup and exact input identities,
and [source notes](docs/source.md) for organization and conventions.
