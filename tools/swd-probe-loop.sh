#!/usr/bin/env bash
# Loop-test the J-Link SWD connection while adjusting pogo pins.
# Prints one status line per attempt; Ctrl-C to stop.

INTERVAL="${1:-1}"   # seconds between attempts (default 1)

while true; do
	out=$(printf 'connect\nnRF52840_xxAA\ns\n4000\nexit\n' | JLinkExe -nogui 1 2>&1)

	vtref=$(grep -oiE 'Measured: *[0-9.]+ *Volt' <<<"$out" | grep -oE '[0-9.]+' | head -1)
	[ -z "$vtref" ] && vtref=$(grep -oiE 'VTref *= *[0-9.]+V' <<<"$out" | grep -oE '[0-9.]+' | head -1)

	if grep -qiE 'Cortex-M4 identified' <<<"$out"; then
		idcode=$(grep -oiE 'IDCODE 0x[0-9A-F]+' <<<"$out" | head -1)
		echo "$(date +%T)  OK    VTref=${vtref:-?}V  target connected ${idcode}"
	elif grep -qiE 'voltage too low' <<<"$out"; then
		echo "$(date +%T)  FAIL  VTref=${vtref:-0}V  (no VTref / check 3.3V sense wire)"
	elif grep -qiE 'Could not connect to the target' <<<"$out"; then
		echo "$(date +%T)  FAIL  VTref=${vtref:-?}V  power OK but no SWD response (SWDIO/SWCLK contact?)"
	elif grep -qiE 'Cannot connect to J-Link|Connecting to J-Link.*FAILED' <<<"$out"; then
		echo "$(date +%T)  FAIL  J-Link probe not found on USB"
	else
		echo "$(date +%T)  ????  unrecognized output; run JLinkExe manually to inspect"
	fi

	sleep "$INTERVAL"
done
