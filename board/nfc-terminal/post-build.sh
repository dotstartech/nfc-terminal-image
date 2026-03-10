#!/bin/bash
# Post-build script for NFC Terminal

set -u
set -e

BOARD_DIR="$(dirname $0)"

# Configure inittab: NFC app on tty1
if [ -e ${TARGET_DIR}/etc/inittab ]; then
    # Remove default tty1 getty entry and any previous NFC lines
    sed -i '/^tty1::/d' ${TARGET_DIR}/etc/inittab
    sed -i '/^# NFC Demo App/d' ${TARGET_DIR}/etc/inittab
    # Comment out other tty entries
    sed -i 's/^tty\([2-9]\)::/#tty\1::/' ${TARGET_DIR}/etc/inittab
    # Collapse multiple consecutive blank lines into one
    sed -i '/^$/N;/^\n$/d' ${TARGET_DIR}/etc/inittab

    # RPi CM4 without PSCI: the reboot() syscall doesn't restart the SoC.
    # Insert sysrq-b (kernel emergency restart) AFTER swapoff but BEFORE
    # umount — /proc must still be mounted for the write to succeed.
    sed -i '/^::shutdown:\/bin\/sh.*sysrq/d' ${TARGET_DIR}/etc/inittab
    sed -i '/^::shutdown:\/sbin\/swapoff/a\
::shutdown:/bin/sh -c '"'"'sync; echo b > /proc/sysrq-trigger'"'"'' ${TARGET_DIR}/etc/inittab

    # Add NFC Demo App on tty1 with respawn
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

# Load I2S MEMS microphone driver at boot (Adafruit SPH0645LM4H)
cat > ${TARGET_DIR}/etc/modules-load.d/mic.conf << 'EOF'
# Google VoiceHAT codec driver (used by Adafruit I2S MEMS Microphone)
snd-soc-googlevoicehat-codec
EOF

# ALSA configuration for I2S MEMS microphone with software volume boost
# The SPH0645LM4H has no hardware gain control, so we use ALSA softvol plugin.
# I2S is always stereo; the route plugin extracts the left channel as mono.
cat > ${TARGET_DIR}/etc/asound.conf << 'EOF'
# Hardware PCM for the I2S microphone (googlevoicehat-soundcard)
pcm.dmic_hw {
    type hw
    card sndrpigooglevoi
    channels 2
    format S32_LE
    rate 48000
}

# Software volume boost on top of dmic_hw
pcm.dmic_sv {
    type softvol
    slave.pcm dmic_hw
    control {
        name "Boost Capture Volume"
        card sndrpigooglevoi
    }
    min_dB -5.0
    max_dB 30.0
}

# Mono capture - extracts left channel from stereo I2S stream
pcm.dmic_raw {
    type route
    slave.pcm dmic_sv
    slave.channels 2
    ttable.0.0 1
}

# Plug wrapper - allows any format/rate, converts automatically to hw params
pcm.dmic {
    type plug
    slave.pcm dmic_raw
}
EOF

# Init script to set microphone boost level at boot
# The softvol control is only created after the first PCM open,
# so we do a brief dummy capture to instantiate it, then set the level.
cat > ${TARGET_DIR}/etc/init.d/S13mic << 'MICEOF'
#!/bin/sh
#
# S13mic - Initialize I2S microphone capture volume
#

CARD="sndrpigooglevoi"
BOOST_PCT=85

case "$1" in
  start)
        # Wait for sound card (max 5 seconds)
        count=0
        while ! cat /proc/asound/cards 2>/dev/null | grep -q "$CARD" && [ $count -lt 50 ]; do
            usleep 100000
            count=$((count + 1))
        done

        if ! cat /proc/asound/cards 2>/dev/null | grep -q "$CARD"; then
            echo "Mic: sound card $CARD not found, skipping"
            exit 0
        fi

        # Brief dummy capture to create the softvol control
        arecord -D dmic_sv -c2 -r 48000 -f S32_LE -d 1 /dev/null >/dev/null 2>&1

        # Set boost level
        if amixer -c "$CARD" cset numid=1 "${BOOST_PCT}%" >/dev/null 2>&1; then
            echo "Mic: boost set to ${BOOST_PCT}%"
        else
            echo "Mic: failed to set boost level"
        fi
        ;;
  stop)
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
MICEOF
chmod 755 ${TARGET_DIR}/etc/init.d/S13mic

