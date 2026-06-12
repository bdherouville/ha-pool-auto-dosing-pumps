# 02 — Zigbee Data Model (SOURCE OF TRUTH for IDs)

## Device

- Zigbee 3.0 **router** (mains powered), ZBOSS stack from nRF Connect SDK.
- Basic cluster: `manufacturer_name = "DIY"`, `model_id = "PISCINE-PUMP4"` (exact strings; the z2m converter keys on them), `power_source = 0x01` (mains).
- Install-code-less joining (well-known key), steering on boot if not joined. Factory reset: hold the user button (if fitted, P0.31) 5 s, or `west flash --erase`.

## Endpoints

EP1..EP4 = pumps 1..4. Identical cluster set on each endpoint, profile **HA (0x0104)**, device id **Dimmable Light-like custom**: use 0x0101 to keep z2m happy; the converter overrides exposes anyway.

### Server clusters per endpoint

| Cluster | ID | Use |
|---------|----|-----|
| Basic | 0x0000 | EP1 only |
| Identify | 0x0003 | standard |
| On/Off | 0x0006 | run/stop. `Off` also aborts a dose. `On` runs at current level/direction indefinitely (capped at 3600 s by safety invariant). |
| Level Control | 0x0008 | `CurrentLevel` 0–254 = pump speed. Changing level while running re-applies duty (remaining dose time preserved). `Options` (0x000F) is declared with **ExecuteIfOff = 1** so MoveToLevel is accepted while the pump is off (ZCL8 3.10.2.2.8.1 would otherwise drop it); a level change while off never starts the pump. |
| **Pump custom** | **0xFC00** (manuf. code **0x1234**) | see below |

### Custom cluster 0xFC00 (manufacturer code 0x1234)

Attributes (all reportable):

| Attr | ID | Type | Meaning |
|------|----|------|---------|
| `direction` | 0x0000 | enum8 | 0 = forward, 1 = reverse. Writable. Applied immediately: a running pump restarts in the new direction (pump_pwm enforces stop + ≥50 ms dead time). |
| `dose_duration_s` | 0x0001 | uint16 | Last requested dose duration (RO). |
| `dose_remaining_s` | 0x0002 | uint16 | Seconds left in current dose, 0 when idle. Reported every 1 s while dosing and on completion. |
| `fill_time_1l_min_s` | 0x0003 | uint16 | Calibration: seconds to pump 1 L at **minimum running speed** (duty 51). Writable, persisted across reboots. 0 = uncalibrated. |
| `fill_time_1l_max_s` | 0x0004 | uint16 | Calibration: seconds to pump 1 L at **maximum speed** (duty 254). Writable, persisted across reboots. 0 = uncalibrated. |

Commands (client → server):

| Cmd | ID | Payload | Meaning |
|-----|----|---------|---------|
| `start_dose` | 0x00 | uint16 duration_s, uint8 level (1–254), uint8 direction (0/1) | Start a timed dose; replaces any running dose on that pump. duration_s clamped to 1–3600. |
| `stop_dose` | 0x01 | none | Abort dose, stop pump (same effect as On/Off Off). |

## State semantics

- `OnOff` attribute mirrors physical running state (true while dosing too) and is reported on change.
- Dose completion: pump stops, `OnOff → false`, `dose_remaining_s → 0`, both reported.
- Power-up: all pumps stopped, `OnOff=false` regardless of previous state (safety invariant — no OnOff startup behavior persistence).
