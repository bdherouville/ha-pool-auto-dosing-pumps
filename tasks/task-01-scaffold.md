# Task 01 — NCS application scaffold

Create the buildable skeleton of the firmware app. Touch ONLY the files listed.

## Files to create

- `firmware/CMakeLists.txt`
- `firmware/prj.conf`
- `firmware/boards/nice_nano_v2.overlay`
- `firmware/src/main.c` (minimal: log "piscine boot", `return 0`)

## CMakeLists.txt

Standard NCS app: `cmake_minimum_required(VERSION 3.20.0)`, `find_package(Zephyr ...)`, `project(piscine_pump4)`, `target_sources(app PRIVATE src/main.c)`.

## prj.conf (copy verbatim)

```
CONFIG_ZIGBEE=y
CONFIG_ZIGBEE_ROLE_ROUTER=y
CONFIG_PWM=y
CONFIG_LOG=y
CONFIG_LOG_BACKEND_RTT=y
CONFIG_USE_SEGGER_RTT=y
CONFIG_CRYPTO=y
CONFIG_CRYPTO_NRF_ECB=y
CONFIG_RAM_POWER_DOWN_LIBRARY=n
CONFIG_HEAP_MEM_POOL_SIZE=2048
```

## Overlay — pin map (copy exactly, source: specs/01-hardware.md)

PWM0 channels 0–3 = pins A: **P0.06, P0.17, P0.22, P1.00**
PWM1 channels 0–3 = pins B: **P0.08, P0.20, P0.24, P0.11**

Define `&pwm0`/`&pwm1` pinctrl for those pins, and a `pwmleds`-style custom node giving devicetree **aliases `pump1a, pump1b, pump2a, pump2b, pump3a, pump3b, pump4a, pump4b`**, each a `pwms = <&pwmX ch PWM_USEC(50) PWM_POLARITY_NORMAL>;` entry (20 kHz period = 50 µs).

## Done-criteria

- `west build -b nice_nano_v2 firmware` succeeds. If that board name is unknown in this NCS revision, retry with `-b nrf52840dongle/nrf52840` and report which one was used.
- Report build output summary (flash/RAM usage lines).

If anything is ambiguous (e.g. board definition missing), STOP and report — do not guess.
