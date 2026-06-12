# 01 — Hardware

## Board

nice!nano v2-compatible nRF52840 dev board. USB-C for power (5V) and CDC console (optional); primary debug via SWD + RTT.

## Pin map (SOURCE OF TRUTH — do not change without updating this file)

| Pump | Pin A (IN1) | Pin B (IN2) | nice!nano label |
|------|-------------|-------------|-----------------|
| 1    | P0.06       | P0.08       | D1 (006) / D0 (008) |
| 2    | P0.17       | P0.20       | D10 (017) / D9 (020) |
| 3    | P0.22       | P0.24       | D8 (022) / D7 (024) |
| 4    | P1.00       | P0.11       | D6 (100) / D5 (011) |

All pins are PWM-capable (nRF52840 PWM peripherals route to any GPIO). One nRF PWM peripheral instance has 4 channels; use **PWM0 channels 0–3 for pins A** and **PWM1 channels 0–3 for pins B**.

## Board-variant note (ProMicro/SuperMini nRF52840 clone)

- The external 3V3/VCC output pin is gated by a MOSFET on **P0.13** (low = off). Firmware drives P0.13 high at boot so the VCC pin (and J-Link VTref) is powered. (Vendor pinout reference: <https://www.beachyuk.com/blog/connecting-and-testing-promicro-nrf52840-clones>.)
- Left header, top to bottom (USB at top) — note the **second GND in position 4**, an easy off-by-one when probing:

  ```
  GND, P0.06, P0.17, GND, P0.20, P0.22, P0.24, P1.00, P0.11, ...
  ```
- Silkscreen D-labels on clones may not match nice!nano; wire by nRF port/pin (table above), not by D-label.
- USB power goes through VDDH; an erased UICR leaves REGOUT0 at 1.8 V (GPIO highs too low for L298N). Firmware programs REGOUT0 to 3.3 V on first boot and resets. A full-chip erase (`tools/test-flash.sh` without `--no-erase`) wipes UICR, so expect one extra self-reset on the first boot after.
- **No 32.768 kHz crystal** on these clones. The LFCLK must be the internal RC with calibration (`CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC=y` in prj.conf). Building with the DK default (XTAL) appears to work, then MPSL asserts (107) minutes after boot and the radio dies until reset.

## Drive scheme (L298N-mini, IN1/IN2 driven directly)

| State   | Pin A        | Pin B        |
|---------|--------------|--------------|
| Forward | PWM (duty=speed) | low      |
| Reverse | low          | PWM (duty=speed) |
| Stop    | low          | low (coast)  |

- PWM frequency: **4 kHz** (period 250 µs). The L298N's darlington outputs switch in ~2–4 µs; above ~10 kHz the switching transients dominate the period and the motor sees near-full voltage at any duty (observed: speed control ineffective at 20 kHz). 4 kHz keeps duty linear with acceptable switching losses; audible whine is acceptable for this application.
- Duty: 0–100% mapped from Zigbee level 0–254. Enforce a minimum running duty of 20% (below that, stop) so motors don't stall-hum.
- Never PWM both pins at once. Transition through Stop when reversing direction (≥50 ms dead time).

## Electrical

- L298N-mini VCC = pump supply (e.g. 12 V); its 5V logic pin NOT connected to the nano. **Common GND** between L298N boards and the nRF board is mandatory.
- nRF GPIO is 3.3 V; L298N-mini inputs accept 3.3 V logic — OK.
- Power pumps from a separate supply, not USB.

## Flashing

- SWD: SWDIO/SWCLK pads on the board underside + GND + 3V3 sense.
- `west flash --erase` first time (removes Adafruit UF2 bootloader → full flash for app).
- Recovery: `nrfjprog --recover` (or `pyocd erase --mass`) if APPROTECT trips.
