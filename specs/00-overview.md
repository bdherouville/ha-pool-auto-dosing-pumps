# 00 — System Overview

## Goal

Firmware for a 4-channel peristaltic-pump dosing controller (pool chemicals) that:

- Runs on an nRF52840 board (nice!nano v2-compatible pinout).
- Drives up to 4 DC pump motors through L298N-mini H-bridge boards (2 GPIO per motor).
- Joins a Zigbee network as a **Zigbee 3.0 router** and is managed by **zigbee2mqtt**.
- Appears in **Home Assistant** (via z2m) with per-pump entities: run/stop switch, speed, direction, and timed dosing.

End state: a fully controllable pool dosing system — every pump independently
runnable, reversible and dosable from HA, with per-liter calibration for
dose-by-volume automation.

## Architecture

```
Home Assistant ⇄ MQTT ⇄ zigbee2mqtt (+ external converter z2m/piscine_pump4.js)
                              ⇅ Zigbee
                    nRF52840 firmware (NCS/Zephyr + ZBOSS)
                       app/zigbee  →  app/dosing  →  drivers/pump_pwm
                                                        ⇅ 8 GPIO (PWM)
                                              4 × L298N-mini → 4 pumps
```

Layering rule (strict):

- `drivers/pump_pwm` — hardware only, no Zigbee, no timers beyond PWM.
- `app/dosing` — timing/state machine, calls pump_pwm only.
- `app/zigbee` — ZBOSS endpoints/attributes, calls dosing only.

## Repo layout

```
piscine/
├── CLAUDE.md            # conventions + Haiku delegation rules
├── specs/               # these documents (source of truth — code must conform)
├── tasks/               # self-contained briefs for Haiku coding agents
├── firmware/            # NCS application
│   ├── CMakeLists.txt
│   ├── prj.conf
│   ├── boards/nice_nano_v2.overlay
│   └── src/
│       ├── main.c
│       ├── pump_pwm.c / pump_pwm.h
│       ├── dosing.c / dosing.h
│       └── zb_pump.c / zb_pump.h
└── z2m/piscine_pump4.js # zigbee2mqtt external converter
```

## Toolchain

- nRF Connect SDK (NCS) v2.x, `west build -b nice_nano_v2` (custom board overlay; the upstream Zephyr board name may be `nice_nano` — task brief confirms).
- Flash/debug via SWD probe (J-Link or CMSIS-DAP): `west flash`. The stock UF2 bootloader is erased.
- Logging: SEGGER RTT.

## Glossary

- **Dose**: run pump N at speed S in direction D for T seconds, then auto-stop.
- **EP**: Zigbee endpoint. EP1–EP4 map to pumps 1–4.
- **z2m**: zigbee2mqtt.

## Safety invariants (non-negotiable)

1. All pumps stopped (both GPIOs low) at boot, before any other init.
2. Any error/reset path leaves pumps stopped.
3. A dose always has a finite duration; max 3600 s, enforced in firmware.
4. On/Off "off" or a new dose command aborts the running dose on that pump.
