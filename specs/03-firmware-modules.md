# 03 — Firmware Modules (function signatures are normative)

Language: C, Zephyr style (tabs, `LOG_MODULE_REGISTER`, `int` return = 0/-errno). Each module is one Haiku task. Do not change signatures, pin numbers, or IDs — they come from specs 01/02.

## Module 1: `firmware/src/pump_pwm.{c,h}`

Hardware abstraction over 4 pump channels. No Zigbee, no dosing logic.

```c
#define PUMP_COUNT 4

enum pump_dir { PUMP_DIR_FORWARD = 0, PUMP_DIR_REVERSE = 1 };

/* Init all channels, force all pumps stopped. Call first in main(). */
int pump_pwm_init(void);

/* Run pump idx (0..3) at duty 0..254 in dir. duty < 20% of 254 (i.e. < 51) -> stop.
 * If direction differs from current running direction: stop, k_msleep(50), then start. */
int pump_set(uint8_t idx, enum pump_dir dir, uint8_t duty);

/* Both pins low. Idempotent. Safe from ISR? NO - thread context only. */
int pump_stop(uint8_t idx);

bool pump_is_running(uint8_t idx);
```

Implementation: `pwm_set_dt()` on devicetree-provided channels. Period 50 µs (20 kHz). Devicetree aliases `pump1a/pump1b ... pump4a/pump4b` (defined by Module 4). Pin A PWMs for forward, pin B for reverse, other pin held at 0% duty.

## Module 2: `firmware/src/dosing.{c,h}`

Per-pump dose scheduler. Uses `k_timer` + `k_work` (timer callbacks are ISR context — delegate pump_stop to the system workqueue).

```c
typedef void (*dosing_event_cb_t)(uint8_t idx, uint16_t remaining_s, bool running);

/* cb fires: every 1 s while dosing (remaining countdown), on completion (0,false),
 * and on abort (0,false). Called from system workqueue context. */
int dosing_init(dosing_event_cb_t cb);

/* Start/replace dose on pump idx. duration_s clamped to [1, 3600], duty to [51, 254]. */
int dosing_start(uint8_t idx, uint16_t duration_s, uint8_t duty, enum pump_dir dir);

/* Run indefinitely = dose of 3600 s (safety cap). Used by On/Off On. */
int dosing_run(uint8_t idx, uint8_t duty, enum pump_dir dir);

int dosing_abort(uint8_t idx);
uint16_t dosing_remaining(uint8_t idx);
bool dosing_active(uint8_t idx);
```

State per pump: `{active, remaining_s, duty, dir}` guarded by a single `k_mutex`.

## Module 3: `firmware/src/zb_pump.{c,h}` + `firmware/src/main.c`

ZBOSS application: declares EP1–EP4 per spec 02, wires cluster callbacks to dosing API, reports attributes.

- Use NCS Zigbee templates (`zigbee_default_signal_handler`, `ZB_AF_DECLARE_DEVICE_CTX`, `ZB_ZCL_DECLARE_*`).
- On/Off On → `dosing_run(idx, current_level, direction_attr)`; Off → `dosing_abort(idx)`.
- Level Move-to-Level → store level; if running, re-issue `pump` duty via `dosing_run`.
- Custom cluster 0xFC00 (manuf code 0x1234): handle `start_dose`/`stop_dose` commands in the ZCL device callback / custom cluster handler; update + report `dose_duration_s`, `dose_remaining_s`.
- dosing event callback → `zb_zcl_set_attr_val` for OnOff and dose_remaining_s (schedule on Zigbee thread with `zb_schedule_app_callback`/`zigbee_schedule_callback` — never call ZBOSS APIs from the workqueue directly).
- `main()`: `pump_pwm_init()` → `dosing_init()` → zigbee start.

## Module 4: build files

- `firmware/CMakeLists.txt` (standard NCS app), `firmware/prj.conf`:
  `CONFIG_ZIGBEE=y`, `CONFIG_ZIGBEE_ROLE_ROUTER=y`, `CONFIG_PWM=y`,
  `CONFIG_LOG=y`, `CONFIG_LOG_BACKEND_RTT=y`, `CONFIG_USE_SEGGER_RTT=y`,
  `CONFIG_CRYPTO=y`, `CONFIG_CRYPTO_NRF_ECB=y` (Zigbee stack crypto suite),
  `CONFIG_RAM_POWER_DOWN_LIBRARY=n`.
- `firmware/boards/nice_nano_v2.overlay`: PWM0 ch0–3 → P0.06/P0.17/P0.22/P1.00 (pins A), PWM1 ch0–3 → P0.08/P0.20/P0.24/P0.11 (pins B), `pwm-leds`-style nodes or custom `pwms` props with aliases `pump1a..pump4b`. Pin map from spec 01 — copy exactly.
- If the board name `nice_nano_v2` doesn't exist in the Zephyr/NCS revision used, fall back to `-b nrf52840dongle/nrf52840` plus the overlay, and note it in the task report.

## Module 5: `z2m/piscine_pump4.js`

See spec 04.
