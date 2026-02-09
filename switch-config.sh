#!/bin/bash
# Script to switch between full and minimal boot configurations

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOARD_DIR="${SCRIPT_DIR}/board/nfc-terminal"
FIRMWARE_DIR="${SCRIPT_DIR}/buildroot/output/images/rpi-firmware"

case "$1" in
    minimal)
        echo "Switching to minimal config (no display overlays)..."
        cp "${BOARD_DIR}/config-minimal.txt" "${FIRMWARE_DIR}/config.txt"
        echo "Done. Now rebuild the image or mount the SD card to update."
        ;;
    debug)
        echo "Switching to ultra-minimal debug config..."
        cp "${BOARD_DIR}/config-debug.txt" "${FIRMWARE_DIR}/config.txt"
        echo "Done. Now rebuild the image or mount the SD card to update."
        ;;
    full)
        echo "Switching to full config (with display overlays)..."
        cp "${BOARD_DIR}/config.txt" "${FIRMWARE_DIR}/config.txt"
        echo "Done. Now rebuild the image or mount the SD card to update."
        ;;
    show)
        echo "Current config.txt:"
        echo "==================="
        cat "${FIRMWARE_DIR}/config.txt"
        ;;
    *)
        echo "Usage: $0 {minimal|debug|full|show}"
        echo ""
        echo "  debug   - Ultra-minimal config for debugging boot issues"
        echo "  minimal - Use boot config without display overlays"
        echo "  full    - Use boot config with ST7703 display overlays"
        echo "  show    - Display current configuration"
        exit 1
        ;;
esac
