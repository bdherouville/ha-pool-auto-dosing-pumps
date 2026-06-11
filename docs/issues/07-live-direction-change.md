Title: [enhancement] apply direction change immediately on a running pump
Labels: enhancement, firmware, spec, done

**Was**: direction was applied on next start only (spec'd behavior) — reversing required
OFF/ON cycling from HA.

**Change**: `specs/02-zigbee-model.md` updated; a direction write on a running pump now
restarts it in the new direction. Safety preserved: `pump_pwm` enforces stop + ≥50 ms
dead time on reversal (initial import commit).
**Regression check**: test plan R7.