# Load DS3231 RTC driver at boot
cat > ${TARGET_DIR}/etc/modules-load.d/rtc.conf << 'EOF'
# DS3231 RTC uses the ds1307 driver family
rtc-ds1307
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
# Only create if nfc-lvgl-app package hasn't installed its own version
if [ ! -x ${TARGET_DIR}/usr/bin/nfc-lvgl-app ]; then
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
fi

# Create hwclock init script to sync system clock from DS3231 RTC at boot
# and write system time back to RTC on shutdown.
# Runs as S12 (after S10udev creates /dev/rtc0, after S11modules loads rtc-ds1307)
# Note: the kernel's CONFIG_RTC_HCTOSYS=y also sets the clock from RTC at kernel
# init time, but hwclock provides a userspace fallback and handles shutdown sync.
cat > ${TARGET_DIR}/etc/init.d/S12hwclock << 'HWCLOCKEOF'
#!/bin/sh
#
# S12hwclock - Synchronize system clock with DS3231 hardware RTC
#

HWCLOCK_ARGS="-u"
RTC_DEV="/dev/rtc0"

case "$1" in
  start)
        if [ -e "$RTC_DEV" ]; then
            printf "Setting system clock from RTC: "
            hwclock $HWCLOCK_ARGS --hctosys && echo "OK" || echo "FAIL"
        else
            echo "RTC device $RTC_DEV not found, skipping hwclock"
        fi
        ;;
  stop)
        if [ -e "$RTC_DEV" ]; then
            printf "Saving system clock to RTC: "
            hwclock $HWCLOCK_ARGS --systohc && echo "OK" || echo "FAIL"
        fi
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
HWCLOCKEOF
chmod 755 ${TARGET_DIR}/etc/init.d/S12hwclock

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

# Set hostname to MAC address of the first Ethernet interface (lowercase, no colons)
# Runs as S02 (after S01depmod, before S10udev/S40network) so udhcpc sends it
# to the DHCP server, making the device reachable by hostname on the local network.
cat > ${TARGET_DIR}/etc/init.d/S02hostname << 'HOSTEOF'
#!/bin/sh
#
# S02hostname - Set hostname from Ethernet MAC address
#

case "$1" in
  start)
        # Find first Ethernet interface with a MAC address
        MAC=""
        for iface in /sys/class/net/eth* /sys/class/net/usb* /sys/class/net/enx*; do
            if [ -f "$iface/address" ]; then
                MAC=$(cat "$iface/address" 2>/dev/null)
                if [ -n "$MAC" ] && [ "$MAC" != "00:00:00:00:00:00" ]; then
                    break
                fi
                MAC=""
            fi
        done

        if [ -z "$MAC" ]; then
            echo "Hostname: no MAC found, keeping default"
            exit 0
        fi

        # Strip colons and lowercase: d8:3a:dd:90:cd:79 -> d83add90cd79
        NEWHOST=$(echo "$MAC" | tr -d ':' | tr 'A-F' 'a-f')

        hostname "$NEWHOST"
        echo "$NEWHOST" > /etc/hostname
        # Update /etc/hosts so local lookups work
        sed -i "s/127.0.1.1.*/127.0.0.1\t$NEWHOST/" /etc/hosts 2>/dev/null
        if ! grep -q "$NEWHOST" /etc/hosts; then
            echo "127.0.0.1\t$NEWHOST" >> /etc/hosts
        fi
        echo "Hostname: $NEWHOST"
        ;;
  stop)
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
HOSTEOF
chmod 755 ${TARGET_DIR}/etc/init.d/S02hostname

# Reboot safety guard — catches hangs during the shutdown sequence.
# rcK runs init scripts in reverse order, so S99zz runs FIRST during shutdown.
# It backgrounds a subshell that forces a kernel-level reboot (sysrq-b)
# after 15 seconds if the orderly shutdown gets stuck in rcK/swapoff/umount.
# The subshell traps SIGTERM so init's kill(-1, SIGTERM) can't stop it.
# Note: sysrq-b is used instead of 'reboot -f' because the RPi CM4 without
# PSCI fails to restart via the reboot() syscall after orderly shutdown.
cat > ${TARGET_DIR}/etc/init.d/S99zz-reboot-guard << 'GUARDEOF'
#!/bin/sh
case "$1" in
  start)
        ;;
  stop)
        (trap '' TERM; sleep 15; sync; echo b > /proc/sysrq-trigger) &
        ;;
