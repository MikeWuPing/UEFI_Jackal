# Jackal UEFI

English | [中文](README.md)

A complete recreation of the NES game **Jackal** that runs natively in a UEFI Shell environment. Rather than emulating NES hardware, this project translates the original game's 6502 disassembly line-by-line into a platform-independent C core, then drives graphics with the UEFI GOP protocol and input/timing with UEFI services. It runs in QEMU (or a real UEFI environment). No audio: sound call sites keep their equivalent state/timing behavior, but nothing depends on the APU or audio data.

<img width="512" height="480" alt="intro" src="https://github.com/user-attachments/assets/e6d96822-c5be-4d65-8680-71fd7ab53187" />
<img width="512" height="480" alt="gameplay_turret" src="https://github.com/user-attachments/assets/f73d49c7-0c24-4335-b69d-63f10882d197" />
<img width="512" height="480" alt="boss_fight" src="https://github.com/user-attachments/assets/aa3ec6a8-6ab4-401e-baab-bb5f27181754" />
<img width="512" height="480" alt="ending" src="https://github.com/user-attachments/assets/68c8d8d4-2231-46f3-9a53-98b299656691" />


This repository contains **all original source code and tooling** (the UEFI app, the ROM asset extractor, and the test/verification toolchain). It does **not** contain the original game ROM or any data extracted from it (see the copyright section below).

## Fidelity Principles

Game logic is **restored from the disassembly, never rewritten from memory**:

- The per-frame main loop mirrors the original NMI-driven model (once per frame, not a free-running loop);
- The layered state machines (`GameControlState` / `GamePlayMode`) keep the original state IDs and transition conditions;
- The 32-slot logical sprite system, OAM generation order, and slot rotation strategy (`SpriteSlotRotation`) are preserved as-is;
- Fixed-point (high/low byte) coordinate and speed behavior, hitboxes and HP/death states, level screen advancement and boss counters are translated line by line;
- Names stay close to the disassembly labels (`Label1096`, `subProcessObjectLogic`, `tblSpriteLogic`…) so every line can be traced back;
- Any deliberate deviation is documented in Chinese comments next to the code.

Fidelity priority: frame timing & state transitions > sprite spawn/despawn > fixed-point coordinates > collision & HP > level progression > palette/pattern updates > render scaling.

## Repository Layout

```
JackalPkg/Application/JackalUefi/   UEFI application and game core
  core/           Platform-independent NES core: RAM mirror, state machines,
                  sprite system, enemy AI, collision, scroll loading, weapons,
                  POWs, Continue/ending machines
  platform/       UEFI platform layer: GOP framebuffer, keyboard, frame pacing
  JackalUefi.c    Entry point and main loop (debug defines below)
JackalPkg/Generated/                ROM extraction output (generated, not committed)
tools/
  rom_extract/    ROM asset extractor: iNES parsing, bank slicing, pattern/
                  metasprite/layout/palette decoding → C arrays + bin + manifest
                  + sample PNGs
  core_test/      Host-side unit tests (cl-compiled core + Generated + tests)
  qemu_capture.py QEMU runner + monitor screendump + key injection
  verify_phase*.py Serial-log end-to-end assertions (title/intro/full playthrough/
                  continue/ending)
  Build-Jackal.ps1 / Run-JackalQemu.ps1 / JackalConfig.ps1  build & run scripts
```

The fully-commented reference disassembly (RayofJay, MIT No Attribution) is not
bundled with this repository; it is publicly available as
"NES Jackal Disassembly Fully Commented".

## Build & Run

### Prerequisites

- Windows + Visual Studio 2019 (C toolchain)
- An EDK2 source tree (the scripts default to `D:\Work\Code\edk2`; adjust
  `WORKSPACE`/`EDK2_DIR` in `tools/Build-Jackal.ps1` for your layout)
- QEMU (default `C:\Program Files\qemu`) and an OVMF firmware image
  (configured in `tools/JackalConfig.ps1`)
- Python 3 (extractor, test and verification tooling)

### Steps

