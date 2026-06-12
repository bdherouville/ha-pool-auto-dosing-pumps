# Task 06 — README and setup guide

Depends: tasks 04 + 05 complete. Touch ONLY: `README.md`.

Write `README.md` at repo root covering, briefly and accurately (verify claims against the actual files in the repo — do not invent):

1. What the project is (one paragraph; see specs/00-overview.md).
2. Wiring: pin map table copied from specs/01-hardware.md, common-GND warning, separate pump supply.
3. Build & flash: NCS setup pointer, `west build -b <board actually used in task 01>`, `west flash --erase` first-flash note (UF2 bootloader removal), RTT logging command.
4. zigbee2mqtt setup: where to put `z2m/piscine_pump4.js`, `external_converters` config snippet, pairing (permit join).
5. Home Assistant: list of entities that appear per pump (from specs/04 acceptance list).
6. Safety notes: boot-stopped invariant, 3600 s dose cap.

## Done-criteria

- README references only files/commands that exist in the repo.
- Report: any mismatches found between specs and implemented code while writing (list them; do not fix them).
