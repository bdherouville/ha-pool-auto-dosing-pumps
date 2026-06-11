Title: [bug] speed never applied — Move-to-Level callback stubbed out + default level 0
Labels: bug, firmware, zigbee, fixed

**Symptom**: turning a pump ON produced no output; moving the HA speed slider had no effect.

**Root cause** (two parts, `firmware/src/zb_pump.c`):
1. `ZB_ZCL_LEVEL_CONTROL_SET_VALUE_CB_ID` was an empty case with a comment claiming it was
   "already handled by SET_ATTR_VALUE_CB_ID" — but ZBOSS delivers Move-to-Level commands
   (what z2m sends) only through that callback. Speed changes were dropped.
2. `current_level` was initialized to 0; ON runs at current level, and duty < 51 (20%) is
   treated as stop per spec — so a factory-fresh device ignored ON entirely.

**Fix**: implement the callback (store level, re-apply live if dosing) and default
`current_level` to 254 (initial import commit).
**Regression check**: test plan R5.
