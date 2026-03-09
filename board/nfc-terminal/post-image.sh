#!/bin/bash
# Post-image script for NFC Terminal

set -e

BOARD_DIR="$(dirname $0)"
GENIMAGE_CFG="${BOARD_DIR}/genimage.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

# Copy firmware files to root level for genimage
# The Pi bootloader expects files at root of boot partition
# Use our custom cmdline.txt instead of the default rpi-firmware one
cp "${BOARD_DIR}/cmdline.txt" "${BINARIES_DIR}/"
# Use our custom config.txt instead of the default rpi-firmware one
cp "${BOARD_DIR}/config.txt" "${BINARIES_DIR}/"
cp "${BINARIES_DIR}/rpi-firmware/fixup4.dat" "${BINARIES_DIR}/"
cp "${BINARIES_DIR}/rpi-firmware/start4.elf" "${BINARIES_DIR}/"

# Copy overlays directory to root level
# First, save any custom overlays that were built by our packages
mkdir -p "${BINARIES_DIR}/custom-overlays"
if [ -d "${BINARIES_DIR}/overlays" ]; then
    cp -a "${BINARIES_DIR}/overlays/"*.dtbo "${BINARIES_DIR}/custom-overlays/" 2>/dev/null || true
fi

# Copy rpi-firmware overlays
if [ -d "${BINARIES_DIR}/rpi-firmware/overlays" ]; then
    rm -rf "${BINARIES_DIR}/overlays"
    cp -a "${BINARIES_DIR}/rpi-firmware/overlays" "${BINARIES_DIR}/"
fi

# Restore our custom overlays (ST7703, FT6336U, etc.)
if [ -d "${BINARIES_DIR}/custom-overlays" ]; then
    cp -a "${BINARIES_DIR}/custom-overlays/"*.dtbo "${BINARIES_DIR}/overlays/" 2>/dev/null || true
    rm -rf "${BINARIES_DIR}/custom-overlays"
fi

# ============================================
# RAUC A/B Partition Setup
# ============================================

# Create initial boot state file for the boot partition
cat > "${BINARIES_DIR}/boot.ini" << 'BOOTINI'
PRIMARY="A"
A_OK=1
A_ATTEMPTS=3
B_OK=0
B_ATTEMPTS=0
BOOTINI

# Create empty rootfs_b partition image (will be populated by first OTA update)
ROOTFS_SIZE=$(stat -c%s "${BINARIES_DIR}/rootfs.ext2")
rm -f "${BINARIES_DIR}/rootfs_b.ext4"
truncate -s ${ROOTFS_SIZE} "${BINARIES_DIR}/rootfs_b.ext4"
${HOST_DIR}/sbin/mkfs.ext4 -q -L "rootfs_b" "${BINARIES_DIR}/rootfs_b.ext4"

# Create empty data partition image (16MB for RAUC status and persistent data)
rm -f "${BINARIES_DIR}/data.ext4"
truncate -s 16M "${BINARIES_DIR}/data.ext4"
${HOST_DIR}/sbin/mkfs.ext4 -q -L "data" "${BINARIES_DIR}/data.ext4"

# ============================================

# Create empty rootpath for genimage
trap 'rm -rf "${ROOTPATH_TMP}"' EXIT
ROOTPATH_TMP="$(mktemp -d)"

rm -rf "${GENIMAGE_TMP}"

genimage \
    --rootpath "${ROOTPATH_TMP}" \
    --tmppath "${GENIMAGE_TMP}" \
    --inputpath "${BINARIES_DIR}" \
    --outputpath "${BINARIES_DIR}" \
    --config "${GENIMAGE_CFG}"

echo "NFC Terminal image generated successfully"
echo "Output: ${BINARIES_DIR}/nfc-terminal.img"

exit $?
