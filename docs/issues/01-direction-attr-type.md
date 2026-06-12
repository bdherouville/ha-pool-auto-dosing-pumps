Title: [bug] direction attribute declared U8 instead of enum8 — writes rejected with INVALID_TYPE
Labels: bug, firmware, fixed

**Symptom**: z2m `Publish 'set' 'direction'` failed; ZBOSS rejects the write because the
frame's data type (enum8, per spec and converter) does not match the attribute descriptor.

**Root cause**: `firmware/src/zb_pump.c` declared `PUMP_ATTR_DIRECTION` as
`ZB_ZCL_ATTR_TYPE_U8` (0x20) while `specs/02-zigbee-model.md` mandates enum8 (0x30).
ZBOSS `zcl_general_commands.c` rejects writes when `attr_desc->type != write_attr_req->attr_type`.

**Fix**: declare the attribute `ZB_ZCL_ATTR_TYPE_8BIT_ENUM` (initial import commit).
**Regression check**: test plan R1.
