# Pokéwalker browser port

An isolated experiment that recompiles the Pokéwalker C firmware to WebAssembly.
The firmware runs in a browser worker. Three.js renders the supplied device model;
a rigid-body simulation provides motion input to the original FFT and step code.

```sh
npm run setup --prefix web
npm run build --prefix web
npm test --prefix web
npm start --prefix web
```

Open http://127.0.0.1:8765/. The build requires LLVM Clang with WebAssembly support,
Python 3, and Node.js. The local EEPROM fixtures and supplied model are already
present in this working directory and are excluded from Git.

See [build and port notes](web/README.md) for controls, input setup, architecture,
verification, and limits. [Original release notes](docs/original-release.md)
describe the source baseline. The experimental source no longer targets a
byte-identical H8 ROM.
