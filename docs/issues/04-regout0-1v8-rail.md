Title: [bug] GPIO rail at 1.8 V after full-chip erase — UICR REGOUT0 lost
Labels: bug, firmware, hardware, fixed

**Symptom**: every GPIO high measured ~1.8 V; L298N inputs (Vih ≈ 2.3 V) never registered
logic high, so pumps could not run. J-Link VTref also read 1.78 V.

**Root cause**: SuperMini boards power the nRF52840 via VDDH (USB 5 V). The internal
regulator output defaults to 1.8 V when UICR REGOUT0 is erased — and the first-flash
full-chip erase (which removes the UF2 bootloader) wipes UICR. The bootloader normally
programs REGOUT0=3V3.

**Fix**: `main.c` checks REGOUT0 at boot, programs 3V3 via NVMC and self-resets once
(same approach as Nordic's nrf52840dongle board code) (initial import commit).
**Regression check**: test plan R4.
