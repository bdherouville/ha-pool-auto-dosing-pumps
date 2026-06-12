# 04 — zigbee2mqtt External Converter

File: `z2m/piscine_pump4.js`. Installed via z2m `external_converters` config. Modern z2m (≥1.35) module style: `module.exports = definition` using `zigbee-herdsman-converters` `exposes`/`fz`/`tz` helpers (import from `zigbee-herdsman-converters/lib/*` or the `lib/modernExtend` API — plain fz/tz style is fine and preferred for custom clusters).

## Matching

```js
zigbeeModel: ['PISCINE-PUMP4'],
model: 'PISCINE-PUMP4',
vendor: 'DIY',
description: '4-channel peristaltic dosing pump controller',
```

## Custom cluster

Register cluster `piscinePump` = ID 0xFC00, manufacturerCode 0x1234, attributes
`direction` (0x0000 enum8), `doseDurationS` (0x0001 uint16), `doseRemainingS` (0x0002 uint16),
commands `startDose` (0x00: duration_s uint16, level uint8, direction uint8), `stopDose` (0x01).
Use `definition.extend`-less custom `fromZigbee`/`toZigbee` with `meta: {multiEndpoint: true}` and
`endpoint: (device) => ({pump1: 1, pump2: 2, pump3: 3, pump4: 4})`.

## Exposes (per endpoint pump1..pump4)

| Expose | Type | Maps to |
|--------|------|---------|
| `switch` (state) | binary | On/Off cluster |
| `speed` | numeric 0–100 % (converter scales to ZCL level 0–254; below 20 % the firmware stops the pump) | Level Control CurrentLevel |
| `direction` | enum [forward, reverse] | 0xFC00 attr 0x0000 |
| `dose_duration` | numeric 1–3600 s (settable) | payload for startDose |
| `dose_remaining` | numeric, read-only | 0xFC00 attr 0x0002 |
| `fill_time_1l_min` | numeric 0–65535 s, settable | 0xFC00 attr 0x0003 (calibration: time to pump 1 L at min speed) |
| `fill_time_1l_max` | numeric 0–65535 s, settable | 0xFC00 attr 0x0004 (calibration: time to pump 1 L at max speed) |
| `start_dose` | composite/action: setting `dose_duration` then `state=ON`? **No** — expose an explicit `enum` button-like `dose` set-only expose that sends `startDose(dose_duration, speed, direction)` | command 0x00 |

Behavior contract:
- Setting `state_pumpN: OFF` sends On/Off Off (aborts dose).
- Setting `dose_pumpN: START` sends `startDose` with the last-set `dose_duration`, `speed`, `direction` values (converter keeps them in `meta`/publishes optimistically).
- Attribute reports for OnOff and doseRemainingS update HA live (z2m → MQTT → HA discovery handles entity creation automatically).

## Reporting setup

`configure:` bind EP1–4 for `genOnOff` and `piscinePump`, set up reporting:
OnOff on-change; doseRemainingS min 1 s / max 60 s / change 1.

## Acceptance

- z2m starts with the converter without errors; device interview shows 4 endpoints.
- HA shows, per pump: switch, speed number, direction select, dose duration number, dose remaining sensor, dose start button.
