# Task 07 — Calibration attributes (time to fill 1 L at min/max speed)

Depends: tasks 04 and 05 complete. Touch ONLY: `firmware/src/zb_pump.c`, `firmware/src/zb_pump.h`, `firmware/prj.conf`, `z2m/piscine_pump4.js`.

Purpose: per pump, two writable calibration values so Home Assistant can convert dose time ↔ volume: seconds to pump 1 litre at minimum running speed (duty 51) and at maximum speed (duty 254). HA interpolates flow for intermediate speeds; the firmware only stores/reports the values.

## Firmware (IDs verbatim from specs/02-zigbee-model.md)

Add to custom cluster **0xFC00** (manufacturer code **0x1234**), on each endpoint 1–4:

| Attr | ID | Type | Access |
|------|----|------|--------|
| `fill_time_1l_min_s` | 0x0003 | uint16 | RW, reportable, default 0 (= uncalibrated) |
| `fill_time_1l_max_s` | 0x0004 | uint16 | RW, reportable, default 0 |

- Extend the existing 0xFC00 attribute list declarations in zb_pump.c (pattern already in place from task 04).
- **Persistence**: values must survive reboot. Use the Zephyr settings subsystem (`CONFIG_SETTINGS=y`, `CONFIG_SETTINGS_NVS=y`, `CONFIG_NVS=y`, `CONFIG_FLASH=y`, `CONFIG_FLASH_MAP=y` in prj.conf — add only those not already set). Keys: `pump/cal/<ep>/min`, `pump/cal/<ep>/max`. On ZCL write of either attribute: store via `settings_save_one`. On boot, after endpoint registration: `settings_load()` then write loaded values into the ZCL attributes.
- No behavior change to dosing — these are storage-only attributes.

## Converter (z2m/piscine_pump4.js)

The custom cluster `piscinePump` is already registered with `customClusters`. Add:

- Cluster attribute entries: `fillTime1lMinS` (0x0003, UINT16), `fillTime1lMaxS` (0x0004, UINT16).
- fromZigbee: map reports/read-responses to `fill_time_1l_min_<pumpN>` / `fill_time_1l_max_<pumpN>`.
- toZigbee: keys `fill_time_1l_min`, `fill_time_1l_max` → `entity.write('piscinePump', {...}, {manufacturerCode: 0x1234})`, plus convertGet reading the attribute.
- Exposes per pump (inside the existing `pumpExposes()` helper): numeric `fill_time_1l_min` and `fill_time_1l_max`, access ALL, range 0–65535, unit `s`, with descriptions mentioning calibration at min/max speed.

## Done-criteria

- `west build -b nrf52840dongle/nrf52840 firmware` succeeds (cwd ~/ncs, build-dir firmware/build).
- `node --check z2m/piscine_pump4.js` passes.
- Report: how persistence is wired (settings keys, load path), and the exposes added.

Ambiguity → STOP and report; do not change IDs or key names.
