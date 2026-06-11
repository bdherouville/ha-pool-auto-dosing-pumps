# Task list (dispatch order)

| # | Task file | Module | Depends on | Gate |
|---|-----------|--------|------------|------|
| 01 | task-01-scaffold.md | NCS app skeleton (CMakeLists, prj.conf, overlay, empty main) | — | builds |
| 02 | task-02-pump-pwm.md | `pump_pwm.{c,h}` | 01 | builds |
| 03 | task-03-dosing.md | `dosing.{c,h}` + ztest | 02 | builds + tests pass |
| 04 | task-04-zigbee.md | `zb_pump.{c,h}` + `main.c` | 03 | builds |
| 05 | task-05-z2m-converter.md | `z2m/piscine_pump4.js` | — (parallel with 02–04) | `node --check` |
| 07 | task-07-calibration.md | Calibration attrs (fill time 1 L @ min/max speed), firmware + converter | 04, 05 | builds + `node --check` |
| 06 | task-06-docs.md | README + flashing/pairing guide | 04, 05, 07 | review |

Rules: see CLAUDE.md §Delegation rules. One Haiku agent per task; orchestrator reviews each result against specs before dispatching the next. Task 05 may run in parallel with 02–04.