esac
exit 0
GUARDEOF
chmod 755 ${TARGET_DIR}/etc/init.d/S99zz-reboot-guard

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
    echo "/dev/mmcblk0p1  /boot           vfat    defaults,noatime,iocharset=utf8  0       2" >> ${TARGET_DIR}/etc/fstab
else
    # Update existing entry to ensure iocharset=utf8 is present
    sed -i 's|/dev/mmcblk0p1.*|/dev/mmcblk0p1  /boot           vfat    defaults,noatime,iocharset=utf8  0       2|' ${TARGET_DIR}/etc/fstab
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

# Configure chrony for NTP time synchronization with RTC support
cat > ${TARGET_DIR}/etc/chrony.conf << 'CHRONYEOF'
# NFC Terminal Chrony Configuration

# Use public NTP pool servers
pool pool.ntp.org iburst

# Record the rate at which the system clock gains/losses time
driftfile /var/lib/chrony/drift

# Allow the system clock to be stepped in the first three updates
# if its offset is larger than 1 second
makestep 1.0 3

# Enable kernel synchronisation of the real-time clock (RTC)
rtcsync

# Serve time even if not synchronized to a time source
local stratum 10
CHRONYEOF

# ============================================
# Image size optimization: remove unneeded files
# ============================================

# Remove udev hardware database (17MB saved)
# Not needed on embedded device with fixed hardware
rm -rf ${TARGET_DIR}/etc/udev/hwdb.d
rm -f ${TARGET_DIR}/lib/udev/hwdb.bin

# Remove iproute2 extras (keep only 'ip' command)
for tool in tc ss bridge genl rtmon ifstat nstat rtacct lnstat; do
    rm -f ${TARGET_DIR}/sbin/${tool}
done
rm -rf ${TARGET_DIR}/usr/lib/tc

# ============================================
# RAUC OTA Update Integration
# ============================================

# Install RAUC system configuration
mkdir -p ${TARGET_DIR}/etc/rauc
cp ${BOARD_DIR}/rauc/system.conf ${TARGET_DIR}/etc/rauc/system.conf

# Install RAUC CA certificate (keyring for bundle verification)
if [ -f ${BOARD_DIR}/rauc/certs/ca.cert.pem ]; then
    cp ${BOARD_DIR}/rauc/certs/ca.cert.pem ${TARGET_DIR}/etc/rauc/ca.cert.pem
else
    echo "WARNING: RAUC CA certificate not found! Run board/nfc-terminal/rauc/certgen.sh first."
fi

# Install custom bootloader backend handler
mkdir -p ${TARGET_DIR}/usr/lib/rauc
cp ${BOARD_DIR}/rauc/rauc-boot-handler ${TARGET_DIR}/usr/lib/rauc/rauc-boot-handler
chmod 755 ${TARGET_DIR}/usr/lib/rauc/rauc-boot-handler

# Create mount points for boot and data partitions
mkdir -p ${TARGET_DIR}/boot
mkdir -p ${TARGET_DIR}/data

# Add fstab entries for boot and data partitions
if [ -e ${TARGET_DIR}/etc/fstab ]; then
    if ! grep -q "mmcblk0p1" ${TARGET_DIR}/etc/fstab; then
        echo "" >> ${TARGET_DIR}/etc/fstab
        echo "# Boot partition (shared between A/B slots)" >> ${TARGET_DIR}/etc/fstab
        echo "/dev/mmcblk0p1	/boot	vfat	defaults,noatime,iocharset=utf8	0	0" >> ${TARGET_DIR}/etc/fstab
    fi
    if ! grep -q "mmcblk0p4" ${TARGET_DIR}/etc/fstab; then
        echo "# Persistent data partition (RAUC status, configs)" >> ${TARGET_DIR}/etc/fstab
        echo "/dev/mmcblk0p4	/data	ext4	defaults,noatime	0	0" >> ${TARGET_DIR}/etc/fstab
    fi
fi

# Create RAUC mark-good init script
# Runs late (S99) after all services have started successfully
cat > ${TARGET_DIR}/etc/init.d/S99rauc << 'RAUCEOF'
#!/bin/sh
#
# S99rauc - Mark current RAUC slot as good after successful boot
#

case "$1" in
  start)
        if [ -x /usr/bin/rauc ]; then
            printf "Marking RAUC slot as good: "
            /usr/bin/rauc status mark-good && echo "OK" || echo "FAIL"
        fi
        ;;
  stop)
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
RAUCEOF
chmod 755 ${TARGET_DIR}/etc/init.d/S99rauc

echo "NFC Terminal post-build completed"
