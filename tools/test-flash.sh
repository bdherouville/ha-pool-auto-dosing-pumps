#!/usr/bin/env bash
# Test SWD contact (1 try/second, 20 s windows); flash via JLinkExe the moment contact is solid.
# Keeps testing window after window until it succeeds. Ctrl-C to abort.
# Usage: tools/test-flash.sh [--no-erase]

HEX="$(cd "$(dirname "$0")/.." && pwd)/firmware/build/zephyr/merged.hex"
ERASE_CMD="erase"
[[ "$*" == *--no-erase* ]] && ERASE_CMD=""

while true; do
	echo "=== testing contact (20 s window) ==="
	ok=0
	for i in $(seq 1 20); do
		if printf 'connect\nnRF52840_xxAA\ns\n4000\nexit\n' | JLinkExe -nogui 1 2>&1 | grep -qi 'Cortex-M4 identified'; then
			echo "  $i/20: CONTACT OK — flashing"
			ok=1
			break
		fi
		echo "  $i/20: no contact"
		sleep 1
	done

	if [ "$ok" = 1 ]; then
		# r = hardware reset-and-halt: far more reliable than 'h' on a running/faulted core
		out=$(printf 'connect\nnRF52840_xxAA\ns\n1000\nr\nh\n%s\nloadfile %s\nr\ng\nexit\n' \
			"$ERASE_CMD" "$HEX" | JLinkExe -nogui 1 2>&1)
		if grep -q 'O.K.' <<<"$out" && ! grep -qE '\*\*\*\*\*\* Error' <<<"$out"; then
			echo "FLASH OK — target reset and running"
			exit 0
		fi
		echo "FLASH FAILED — back to testing"
		grep -iE 'error' <<<"$out" | head -3
	fi
done
