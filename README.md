# piscine — 4-Channel Zigbee Peristaltic Pump Controller

A firmware-based dosing controller for 4 peristaltic pumps (pool chemicals) running on an nRF52840 board. The device joins a Zigbee network as a router, is managed via zigbee2mqtt, and appears in Home Assistant with per-pump control entities. Each pump can be run forward or reverse, at variable speed, and automatically stopped after a timed dose.

## Goal

A **fully controllable pool dosing system**: 4 peristaltic pumps, each with on/off,
variable speed, live forward/reverse, bounded timed doses (≤ 1 h), and per-liter
calibration — all managed from Home Assistant via zigbee2mqtt, with safety
invariants (pumps stopped on boot and on every error path) enforced in firmware.

### Status / roadmap

- [x] Firmware: PWM drive, dosing engine, Zigbee 3.0 router, 4 endpoints
- [x] z2m external converter + HA entities for all 4 pumps
- [x] Pump 1 verified on hardware (on/off, speed, live direction change)
- [x] SuperMini/ProMicro nRF52840 board quirks handled (REGOUT0 3.3 V rail, switched VCC, UART/PWM pin conflict)
- [ ] Pumps 2–4 bench verification (wiring + meter check per `specs/05-test-plan.md`)
- [ ] Calibration workflow (fill-time per liter → dose-by-volume)
- [ ] HA dashboard + automation examples (pH/chlorine schedule)

## Hardware & Wiring

**Board:** nice!nano v2 (nRF52840 compatible)  
**Motor drivers:** 4× L298N-mini H-bridges (one per pump)  
**Control:** 8 GPIO pins (2 per motor) for PWM direction signals

### Pin Map

| Pump | Pin A (IN1) | Pin B (IN2) | nice!nano label |
|------|-------------|-------------|-----------------|
| 1    | P0.06       | P0.08       | D1 (006) / D0 (008) |
| 2    | P0.17       | P0.20       | D10 (017) / D9 (020) |
| 3    | P0.22       | P0.24       | D8 (022) / D7 (024) |
| 4    | P1.00       | P0.11       | D6 (100) / D5 (011) |

**Critical:** All L298N-mini boards must share **common GND** with the nRF52840 board. Power the pumps from a separate 12 V supply (not USB); the L298N logic inputs accept 3.3 V from the nRF GPIO.

## Build & Flash

**Prerequisite:** nRF Connect SDK (NCS) v2.x workspace set up with `west`.

Build the firmware:

```bash
west build -b nrf52840dongle/nrf52840 firmware
```

Flash for the first time (erases the UF2 bootloader):

```bash
west flash --erase
```

Recovery (if APPROTECT is set):

```bash
nrfjprog --recover
```

**Logging:** SEGGER RTT via SWD debugger. Connect via your IDE's RTT viewer or:

```bash
JLinkExe -device NRF52840 -if SWD -RTTChannel 0
```

## Zigbee2mqtt Setup

The external converter file `z2m/piscine_pump4.js` must be placed in your zigbee2mqtt installation. Add its path to your z2m configuration:

```yaml
external_converters:
  - piscine_pump4.js
```

**Pairing:** Set z2m to permit joining (default 255 s), then power the device. It will interview as a Zigbee 3.0 router with 4 endpoints.

## Home Assistant Integration

Once paired and configured via zigbee2mqtt, each pump appears in Home Assistant with the following entities:

- **state** — On/Off switch (enables/disables pump; Off aborts any running dose)
- **speed** — Numeric slider (0–254, motor duty cycle; <20% is treated as stop to prevent stalling)
- **direction** — Enum selector (forward / reverse)
- **dose_duration** — Numeric input (1–3600 seconds; settable)
- **dose_remaining** — Read-only sensor (seconds remaining in current dose)
- **dose** — Action button (triggers timed dose using the current speed, direction, and duration)
- **fill_time_1l_min** — Numeric calibration value (seconds to pump 1 L at minimum speed, settable)
- **fill_time_1l_max** — Numeric calibration value (seconds to pump 1 L at maximum speed, settable)

Entities appear as `piscine_pump4_pump1_*`, `piscine_pump4_pump2_*`, etc.

## Safety

- **Boot invariant:** All pumps are stopped (both GPIOs low) before any other initialization.
- **Dose cap:** Maximum dose duration is enforced at 3600 seconds in firmware. Any dose command exceeding this is rejected.
- **Error handling:** Any error or reset path guarantees pumps are stopped.

For detailed hardware specifications, pin assignments, Zigbee cluster definitions, and firmware architecture, see `specs/`.
