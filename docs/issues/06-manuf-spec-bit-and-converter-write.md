Title: [bug] custom cluster writes fail end-to-end — MANUF_SPEC bit missing (fw) and write:true missing (z2m)
Labels: bug, firmware, z2m-converter, fixed

**Symptom**: direction/calibration writes failed; first as a client-side
"NOT_AUTHORIZED ... is not writable" (never transmitted), then — once the converter was
fixed — as device-side `UNSUPPORTED_ATTRIBUTE`.

**Root cause** (two layers):
1. zigbee-herdsman ≥2.x validates writability client-side; custom-cluster attribute
   definitions default to read-only unless they declare `write: true`
   (see Koenkk/zigbee2mqtt#30768). `z2m/piscine_pump4.js` declared only `{ID, type}`.
2. ZBOSS only matches manufacturer-specific writes against attributes whose `access`
   field carries `ZB_ZCL_ATTR_MANUF_SPEC`; storing the manuf code in the descriptor is
   not sufficient (`zb_zcl_get_attr_desc_manuf`). The firmware attribute lists lacked the bit.

**Fix**: `write: true` (+ `report: true` for doseRemainingS) in the converter; `| ZB_ZCL_ATTR_MANUF_SPEC`
on all five custom attributes in `zb_pump.c` (initial import commit).
**Regression check**: test plan R6.
