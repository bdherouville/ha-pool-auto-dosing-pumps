# Task 03 — dosing scheduler

Depends: task 02. Touch ONLY: `firmware/src/dosing.c`, `firmware/src/dosing.h`, CMakeLists addition, and `firmware/tests/dosing/` (ztest suite).

## Header (copy verbatim — normative)

```c
typedef void (*dosing_event_cb_t)(uint8_t idx, uint16_t remaining_s, bool running);
int dosing_init(dosing_event_cb_t cb);
int dosing_start(uint8_t idx, uint16_t duration_s, uint8_t duty, enum pump_dir dir);
int dosing_run(uint8_t idx, uint8_t duty, enum pump_dir dir);   /* = dosing_start(idx, 3600, ...) */
int dosing_abort(uint8_t idx);
uint16_t dosing_remaining(uint8_t idx);
bool dosing_active(uint8_t idx);
```

`enum pump_dir` comes from `pump_pwm.h`.

## Behavior contract

- Per pump: one `k_timer` with 1 s period + one `k_work`. Timer expiry (ISR context) submits the work item; ALL pump calls and callback invocations happen in the work handler (system workqueue), never in the timer ISR.
- `dosing_start`: clamp duration_s to [1, 3600], duty to [51, 254]; call `pump_set`; on its error, propagate and stay inactive. Replaces a running dose (no stop/start glitch needed beyond what pump_set does).
- Each 1 s tick: decrement remaining, invoke cb(idx, remaining, true). At 0: `pump_stop`, cb(idx, 0, false), timer stopped.
- `dosing_abort`: stop timer, `pump_stop`, cb(idx, 0, false) only if it was active; idempotent.
- State guarded by one `k_mutex` (lock in work handler and API calls; cb invoked OUTSIDE the lock).
- `LOG_MODULE_REGISTER(dosing, ...)`.

## Tests (`firmware/tests/dosing/`)

ztest suite for pure logic (clamping, state transitions). Abstract pump calls behind weak symbols or a `CONFIG_ZTEST` mock of pump_pwm so it builds for `native_sim`. Cover: clamp 0→1 s and 5000→3600 s, duty 30→stopped, abort idempotency, replace-while-active.

## Done-criteria

- `west build -b nrf52840dongle/nrf52840 firmware` succeeds.
- `west build -b native_sim firmware/tests/dosing -t run` passes (if native_sim is unavailable in the environment, report that and provide the test code anyway).
- Report which criteria ran and their output.

Ambiguity → STOP and report.
