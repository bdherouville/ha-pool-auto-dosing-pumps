# Task 05 — zigbee2mqtt external converter

No firmware dependency (can run parallel to tasks 02–04). Touch ONLY: `z2m/piscine_pump4.js`.

Read `specs/04-z2m-converter.md`; contract summary (IDs verbatim from specs/02):

- Match: `zigbeeModel: ['PISCINE-PUMP4']`, vendor `DIY`.
- Multi-endpoint: `pump1..pump4` → endpoints 1..4; `meta: {multiEndpoint: true}`.
- Custom cluster `piscinePump` = 0xFC00, manufacturerCode 0x1234; attrs `direction`(0x0000 enum8), `doseDurationS`(0x0001 uint16), `doseRemainingS`(0x0002 uint16); commands `startDose` 0x00 (duration_s uint16, level uint8, direction uint8), `stopDose` 0x01.
- Exposes per pump: binary `state` (genOnOff), numeric `speed` 0–254 (genLevelCtrl CurrentLevel), enum `direction` [forward, reverse], numeric `dose_duration` 1–3600 s (settable, converter-side state), numeric `dose_remaining` (read-only, from reports), enum `dose` with single value `START` (set-only) that sends `startDose(dose_duration, speed, direction)`.
- `configure`: bind genOnOff + piscinePump on EP1–4; reporting: OnOff on change, doseRemainingS min 1 s / max 60 s / change 1.
- Modern zigbee-herdsman-converters style (z2m ≥ 1.35): export a definition object with custom `fromZigbee`/`toZigbee` arrays; register the custom cluster via `device.customClusters` / definition `customClusters` field if available, else use raw `command`/`readResponse` frames with manufacturerCode in options.

## Done-criteria

- `node --check z2m/piscine_pump4.js` passes.
- Report: list of exposes generated and how `startDose` gathers its three parameters.

Ambiguity in herdsman API surface → implement the simplest variant that passes `node --check`, and report the assumption made (this task allows ONE documented assumption since the live z2m version is unknown).
