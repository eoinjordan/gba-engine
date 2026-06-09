# GBA Engine

A small, portable C runtime for Game Boy Advance homebrew, built around
**devkitARM**. It provides a hardware-abstraction layer (HAL), a Mode 0
tiled-background renderer, scene/actor state management, and a minimal
bytecode VM for scripted game behaviour.

This engine started life as the runtime backend for [GBA Studio](https://github.com/eoinjordan/GBA-Studio)
(a visual, drag-and-drop GBA game maker forked from gb-studio), and was
pulled out into its own repo so it can be reused or embedded in other GBA
projects independent of the studio's editor/compiler.

**GBA Studio now consumes this repo as a git submodule**, mounted at
`appData/engine/gbavm` (see `.gitmodules` in that repo). The studio's
editor/compiler paths are unchanged — they still point at
`appData/engine/gbavm` — but the engine source of truth lives here, and
GBA Studio just pins a commit of it. That means: changes meant to ship in
GBA Studio land here first (with host tests), then GBA Studio bumps its
submodule pointer to pick them up. See "Porting tests back into GBA
Studio" below for how the test suite travels with it.

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
  palette access, vblank sync, key input, DMA copy/fill, BG scroll registers.
- **`engine.{c,h}`** — Engine loop (`engine_init`/`engine_update`/`engine_render`/`engine_run`),
  Mode 0 tiled background rendering (palette + tile + tilemap loading),
  scene state, actor spawn/destroy/update, camera follow, and
  collision-aware actor movement.
- **`vm.{c,h}`** — Bytecode interpreter (`script_execute`, `script_runner_*`):
  context management/concurrency plus an opcode set covering scene loads,
  palette "tone" changes, waits, 256 signed-16-bit variables, math
  (set/copy/add/sub/random), and control flow (relative jumps and
  variable-comparison branches) — enough for a visual editor's "set
  variable", "if/else", and "repeat" events to compile down to.
- **`camera.{c,h}`** — Pure follow/clamp math for scrolling a viewport
  around a scene larger than one screen (centres on the player, clamps to
  scene bounds, locks axes that fit within the viewport).
- **`collision.{c,h}`** — Pure tile-map collision math: rectangle-vs-tilemap
  overlap testing and per-axis "slide along walls" movement resolution.
- **`movement.{c,h}`** — Pure per-actor movement-pattern math (GB Studio's
  "movement types"): patrol (pace back and forth across a bounds rectangle,
  reversing at each edge) and follow (walk toward a target on each axis
  independently while within range, without overshooting). Logic is done
  and tested; not yet wired into `engine_update` pending new compiled-data
  fields (move speed, patrol/follow bounds) — see Status below.
- **`trigger.{c,h}`** — Pure scene-trigger geometry: axis-aligned overlap
  testing between an actor and rectangular trigger zones, plus a
  deterministic first-match scan over a scene's zones. Logic is done and
  tested; not yet wired in pending a finalised compiled trigger-zone format.
- **`text.{c,h}`** — Pure dialogue-text helpers: `{N}`-placeholder variable
  substitution and greedy word-wrap to a fixed character width.
- **`savegame.{c,h}`** — Pure save-data encode/decode: a fixed-size,
  versioned, checksummed byte record covering scene index, player position,
  and all VM variables, with explicit little-endian field encoding and
  defensive decoding (bad size/magic/version/checksum is reported as
  failure rather than handed back as if valid).
- **`gba_types.h` / `gba_scene.h` / `gbs_types.h` / `bankdata.h`** — Shared
  types: screen/map dimensions, actor structs, scene definitions, compiled
  game-data layout.

## Status

This is an early-stage runtime, being built out in phases tracked against
GB Studio's GBDK + gbvm stack. It currently boots the GBA, renders a single
Mode 0 tiled background, manages scene/actor state, scrolls a camera around
scenes larger than one screen, moves actors with tile-collision-aware
"slide along walls" physics, and runs scripts through a VM with variables,
math, conditionals, and control flow. The following subsystems still need
their hardware-facing halves (rendering, audio, SRAM) — the portable logic
underneath several of them already exists and is unit-tested (see below):

- [ ] Text/dialogue *rendering* (font/glyph tiles, text boxes, choices) —
      variable substitution and word-wrap logic done (`text.{c,h}`)
- [x] Actor scripting fundamentals (variables, math, conditionals, control
      flow VM opcodes) — movement/interaction opcodes still to come
- [ ] Scene triggers and events — overlap-detection logic done
      (`trigger.{c,h}`); needs a compiled trigger-zone data format and
      VM/engine wiring to actually run a zone's script on entry
- [ ] Real background tile graphics from compiled art (vs. solid/checker test tiles) —
      camera/scroll done (`camera.{c,h}`)
- [ ] Sprite rendering via OAM
- [ ] Audio (GBA APU / DirectSound)
- [ ] Save/load *hardware* (SRAM HAL, opcodes, menu UI) — record format
      done (`savegame.{c,h}`)
- [x] Collision-aware actor movement (`collision.{c,h}`)
- [ ] Movement *types* (static/patrol/follow) — velocity logic done
      (`movement.{c,h}`); needs new per-actor compiled-data fields (move
      speed, patrol/follow bounds distinct from the collision hitbox) before
      it can drive `engine_update`

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

- **Unit tests (host-side, fast).** Several engine subsystems are
  deliberately split into a "portable, hardware-free logic" half and a
  "talks-to-real-hardware" half — the VM/script-runner (`src/vm.c`), camera
  follow/clamp math (`src/camera.c`), tile collision math (`src/collision.c`),
  per-actor movement-pattern math (`src/movement.c`), scene-trigger overlap
  geometry (`src/trigger.c`), dialogue text helpers (`src/text.c`), and
  save-data encode/decode (`src/savegame.c`) are all plain, portable C with
  no hardware dependencies, so they're compiled and run directly with the
  host compiler — no devkitARM or emulator needed. The suite covers VM
  context allocation/recycling, opcode dispatch (including variables, math,
  and control-flow opcodes), `WAIT`/exception handling, concurrent scripts,
  camera centring/clamping, tile-map collision detection and wall-sliding
  movement resolution, patrol/follow velocity computation, trigger-zone
  overlap and first-match scanning, placeholder substitution and word-wrap,
  and save-record round-tripping plus every corruption-rejection path.

  ```sh
  make test
  ```

  Test sources live in `tests/`: `test_framework.h` is a tiny single-header
  assertion framework (no external dependencies), `test_stubs.{c,h}` provide
  recording test doubles for the engine-side hooks the VM calls into
  (`vm_scene_load`, `vm_scene_set_tone`), and `test_vm.c` is the suite itself.

- **Integration tests (host-side, fast).** `engine.c` is exercised on the host
  with fake VRAM/register state and synthetic compiled scene data. This covers
  scene rendering, palette changes, actor updates, and the VM-to-engine bridge
  without needing devkitARM or an emulator.

  ```sh
  make test-integration
  ```

- **Build verification (devkitARM).** CI also builds an actual `.gba` ROM
  with the real toolchain and checks that `gbafix` produced a well-formed
  header. This is the only way to catch register/ABI/linker issues that host
  unit tests can't see — but it doesn't tell you *what's on screen*, which
  is why the VM logic above is tested independently of rendering.

- **Emulator smoke test (end-to-end).** CI boots the built ROM under mGBA with
  dummy SDL drivers and requires it to stay alive for a short window without
  crashing immediately.

  ```sh
  make test-e2e
  ```

Both run automatically in CI on every push and PR (see
`.github/workflows/ci.yml`).

### This suite travels with GBA Studio via the submodule

`tests/` only depends on a handful of portable headers/sources
(`vm.{c,h}`, `camera.{c,h}`, `collision.{c,h}`, `movement.{c,h}`,
`trigger.{c,h}`, `text.{c,h}`, `savegame.{c,h}`) plus the host compiler —
and now that GBA Studio mounts
this repo at `appData/engine/gbavm` as a git submodule, the suite simply
*comes along for the ride*. No copying or syncing required: whatever
commit GBA Studio's submodule pointer is pinned to, that's the exact test
suite (and engine source) available at `appData/engine/gbavm/`. Wiring up
an equivalent `make test` target in GBA Studio's build tooling runs the
very same suite in place, against the very same sources.

Practically, this means the workflow for engine changes is:

1. Land the change here (in `gba-engine`), with host tests, and push.
2. In GBA Studio, bump the submodule pointer
   (`git -C appData/engine/gbavm pull && git add appData/engine/gbavm`)
   to the new commit.
3. Run `make test` from GBA Studio's tooling to confirm the suite still
   passes against the pinned commit before committing the bump.

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
