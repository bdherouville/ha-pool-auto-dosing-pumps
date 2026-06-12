# 05 — Test Plan

## Per-task gate (Haiku tasks must pass before merge)

- Code compiles: `west build -b nice_nano_v2 firmware` (or fallback board per spec 03 §Module 4).
- Pure-logic modules (dosing clamping, duty mapping) have ztest unit tests under `firmware/tests/` runnable with `west build -b native_sim firmware/tests/<suite> -t run` where feasible.
- Converter: `node --check z2m/piscine_pump4.js` passes.

## Bench tests (hardware, manual)

1. **Safe boot**: flash, power-cycle with scope/LEDs on all 8 pins — all low until commanded.
2. **PWM**: command pump 1 forward 50% — pin A shows 4 kHz / ~50% duty, pin B low. Reverse — swapped, with ≥50 ms both-low gap. Motor speed must visibly differ between duty 80/150/254 (catches L298N switching-loss saturation, see spec 01).
3. **Dose timing**: 60 s dose, stopwatch; error < 1 s. `dose_remaining_s` counts down in RTT log.
4. **Abort**: Off mid-dose stops immediately, remaining → 0.
5. **Min duty**: level 30 (<51) → pump stays stopped.

## Network tests

1. Open z2m permit-join; device joins as Router, interview completes, model `PISCINE-PUMP4`.
2. HA shows the entities listed in spec 04 §Acceptance for all 4 pumps.
3. MQTT: `zigbee2mqtt/<device>/set {"state_pump2":"ON"}` runs pump 2; `OFF` stops it.
4. Dose via MQTT: set `dose_duration_pump3: 10`, `speed_pump3: 200`, trigger `dose_pump3` → 10 s run, `dose_remaining_pump3` counts down, auto-stop reported.
5. Power-cycle while dosing → pump comes up stopped, z2m state resyncs to OFF.
6. Range/router check: device shows as Router in z2m map.

## Regression tests (from bring-up bugs, June 2026)

Each bug found during hardware bring-up gets a permanent regression check. Bugs are
tracked as GitHub issues (drafts in `docs/issues/`); the checks below must pass on
every release build.

| # | Bug (symptom → root cause) | Regression check |
|---|---------------------------|------------------|
| R1 | Direction write rejected (`INVALID_TYPE`) → attr declared `ZB_ZCL_ATTR_TYPE_U8`, spec says enum8 | `grep -A1 PUMP_ATTR_DIRECTION firmware/src/zb_pump.c` shows `ZB_ZCL_ATTR_TYPE_8BIT_ENUM`; network test: forward/reverse from HA latches without z2m error |
| R2 | P0.06 stuck mid-rail → `uart0` default pins collide with pump 1 PWM | merged devicetree has `uart0` `status = "disabled"`; bench test 1 (safe boot, all pins low) |
| R3 | No RTT logs after reboot → `LOG_BACKEND_RTT_MODE_BLOCK` stalls log thread with no host | `prj.conf` has `CONFIG_LOG_BACKEND_RTT_MODE_DROP=y`; boot log readable after cold boot without debugger attached since power-on |
| R4 | GPIO high only 1.8 V after full erase → UICR REGOUT0 factory default in VDDH mode | bench: GPIO high ≥ 3.0 V on first boot after `tools/test-flash.sh` (full erase); main.c programs REGOUT0=3V3 + self-reset once |
| R5 | ON does nothing / speed slider ignored → Move-to-Level CB was a stub and default level 0 | network test: factory-fresh device, plain ON runs pump at full speed; moving speed slider while running changes duty live |
| R6 | Direction write `UNSUPPORTED_ATTRIBUTE` → custom attrs missing `ZB_ZCL_ATTR_MANUF_SPEC` bit; converter attrs missing `write: true` | all 5 custom attrs in `zb_pump.c` carry `ZB_ZCL_ATTR_MANUF_SPEC`; converter declares `write: true` on direction/fillTime attrs; network test: direction + calibration writes succeed |
| R7 | Direction only applied after off/on → spec said "next start" | network test: reverse while running swaps IN1/IN2 within ~100 ms (50 ms dead time) |

