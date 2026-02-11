#!/bin/bash
# Post-build script for NFC Terminal

set -u
set -e

BOARD_DIR="$(dirname $0)"

# Configure tty1 to run nfcDemoApp instead of getty
# This displays NFC polling output on the screen
if [ -e ${TARGET_DIR}/etc/inittab ]; then
    # Remove default tty1 getty entry
    sed -i '/^tty1::/d' ${TARGET_DIR}/etc/inittab
    # Comment out other tty entries
    sed -i 's/^tty\([2-9]\)::/#tty\1::/' ${TARGET_DIR}/etc/inittab
    # Add nfcDemoApp on tty1 with respawn
    echo '' >> ${TARGET_DIR}/etc/inittab
    echo '# NFC Demo App on display (respawns if exits)' >> ${TARGET_DIR}/etc/inittab
    echo 'tty1::respawn:/usr/bin/nfc-console' >> ${TARGET_DIR}/etc/inittab
fi

# Enable I2C devices at boot
if [ ! -e ${TARGET_DIR}/etc/modules-load.d ]; then
    mkdir -p ${TARGET_DIR}/etc/modules-load.d
fi

cat > ${TARGET_DIR}/etc/modules-load.d/i2c.conf << 'EOF'
# I2C modules
i2c-dev
i2c-bcm2835
EOF

# Create I2C udev rules for non-root access
if [ ! -e ${TARGET_DIR}/etc/udev/rules.d ]; then
    mkdir -p ${TARGET_DIR}/etc/udev/rules.d
fi

cat > ${TARGET_DIR}/etc/udev/rules.d/99-i2c.rules << 'EOF'
# I2C device permissions
KERNEL=="i2c-[0-9]*", GROUP="i2c", MODE="0660"
EOF

# Create i2c group
if ! grep -q "^i2c:" ${TARGET_DIR}/etc/group; then
    echo "i2c:x:997:" >> ${TARGET_DIR}/etc/group
fi

# Load display driver module at boot
cat > ${TARGET_DIR}/etc/modules-load.d/display.conf << 'EOF'
# DRM/VC4 graphics stack for Raspberry Pi
drm
vc4
# ST7703 GX040HD Display Panel Driver
panel-sitronix-st7703-gx040hd
# EDT FT5x06 touchscreen driver (supports FT6336U)
edt_ft5x06
EOF

# Load NFC kernel driver at boot
cat > ${TARGET_DIR}/etc/modules-load.d/nfc.conf << 'EOF'
# PN5xx NFC I2C driver (creates /dev/pn544)
pn5xx_i2c
EOF

# Create udev rule for NFC device permissions
cat > ${TARGET_DIR}/etc/udev/rules.d/99-nfc.rules << 'EOF'
# PN5xx NFC device - allow all users to access
KERNEL=="pn544", MODE="0666"
EOF

# Configure libnfc-nci to use the correct device node (/dev/pn544)
# The pn5xx kernel driver creates /dev/pn544, not /dev/pn54x
if [ -e ${TARGET_DIR}/etc/libnfc-nxp-init.conf ]; then
    # Add device node configuration if not present
    if ! grep -q "NXP_NFC_DEV_NODE" ${TARGET_DIR}/etc/libnfc-nxp-init.conf; then
        echo "" >> ${TARGET_DIR}/etc/libnfc-nxp-init.conf
        echo "###############################################################################" >> ${TARGET_DIR}/etc/libnfc-nxp-init.conf
        echo "# NFC Device Node (created by pn5xx_i2c kernel driver)" >> ${TARGET_DIR}/etc/libnfc-nxp-init.conf
        echo "NXP_NFC_DEV_NODE=\"/dev/pn544\"" >> ${TARGET_DIR}/etc/libnfc-nxp-init.conf
    fi
fi

# Create NFC console wrapper script for tty1
# This waits for the device, clears splash, and runs nfcDemoApp
cat > ${TARGET_DIR}/usr/bin/nfc-console << 'NFCEOF'
#!/bin/sh
#
# NFC Console - runs nfcDemoApp on the display (tty1)
#

NFC_DEV="/dev/pn544"

# Wait for NFC device to appear (max 30 seconds)
echo "Waiting for NFC hardware..."
count=0
while [ ! -c "$NFC_DEV" ] && [ $count -lt 60 ]; do
    sleep 0.5
    count=$((count + 1))
done

