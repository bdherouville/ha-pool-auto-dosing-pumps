Title: [bug] RTT log backend in BLOCK mode stalls all logging when no debugger reads
Labels: bug, firmware, logging, fixed

**Symptom**: zero RTT output after a reboot; first boot after flash filled the 1 KB buffer
then logging went permanently silent.

**Root cause**: `CONFIG_LOG_BACKEND_RTT_MODE_BLOCK` waits for a host reader; headless
operation (normal for a pool controller) blocks the log thread.

**Fix**: `CONFIG_LOG_BACKEND_RTT_MODE_DROP=y` in `prj.conf` (initial import commit).
**Regression check**: test plan R3.
