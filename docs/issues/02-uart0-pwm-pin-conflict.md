Title: [bug] uart0 console claims P0.06/P0.08, conflicting with pump 1 PWM
Labels: bug, firmware, devicetree, fixed

**Symptom**: P0.06 (pump 1 IN1) stuck around mid-rail regardless of commanded duty.

**Root cause**: board DTS enables `uart0` on P0.06 (TX) / P0.08 (RX) by default — the same
pins as pump 1's H-bridge inputs. Console is RTT-only per spec, so the UART served no purpose.

**Fix**: `&uart0 { status = "disabled"; };` in all board overlays + `CONFIG_SERIAL=n`,
`CONFIG_UART_CONSOLE=n` in `prj.conf` (initial import commit).
**Regression check**: test plan R2.
