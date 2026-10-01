# Tang Nano 20K engine build

This platform compiles GBA-engine and Studio's generated C game data for
the Nano 20K's RV32IM soft CPU. It uses the `studio_lcd` FPGA project in
GBA-FPGA and the same 480x272 RGB565 panel as the Game Boy LCD target.
The game view is 240x160, centred at native resolution.

An ARM `.gba` ROM remains a separate export. The Tang runs `game.tang.bin`;
it does not emulate an ARM CPU or execute commercial GBA ROMs.

From GBA Studio, build the CLI once with `npm run make:cli`, then export:

```sh
node out/cli/gb-studio-cli.js export path/to/project.gbsproj out/tang-data --target gba
```

Compile using LLVM (clang, LLD and llvm-objcopy):

```sh
python platform/tangnano20k/build.py --data ../gba-studio/out/tang-data --out bin/tang-game
```

Set `--llvm` to the LLVM `bin` directory when it is not on PATH. Windows
also checks `C:/Program Files/LLVM/bin`. These host scripts work on
Windows, Linux and macOS. Gowin FPGA synthesis requires Windows or Linux.
`bin/tang-game/build.json` records the firmware hash and source hashes.

The FPGA exposes the engine's palette, VRAM and OAM addresses through
SDRAM. Mode 0 registers are local. The hardware renderer supports the
engine's 4bpp BG0 world, BG1 dialogue, tile flips, scrolling, RGB555
palettes, and regular sprites, including 8x16 objects. Affine objects,
8bpp tiles, blending, audio and persistent saves are not implemented.
The framebuffer is write-only and single-buffered; tearing is possible.
The engine detects renderer ID `0x54475231` and starts a draw through
`0x80000008`. It falls back to the software renderer on earlier platform
images. During a hardware draw, the renderer owns the SDRAM bus and the
CPU waits; palette and OAM reads use an on-chip shadow.

On the Nano 20K, the Studio starter title screen measured 57.9 game frames
per second. Its menu scenes measured about 29 fps. The LCD raster remains
58 Hz; scenes requiring more draw/update time can repeat a displayed frame.

The renderer's palette and row buffers use 2 KiB of on-chip scratch RAM.
Code, game assets, stack, and remaining state use SDRAM. The linker reserves
896 KiB for code/data/stack and rejects an image that exceeds the budget.
No SD card is required. Firmware and save state are volatile.

Run `make test-host test-tang-renderer` to check VM/engine behaviour and
rendering. The renderer test checks tile flips, scroll, background/OBJ
transparency, sprite priority, 8x16 objects and framebuffer boundaries.
