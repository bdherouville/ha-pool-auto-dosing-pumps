#!/usr/bin/env bash
# Countdown then flash via J-Link — no confirmation, just time to seat the pogo pins.
# Usage: tools/flash.sh [countdown_seconds] [--no-erase]
#   First flash must erase (removes UF2 bootloader); pass --no-erase for later reflashes.

set -e

COUNT="${1:-5}"
ERASE="--erase"
[[ "$*" == *--no-erase* ]] && ERASE=""

BUILD_DIR="$(cd "$(dirname "$0")/.." && pwd)/firmware/build"

echo "Flashing in:"
for ((i=COUNT; i>0; i--)); do
	printf '  %d...\n' "$i"
	sleep 1
done
echo "GO — hold the pogo pins steady"

# NCS workspace root (west topdir); override with NCS_ROOT if yours lives elsewhere
cd "${NCS_ROOT:-$HOME/ncs}"
west flash --build-dir "$BUILD_DIR" --runner jlink $ERASE
echo "DONE — board should reboot into the pump firmware (check RTT for 'piscine boot')"