if [ ! -c "$NFC_DEV" ]; then
    echo "ERROR: NFC device $NFC_DEV not found!"
    echo "Check: lsmod | grep pn5xx"
    echo "       dmesg | grep -i nfc"
    echo ""
    echo "Press Enter to retry..."
    read dummy
    exit 1
fi

# Clear the screen (removes splash image)
clear

# Set permissions
chmod 666 "$NFC_DEV" 2>/dev/null

# Run nfcDemoApp in foreground on this console
echo "Starting NFC polling..."
echo ""
exec /usr/bin/nfcDemoApp poll
NFCEOF
chmod 755 ${TARGET_DIR}/usr/bin/nfc-console

# Create early boot script to run depmod and load display modules
# This runs before S10udev to ensure modules are available
cat > ${TARGET_DIR}/etc/init.d/S01depmod << 'INITEOF'
#!/bin/sh
#
# S01depmod - Run depmod to generate modules.dep and load display modules
#

case "$1" in
  start)
        echo "Running depmod..."
        KVER=$(uname -r)
        /sbin/depmod -a "$KVER" 2>/dev/null || true
        
        echo "Loading display modules..."
        # Load VC4 graphics driver (required for DSI display)
        modprobe drm 2>/dev/null || true
        modprobe vc4 2>/dev/null || true
        # Load panel driver
        modprobe panel-sitronix-st7703-gx040hd 2>/dev/null || true
        # Load touchscreen driver
        modprobe edt_ft5x06 2>/dev/null || true
        ;;
  stop)
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
INITEOF
chmod 755 ${TARGET_DIR}/etc/init.d/S01depmod

# Copy boot splash logo to target
mkdir -p ${TARGET_DIR}/usr/share/images
cp ${BOARD_DIR}/logo-mid.png ${TARGET_DIR}/usr/share/images/splash.png

# Create early splash screen init script - runs as early as possible
# S11splash runs right after S10udev (which creates /dev/fb0)
cat > ${TARGET_DIR}/etc/init.d/S11splash << 'SPLASHEOF'
#!/bin/sh
#
# S11splash - Display boot splash as soon as framebuffer is available
#

case "$1" in
  start)
        # Wait for framebuffer to appear (max 10 seconds)
        count=0
        while [ ! -e /dev/fb0 ] && [ $count -lt 100 ]; do
            usleep 100000  # 100ms
            count=$((count + 1))
        done
        
        # Display splash image immediately when fb0 is ready
        if [ -e /dev/fb0 ] && [ -x /usr/bin/fbv ]; then
            # Clear screen and display centered logo
            /usr/bin/fbv -c -f /usr/share/images/splash.png >/dev/null 2>&1 &
        fi
        ;;
  stop)
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
SPLASHEOF
chmod 755 ${TARGET_DIR}/etc/init.d/S11splash

# Keep disabled splash script for reference
cat > ${TARGET_DIR}/etc/init.d/S50splash.disabled << 'SPLASHEOF'
#!/bin/sh
#
# S50splash - Display boot splash image after display is fully initialized
#

case "$1" in
  start)
        # Small delay to ensure console switch is complete
        sleep 1
        
        # Display splash image centered on screen
        if [ -e /dev/fb0 ] && [ -x /usr/bin/fbv ]; then
            # Display the logo centered (fbv will handle clearing)
            /usr/bin/fbv -c -f /usr/share/images/splash.png >/dev/null 2>&1 &
        fi
        ;;
  stop)
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
SPLASHEOF
chmod 755 ${TARGET_DIR}/etc/init.d/S50splash.disabled

# Run depmod to generate modules.dep (required for modprobe to work)
KERNEL_VERSION=$(ls ${TARGET_DIR}/lib/modules/ | head -1)
if [ -n "${KERNEL_VERSION}" ]; then
    ${HOST_DIR}/sbin/depmod -a -b ${TARGET_DIR} ${KERNEL_VERSION} 2>/dev/null || true
fi

# Load USB Ethernet modules at boot
cat > ${TARGET_DIR}/etc/modules-load.d/usb-ethernet.conf << 'EOF'
# USB Ethernet modules (for LAN9500A on PoE backplate)
smsc95xx
usbnet
EOF

# Create network interfaces file that handles USB Ethernet
mkdir -p ${TARGET_DIR}/etc/network
cat > ${TARGET_DIR}/etc/network/interfaces << 'EOF'
# Loopback
auto lo
iface lo inet loopback

