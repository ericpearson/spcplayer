# spcplayer

A command-line SNES SPC700 music player. Loads `.spc` files and plays them back by emulating the SPC700 CPU and S-DSP.

## Features

- SPC700 CPU emulation
- S-DSP emulation (BRR decoding, ADSR/GAIN envelopes, echo, noise, pitch modulation)
- ID666 metadata parsing and display
- Resampling from 32kHz to 48kHz output
- Pseudo-surround sound (`-s`)
- Bass boost (`-b`)

## Building

Requires CMake and SDL2.

```sh
cmake -B build
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
