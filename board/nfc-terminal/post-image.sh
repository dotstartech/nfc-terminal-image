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
# Only include rpi-firmware overlays actually referenced in config.txt
# Custom overlays (nfc-pn7150, st7703-gx040hd, ft6336u-gx040hd) are installed
# directly by their package .mk files into ${BINARIES_DIR}/overlays/

# rpi-firmware overlays needed by config.txt dtoverlay= lines:
# Note: vc4-kms-v3d-pi4.dtbo is required by vc4-kms-v3d.dtbo (resolved via overlay_map.dtb)
RPI_OVERLAYS="
    i2c-rtc.dtbo
    dwc2.dtbo
    disable-bt.dtbo
    vc4-kms-v3d.dtbo
    vc4-kms-v3d-pi4.dtbo
    googlevoicehat-soundcard.dtbo
"

if [ -d "${BINARIES_DIR}/rpi-firmware/overlays" ]; then
    mkdir -p "${BINARIES_DIR}/overlays"
    for dtbo in ${RPI_OVERLAYS}; do
        if [ -f "${BINARIES_DIR}/rpi-firmware/overlays/${dtbo}" ]; then
            cp -a "${BINARIES_DIR}/rpi-firmware/overlays/${dtbo}" "${BINARIES_DIR}/overlays/"
        else
            echo "WARNING: Expected overlay ${dtbo} not found in rpi-firmware" >&2
        fi
    done
    # overlay_map.dtb is used by Pi firmware to resolve platform-specific overlays
    if [ -f "${BINARIES_DIR}/rpi-firmware/overlays/overlay_map.dtb" ]; then
        cp -a "${BINARIES_DIR}/rpi-firmware/overlays/overlay_map.dtb" "${BINARIES_DIR}/overlays/"
    fi
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
