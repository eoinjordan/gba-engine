# GBA Engine

A small, portable C runtime for Game Boy Advance homebrew, built around
**devkitARM**. It provides a hardware-abstraction layer (HAL), a Mode 0
tiled-background renderer, scene/actor state management, and a minimal
bytecode VM for scripted game behaviour.

This engine started life as the runtime backend for [GBA Studio](https://github.com/eoinjordan/GBA-Studio)
(a visual, drag-and-drop GBA game maker forked from gb-studio), and was
pulled out into its own repo so it can be reused or embedded in other GBA
projects independent of the studio's editor/compiler.

## What's here

```
include/   Public headers — HAL, types, scene/VM definitions
src/       Engine source — boot, rendering, scene/actor management, VM
tests/     Host-side unit tests for the portable VM/script-runner
.github/   CI workflow — unit tests + devkitARM ROM build
Makefile   devkitARM build + host test runner
gba.ld     Linker script
engine.json  Field/metadata definitions consumed by GBA Studio's compiler
```

### Architecture

- **`gba_system.{c,h}`** — Hardware-abstraction layer: boot/init, VRAM and
  palette access, vblank sync, key input, DMA copy/fill.
- **`engine.{c,h}`** — Engine loop (`engine_init`/`engine_update`/`engine_render`/`engine_run`),
  Mode 0 tiled background rendering (palette + tile + tilemap loading),
  scene state, and actor spawn/destroy/update.
- **`vm.{c,h}`** — Minimal bytecode interpreter (`script_execute`, `script_runner_*`)
  driving scene loads and palette "tone" changes from compiled scripts.
- **`gba_types.h` / `gba_scene.h` / `gbs_types.h` / `bankdata.h`** — Shared
  types: screen/map dimensions, actor structs, scene definitions, compiled
  game-data layout.

## Status

This is an early-stage runtime. It currently boots the GBA, renders a
single Mode 0 tiled background, manages scene/actor state, and runs basic
scripted scene transitions. The following subsystems are still being
ported/built out as separate phases:

- [ ] Text rendering (dialogue boxes, labels)
- [ ] Full actor scripting (movement, interactions, conditionals)
- [ ] Scene triggers and events
- [ ] Real background tile graphics from compiled art (vs. solid/checker test tiles)
- [ ] Sprite rendering via OAM
- [ ] Audio (GBA APU / DirectSound)
- [ ] Save/load system

## Building

Requires the [devkitPro](https://devkitpro.org/) toolchain with **devkitARM**
installed (`arm-none-eabi-gcc`, `gba.specs`, `gbafix`).

```sh
make
```

This produces `bin/game.gba` — a flashable/emulatable GBA ROM.

## Testing

Two layers of testing back this engine, mirroring the split between
"hardware-facing" and "portable" code described above:

- **Unit tests (host-side, fast).** The bytecode VM/script-runner
  (`src/vm.c`) is plain, portable C with no hardware dependencies, so it's
  compiled and run directly with the host compiler — no devkitARM or emulator
  needed. These cover context allocation/recycling, opcode dispatch,
  `WAIT`/exception handling, and concurrent scripts.

  ```sh
  make test
  ```

  Test sources live in `tests/`: `test_framework.h` is a tiny single-header
  assertion framework (no external dependencies), `test_stubs.{c,h}` provide
  recording test doubles for the engine-side hooks the VM calls into
  (`vm_scene_load`, `vm_scene_set_tone`), and `test_vm.c` is the suite itself.

- **Build verification (devkitARM).** CI also builds an actual `.gba` ROM
  with the real toolchain and checks that `gbafix` produced a well-formed
  header. This is the only way to catch register/ABI/linker issues that host
  unit tests can't see — but it doesn't tell you *what's on screen*, which
  is why the VM logic above is tested independently of rendering.

Both run automatically in CI on every push and PR (see
`.github/workflows/ci.yml`).

### Porting tests back into GBA Studio

Because `tests/` only depends on `include/vm.h` and `src/vm.c` (plus the
host compiler), it can be copied wholesale into GBA Studio's
`appData/engine/gbavm/` alongside the engine sources it tests — just wire
up an equivalent `make test` target there and the same suite runs in place.

## Using this engine elsewhere

The engine is plain C with no dependencies beyond devkitARM's libgba/libc.
To embed it in your own project:

1. Copy `include/` and `src/` (minus `main.c`, which is just a thin entry
   point) into your project.
2. Provide your own compiled game data matching the layout in `bankdata.h`
   / `gba_scene.h` (or supply a `data/gba_scene_data.h` — the engine falls
   back to a minimal built-in scene if one isn't present).
3. Call `engine_run()` from your `main()`.

## License

MIT — see [LICENSE](LICENSE).
