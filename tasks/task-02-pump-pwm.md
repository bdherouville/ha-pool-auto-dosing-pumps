# Task 02 — pump_pwm driver

Depends: task 01 (overlay aliases exist). Touch ONLY: `firmware/src/pump_pwm.c`, `firmware/src/pump_pwm.h`, add the .c to `firmware/CMakeLists.txt`.

## Header (copy signatures verbatim — normative, from specs/03-firmware-modules.md)

```c
#define PUMP_COUNT 4
enum pump_dir { PUMP_DIR_FORWARD = 0, PUMP_DIR_REVERSE = 1 };
int pump_pwm_init(void);
int pump_set(uint8_t idx, enum pump_dir dir, uint8_t duty);
int pump_stop(uint8_t idx);
bool pump_is_running(uint8_t idx);
```

## Behavior contract

- Channels from devicetree aliases `pump1a/pump1b .. pump4a/pump4b` via `PWM_DT_SPEC_GET(DT_ALIAS(...))`.
- `pump_pwm_init`: verify `pwm_is_ready_dt` for all 8, set every channel to 0% duty (all pumps stopped). Return -ENODEV if any not ready.
- `pump_set(idx, dir, duty)`:
  - idx ≥ 4 → -EINVAL.
  - duty < 51 → behave as `pump_stop`.
  - Forward: pin A duty = `duty/254` of period, pin B = 0. Reverse: swapped.
  - If currently running in the **opposite** direction: set both to 0, `k_msleep(50)`, then apply. Thread context only.
- `pump_stop`: both pins 0% duty, idempotent.
- Track per-pump state `{running, dir}` in a static array.
- `LOG_MODULE_REGISTER(pump_pwm, LOG_LEVEL_INF)`; log start/stop with idx, dir, duty.
- Zephyr style: tabs, return 0/-errno. No Zigbee or dosing includes.

## Done-criteria

- `west build -b nrf52840dongle/nrf52840 firmware` succeeds.
- Report: list of public functions implemented and the duty→pulse computation used.

Ambiguity → STOP and report; do not change pins, signatures, or thresholds.