# Native Ethernet (official IO Board)
auto eth0
iface eth0 inet dhcp
    pre-up sleep 2

# USB Ethernet - usb0 naming (common for USB NICs)
auto usb0
iface usb0 inet dhcp
    pre-up sleep 3

# Allow hotplug for any additional interfaces
allow-hotplug eth1
iface eth1 inet dhcp

allow-hotplug enx*
iface enx* inet dhcp
EOF

# Create udev rule to rename LAN9500A to usb0 consistently
cat > ${TARGET_DIR}/etc/udev/rules.d/70-usb-ethernet.rules << 'EOF'
# Rename LAN9500A (SMSC95xx) USB Ethernet to usb0
SUBSYSTEM=="net", ACTION=="add", DRIVERS=="smsc95xx", NAME="usb0"
EOF

# Create /boot directory and add to fstab for boot partition mounting
mkdir -p ${TARGET_DIR}/boot
if ! grep -q "/dev/mmcblk0p1" ${TARGET_DIR}/etc/fstab; then
    echo "# Boot partition (config.txt, overlays, etc.)" >> ${TARGET_DIR}/etc/fstab
    echo "/dev/mmcblk0p1  /boot           vfat    defaults,noatime        0       2" >> ${TARGET_DIR}/etc/fstab
fi

# Create boot-complete init script that blinks LED to indicate successful boot
mkdir -p ${TARGET_DIR}/etc/init.d
cat > ${TARGET_DIR}/etc/init.d/S99boot-complete << 'EOF'
#!/bin/sh
#
# Boot complete indicator - blinks ACT LED to show Linux has booted
#

case "$1" in
  start)
        echo "Boot complete - signaling via LED"
        # Set ACT LED to heartbeat to indicate system is running
        if [ -e /sys/class/leds/ACT/trigger ]; then
            echo heartbeat > /sys/class/leds/ACT/trigger
        elif [ -e /sys/class/leds/led0/trigger ]; then
            echo heartbeat > /sys/class/leds/led0/trigger
        fi
        
        # Wait for network interfaces to appear and log debug info
        echo "=== Network Debug Info ===" > /var/log/boot-network.log
        echo "Date: $(date)" >> /var/log/boot-network.log
        echo "" >> /var/log/boot-network.log
        
        echo "=== Loaded Modules ===" >> /var/log/boot-network.log
        lsmod | grep -E "smsc|usb|net" >> /var/log/boot-network.log 2>&1
        echo "" >> /var/log/boot-network.log
        
        echo "=== USB Devices ===" >> /var/log/boot-network.log
        lsusb >> /var/log/boot-network.log 2>&1 || cat /sys/bus/usb/devices/*/product >> /var/log/boot-network.log 2>&1
        echo "" >> /var/log/boot-network.log
        
        echo "=== Network Interfaces ===" >> /var/log/boot-network.log
        ip link >> /var/log/boot-network.log 2>&1
        echo "" >> /var/log/boot-network.log
        
        echo "=== IP Addresses ===" >> /var/log/boot-network.log
        ip addr >> /var/log/boot-network.log 2>&1
        echo "" >> /var/log/boot-network.log
        
        echo "=== Trying to bring up any down interfaces ===" >> /var/log/boot-network.log
        for iface in /sys/class/net/*; do
            iface_name=$(basename $iface)
            if [ "$iface_name" != "lo" ]; then
                echo "Checking $iface_name..." >> /var/log/boot-network.log
                ip link set "$iface_name" up 2>> /var/log/boot-network.log
                udhcpc -i "$iface_name" -n -q -t 5 >> /var/log/boot-network.log 2>&1 &
            fi
        done
        
        # Wait a bit for DHCP
        sleep 10
        
        echo "=== Final Network Status ===" >> /var/log/boot-network.log
        ip addr >> /var/log/boot-network.log 2>&1
        
        echo "Network debug info logged to /var/log/boot-network.log"
        ;;
  stop)
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac
EOF
chmod 755 ${TARGET_DIR}/etc/init.d/S99boot-complete

# Install NFC diagnostic script
if [ -e ${BOARD_DIR}/nfc-diag.sh ]; then
    install -D -m 0755 ${BOARD_DIR}/nfc-diag.sh ${TARGET_DIR}/usr/bin/nfc-diag
fi

# Remove S95nfc if present (nfc-console on tty1 handles autostart)
rm -f ${TARGET_DIR}/etc/init.d/S95nfc*

echo "NFC Terminal post-build completed"
