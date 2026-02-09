#!/bin/bash
# QEMU boot script for NFC Terminal image testing
#
# Note: QEMU doesn't have raspi4/CM4 machine type, so we use 'virt' machine
# This allows testing kernel boot and rootfs, but not Pi-specific hardware

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGES_DIR="${SCRIPT_DIR}/buildroot/output/images"

# Check if images exist
if [ ! -f "${IMAGES_DIR}/Image" ]; then
    echo "Error: Kernel image not found at ${IMAGES_DIR}/Image"
    echo "Please run './build.sh build' first"
    exit 1
fi

if [ ! -f "${IMAGES_DIR}/rootfs.ext4" ]; then
    echo "Error: Root filesystem not found at ${IMAGES_DIR}/rootfs.ext4"
    exit 1
fi

# Create a copy of rootfs for QEMU (to avoid modifying the original)
QEMU_ROOTFS="${IMAGES_DIR}/rootfs-qemu.ext4"
if [ ! -f "${QEMU_ROOTFS}" ] || [ "${IMAGES_DIR}/rootfs.ext4" -nt "${QEMU_ROOTFS}" ]; then
    echo "Creating QEMU rootfs copy..."
    cp "${IMAGES_DIR}/rootfs.ext4" "${QEMU_ROOTFS}"
fi

echo "Starting QEMU with ARM64 virt machine..."
echo "Console output will appear below."
echo "Press Ctrl-A X to exit QEMU"
echo ""

# Run QEMU with virt machine (generic ARM64 virtual machine)
# -M virt: Use the generic ARM virtual machine
# -cpu cortex-a72: Use Cortex-A72 CPU (same as CM4)
# -m 1G: 1GB RAM (same as your CM4)
# -kernel: Direct kernel boot
# -append: Kernel command line
# -drive: Root filesystem
# -nographic: Serial console only (no GUI)
# -netdev/device: Virtual network with user-mode networking

qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a72 \
    -m 1G \
    -kernel "${IMAGES_DIR}/Image" \
    -append "root=/dev/vda rw console=ttyAMA0,115200 loglevel=8" \
    -drive file="${QEMU_ROOTFS}",format=raw,if=none,id=hd0 \
    -device virtio-blk-pci,drive=hd0 \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-pci,netdev=net0 \
    -nographic

echo ""
echo "QEMU session ended."
