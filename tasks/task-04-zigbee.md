# Task 04 — Zigbee application layer

Depends: task 03. Touch ONLY: `firmware/src/zb_pump.c`, `firmware/src/zb_pump.h`, `firmware/src/main.c`, CMakeLists/prj.conf additions if strictly required.

Read `specs/02-zigbee-model.md` — every ID below comes from it; copy verbatim.

## Device identity

- Zigbee 3.0 **router**. Basic cluster on EP1: manufacturer `"DIY"`, model `"PISCINE-PUMP4"`, power_source mains (0x01).
- Endpoints **1–4** = pumps index **0–3**. Profile HA 0x0104, device id 0x0101.

## Clusters per endpoint (server)

- Identify 0x0003, On/Off 0x0006, Level Control 0x0008 (CurrentLevel 0–254).
- Custom cluster **0xFC00**, manufacturer code **0x1234**:
  - attrs: 0x0000 `direction` enum8 RW, 0x0001 `dose_duration_s` uint16 RO, 0x0002 `dose_remaining_s` uint16 RO reportable.
  - cmds: 0x00 `start_dose` (uint16 duration_s, uint8 level, uint8 direction), 0x01 `stop_dose`.

## Wiring (uses ONLY the dosing API from task 03)

- On/Off On → `dosing_run(idx, current_level, direction_attr)`; Off → `dosing_abort(idx)`.
- Move-to-Level → store CurrentLevel; if `dosing_active(idx)`, re-issue `dosing_run` with new level.
- `start_dose` → set direction + dose_duration_s attrs, `dosing_start(...)`. `stop_dose` → `dosing_abort`.
- dosing callback (workqueue context!): marshal to Zigbee context via `zigbee_schedule_callback`/ZBOSS scheduler, then `zb_zcl_set_attr_val` for OnOff (bool running) and dose_remaining_s. NEVER call ZBOSS APIs directly from the workqueue.
- `main()`: `pump_pwm_init()` → `dosing_init(cb)` → register device ctx, `zigbee_enable()`. Network steering on boot if not joined (use NCS `zigbee_default_signal_handler`).
- Boot state: OnOff=false on all endpoints, no persistence of running state.

Base the structure on the NCS Zigbee light_bulb sample patterns (declaration macros, ZB_AF_DECLARE_DEVICE_CTX_EP_VA for multiple endpoints).

## Done-criteria

- `west build -b nrf52840dongle/nrf52840 firmware` succeeds.
- Report: endpoint/cluster declaration summary, how custom-cluster commands are parsed, and how cross-thread marshalling is done.

Ambiguity (e.g. macro availability differs by NCS version) → STOP and report with the exact compile error; do not redesign the data model.
