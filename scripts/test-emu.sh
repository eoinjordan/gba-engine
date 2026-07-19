#!/usr/bin/env bash
set -euo pipefail

rom_path="${1:-bin/game.gba}"
timeout_secs="${GBA_ENGINE_EMULATOR_TIMEOUT_SECS:-10}"
emulator="${GBA_ENGINE_EMULATOR:-mgba}"

if [[ ! -f "$rom_path" ]]; then
  echo "[emu] ROM not found: $rom_path" >&2
  exit 1
fi

if ! command -v "$emulator" >/dev/null 2>&1; then
  echo "[emu] Emulator not found: $emulator" >&2
  exit 1
fi

echo "[emu] $emulator $rom_path"

set +e
SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" \
SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}" \
timeout "${timeout_secs}s" "$emulator" "$rom_path"
status=$?
set -e

if [[ "$status" -eq 0 ]]; then
  echo "[emu] Emulator exited successfully."
  exit 0
fi

if [[ "$status" -eq 124 ]]; then
  echo "[emu] Emulator stayed alive for ${timeout_secs}s."
  exit 0
fi

echo "[emu] Emulator exited with code $status." >&2
exit "$status"