1. Place a **legally obtained** `orgrom.nes` into `ref/` (not distributed — see below).
2. Extract assets: `python tools/rom_extract/run_extract.py`
   (produces `assets/` and `JackalPkg/Generated/`).
3. Build: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build-Jackal.ps1`
   (auto-increments the version; output staged to `run/hda/`).
4. Verify: `python tools/qemu_capture.py --seconds 40`
   (snapshots and serial logs land in `snapshot/` and `run_logs/`),
   or boot `run/hda` in QEMU directly to play (SDL window).

### Controls

| NES pad | Keyboard |
|---|---|
| DPad | Arrow keys |
| A (main weapon) | X |
| B (machine gun) | Z |
| Start | Enter |
| Select | Backspace |
| Quit app | Esc (or Select+Start) |

## Testing

- Host unit tests: `python tools/core_test/run_tests.py`
  (~160 cases: state machines, AI, collision, scrolling, weapons, buildings, ending)
- Extractor regression: `cd tools/rom_extract && python -m unittest discover -s tests`
- End-to-end assertions (require a verification build, see below):
  - `python tools/verify_phase3.py --mode autostart` (Chinook intro + first spawns)
  - `python tools/verify_phase4.py --mode autostart` (full Level 1 playthrough)
  - `python tools/verify_phase6.py --mode autostart` / `--mode continue` (segue/continue)
  - `python tools/verify_phase7.py --mode force-ending` (ending machine loop)

## Debug Defines (`-VerifyDefine`, non-original behavior, for verification/playtesting)

- `JACKAL_DEBUG_AUTO_START`: presses Start automatically
- `JACKAL_DEBUG_AUTOSCROLL`: simulated player that pushes the scroll (with a boss-fight assist driver)
- `JACKAL_DEBUG_FORCE_ENDING`: drives the ending machine directly
- `JACKAL_DEBUG_MAX_WEAPON`: max main weapon + 99 lives permanently (playtest cheat)

Example: `powershell -File tools/Build-Jackal.ps1 -VerifyDefine JACKAL_DEBUG_AUTO_START,JACKAL_DEBUG_AUTOSCROLL`

## Technical Highlights

- **Core/platform split**: `core/` mirrors the original RAM layout (sprite arrays `$0500-$074F`, collision map `$0300/$0400`, OAM shadow, the `$0770` PPU update queue) and can be compiled with cl on a PC to run every logic test; `platform/` only handles presentation, input, and timing.
- **Extraction, not embedding**: with CHR ROM size 0 (UxROM), pattern data lives in PRG banks and is decompressed into CHR RAM at runtime; the extractor tracks `subLoadNewPatternTable` and the graphics/layout tables, converting 2bpp tiles, palettes, metasprites and screen layouts into C arrays. Everything is reproducible, and each table's disassembly name and bank offset is documented in comments.
- **Collision system**: 2-bit × 4 per byte, 8px granularity, six-threshold classification — byte-identical to the original algorithm, with NearLookAhead/FarLookAhead translated line by line.
- **Tests as evidence**: every key restored mechanism (scroll coordination, parent-child spawning, splash oscillation…) has a host-side anchor test; the full playthrough chain is covered by QEMU serial assertions.

## Copyright & License

- **Original code and tooling in this repository** (`JackalPkg/`, `tools/`) is released under the MIT License (see `LICENSE` in the repository root).
- The fully-commented reference disassembly is the work of RayofJay (MIT No Attribution) and is not bundled with this repository.
- **The Jackal game itself (ROM, graphics, level data) is Konami's intellectual property.**
  This repository neither contains nor redistributes the original ROM or assets extracted from it:
  `ref/orgrom.nes`, `assets/`, and `JackalPkg/Generated/` are all listed in `.gitignore`.
  Please obtain the ROM legally on your own and generate the assets with the included toolchain.
  This is a non-commercial, educational recreation, unaffiliated with and unendorsed by Konami.

## Acknowledgements

- RayofJay for the fully-commented disassembly (MIT No Attribution) — the foundation of this line-by-line restoration.
- The EDK2 and QEMU open-source communities.
