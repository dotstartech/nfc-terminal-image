# NFC Terminal Image - Raspberry Pi CM4

Custom Linux image built with Buildroot for Raspberry Pi Compute Module 4, carrier board [dotstartech/wall-panel](https://github.com/dotstartech/wall-panel) and [4 inch MIPI DSI LCD panel with ST7703 controller](https://github.com/dotstartech/st7703-gx040hd-driver)

## Target Hardware

- **Board**: Raspberry Pi Compute Module 4
- **Carrier**: [dotstartech/wall-panel](https://github.com/dotstartech/wall-panel)
- **RAM**: 1GB
- **Storage**: 8GB eMMC
- **WiFi/BT**: None
- **Display**: GX040HD-30MB-A1 (720x720 IPS LCD with ST7703 controller)
- **Touch**: FocalTech FT6336U (I2C0 @ 0x48)
- **NFC**: NXP PN7150 (I2C1 @ 0x28)
- **RTC**: DS3231 (I2C1 @ 0x68)

## Features

- Linux Kernel 6.12 (Raspberry Pi fork)
- [ST7703 display panel driver](https://github.com/dotstartech/st7703-gx040hd-driver)
- FT6336U touchscreen support
- DS3231 RTC support
- I2C interface enabled (I2C0 and I2C1)
- USB Ethernet (smsc95xx for PoE backplate)

## Directory Structure

```
nfc-terminal-image/
├── buildroot/              # Buildroot source (cloned)
├── board/nfc-terminal/     # Board-specific files
│   ├── config.txt          # Boot configuration
│   ├── cmdline.txt         # Kernel command line
│   ├── genimage.cfg        # Image generation config
│   ├── linux.config.fragment  # Kernel config additions
│   ├── post-build.sh       # Post-build customization
│   └── post-image.sh       # Image generation script
├── configs/                # Buildroot defconfigs
│   └── nfc_terminal_cm4_defconfig
├── package/                # Custom packages
│   └── st7703-gx040hd/     # Display driver package
├── external.desc           # External tree descriptor
├── external.mk             # External tree makefile
├── Config.in               # External tree Kconfig
└── README.md               # This file
```

## Building

### Prerequisites

Install build dependencies (Debian/Ubuntu):

```bash
sudo apt-get update
sudo apt-get install -y build-essential git wget cpio unzip rsync bc \
    libncurses-dev libssl-dev python3 python3-dev python3-setuptools file
```

### Build Steps

1. **Clone Buildroot**:

```bash
git clone https://github.com/buildroot/buildroot.git --branch 2024.02.x --depth 1
```

2. **Build the image**:

```bash
./build.sh
```

This configures Buildroot with the external tree and builds everything.

3. **Optional: Customize configuration**:

```bash
./build.sh menuconfig        # Main buildroot config
./build.sh linux-menuconfig  # Kernel config
./build.sh savedefconfig     # Save changes
```

4. **Output files** will be in `buildroot/output/images/`:
   - `nfc-terminal.img` - Complete bootable image
   - `Image` - Linux kernel
   - `rootfs.ext4` - Root filesystem
   - `bcm2711-rpi-cm4.dtb` - Device tree

## Flashing Image

1. Install boot jumpers on IO board to disable eMMC boot
2. Connect IO board USB to host
3. Flash using rpiboot:
```bash
sudo ./buildroot/output/host/bin/rpiboot

# Wait for eMMC to appear as mass storage, then:
sudo dd if=buildroot/output/images/nfc-terminal.img of=/dev/sdX bs=4M status=progress
sync
```
Alternatively Raspberry Pi Imager can be used to flush the image.

4. Disconnect IO board USB from host
5. Switch boot jumper back to eMMC boot
6. Connect LAN cable to PoE switch

## Configuration

### config.txt Parameters

Key display, I2C, and peripheral settings in `/boot/config.txt`:

```ini
# Display
dtoverlay=vc4-kms-v3d
dtoverlay=st7703-gx040hd
dtoverlay=ft6336u-gx040hd

# I2C
dtparam=i2c_arm=on    # I2C1 (GPIO 2/3) - RTC, NFC
dtparam=i2c_vc=on     # I2C0 (GPIO 0/1) - Touch controller

# RTC
dtoverlay=i2c-rtc,ds3231
```

### I2C Bus Usage

| Bus | Address | Device | Driver |
|-----|---------|--------|--------|
| I2C0 (i2c_vc) | 0x48 | FT6336U Touch Controller | edt_ft5x06 |
| I2C1 (i2c_arm) | 0x28 | PN7150 NFC Controller | nxp-nci-i2c |
| I2C1 (i2c_arm) | 0x68 | DS3231 RTC | rtc-ds1307 |

- **I2C0** (GPIO 0/1): Reserved for DSI display peripherals (touch controller)
- **I2C1** (GPIO 2/3): General peripherals (RTC, NFC, user devices)

## Credentials

- **Username**: root
- **Password**: nfc-terminal

## Customization

### Adding Packages

Edit `configs/nfc_terminal_cm4_defconfig` or use `make menuconfig`.

### Kernel Configuration

Modify `board/nfc-terminal/linux.config.fragment` for kernel options.

### Boot Configuration

Edit `board/nfc-terminal/config.txt` for Raspberry Pi boot parameters.

## Troubleshooting

### Display not working

```bash
# Check kernel messages
dmesg | grep -E "(st7703|dsi|panel|vc4)"

# Verify overlays loaded
dtoverlay -l
```

### Touch not responding

```bash
# Check I2C devices
i2cdetect -y 0    # Should show 0x48

# Test touch events
evtest /dev/input/event0
```

### I2C Issues

```bash
# List I2C buses
ls /dev/i2c-*

# Scan for devices
i2cdetect -y 1
```

## References

- [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html)
- [Carrier Board](https://github.com/dotstartech/wall-panel)
- [ST7703 Display Driver](https://github.com/dotstartech/st7703-gx040hd-driver)
- [Raspberry Pi Device Trees](https://www.raspberrypi.com/documentation/computers/configuration.html#device-trees-overlays-and-parameters)
- [CM4 Datasheet](https://datasheets.raspberrypi.com/cm4/cm4-datasheet.pdf)
