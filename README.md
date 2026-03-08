# spcplayer

[![Build](https://github.com/ericpearson/spcplayer/actions/workflows/build.yml/badge.svg)](https://github.com/ericpearson/spcplayer/actions/workflows/build.yml)

A command-line SNES SPC700 music player. Loads `.spc` files and plays them back by emulating the SPC700 CPU and S-DSP.

## Features

- SPC700 CPU emulation
- S-DSP emulation (BRR decoding, ADSR/GAIN envelopes, echo, noise, pitch modulation)
- ID666 metadata parsing and display
- Resampling from 32kHz to 48kHz output
- Pseudo-surround sound (`-s`)
- Bass boost (`-b`)

## Building

### Linux / macOS

Requires CMake and SDL3.

```sh
cmake -B build
cmake --build build
```

### Windows (MSYS2 / UCRT64)

Install [MSYS2](https://www.msys2.org/), then from a UCRT64 shell:

```sh
pacman -S mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-sdl3
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

## Usage

```
spcplayer [options] <file.spc>
```

### Options

| Flag | Description |
|------|-------------|
| `-s`, `--surround` | Enable pseudo-surround sound |
| `-b`, `--bass` | Enable bass boost |

### Controls

- `Ctrl+C` — Quit

## License

GPLv3 — see [LICENSE](LICENSE).
