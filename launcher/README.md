# FiveInvader Desktop Launcher

Electron desktop UI for configuring and starting the native FiveInvader engine. The renderer is React + TypeScript + Vite with Tailwind CSS and Radix UI primitives. The Electron preload exposes a limited IPC API; the renderer never receives Node access.

## Run

```sh
npm install
npm run dev
```

For a production renderer/main build, run `npm run build`, then `npm start`. The native engine remains an external executable. By default the launcher expects:

- `../FiveInvader/build/FiveInvader/FiveInvader` relative to the launcher source tree
- `../rom_data/extracted_disc2` as the game directory

Override those locations with `FIVEINVADER_ENGINE_PATH` and `FIVEINVADER_GAME_DIR` when starting the launcher. The game directory must contain `default.xex`. The file manifest lists root-level RPF archives and sizes; without known-good hashes, it does not claim cryptographic integrity.

## Engine flags

The launch bridge passes `--game-dir`, optional `--config`, `--lang`, `--res`, `--vsync 0|1`, `--fullscreen 0|1`, and `--fps` as individual process arguments (no shell). FPS `0` means unlimited. Output from both process streams is forwarded to the Log Console.

## Source layout

- `src/main` — Electron window, filesystem dialogs, validation, and native process lifecycle
- `src/preload` — isolated, typed renderer IPC bridge
- `src/renderer/src` — React UI, Radix controls, and siren-themed styles
- `public/Untitled.png` — official FiveInvader brand asset
