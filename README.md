# NFC Terminal Image - Raspberry Pi CM4

Custom Linux image built with Buildroot for Raspberry Pi Compute Module 4, carrier board [dotstartech/wall-panel](https://github.com/dotstartech/wall-panel) and [4 inch MIPI DSI LCD panel with ST7703 controller](https://github.com/dotstartech/st7703-gx040hd-driver)

## Target Hardware

- **Board**: Raspberry Pi Compute Module 4 (**RAM**: 1GB, **Storage**: 8GB eMMC, **WiFi/BT**: None)
- **Carrier**: [dotstartech/wall-panel](https://github.com/dotstartech/wall-panel)
- **Display**: GX040HD-30MB-A1 (720x720 IPS LCD with ST7703 controller)
- **Touch**: FocalTech FT6336U (I2C0 @ 0x48)
- **NFC**: NXP PN7150 (I2C1 @ 0x28)
- **RTC**: DS3231 (I2C1 @ 0x68)

## Features

- Linux Kernel 6.12 (Raspberry Pi fork)
- [ST7703 display panel driver](https://github.com/dotstartech/st7703-gx040hd-driver)
- FT6336U touchscreen support
- PN7150 NFC with kernel driver (pn5xx_i2c) and libnfc-nci
- DS3231 RTC support
- I2C interface enabled (I2C0 and I2C1)
- USB Ethernet (smsc95xx for PoE backplate)

## Directory Structure

```
nfc-terminal-image/
├── buildroot/                 # Buildroot source (cloned)
├── board/nfc-terminal/        # Board-specific files
│   ├── config.txt             # Boot configuration
│   ├── cmdline.txt            # Kernel command line
│   ├── genimage.cfg           # Image generation config
│   ├── linux.config.fragment  # Kernel config additions
│   ├── overlays/              # Device tree overlays
│   │   └── nfc-pn7150.dts     # NFC PN7150 overlay
│   ├── post-build.sh          # Post-build customization
│   └── post-image.sh          # Image generation script
├── configs/                   # Buildroot defconfigs
│   └── nfc_terminal_cm4_defconfig
├── docs/                      # Reference documentation
│   └── AN11697.pdf            # NXP PN7150 Linux integration guide
├── package/                   # Custom packages
│   ├── libnfc-nci/            # NFC userspace library
│   ├── pn5xx-i2c/             # NFC kernel driver (patched)
│   └── st7703-gx040hd/        # Display driver package
├── external.desc              # External tree descriptor
├── external.mk                # External tree makefile
├── Config.in                  # External tree Kconfig
└── README.md                  # This file
```

## Building Image

Install build dependencies (Debian/Ubuntu):

```bash
sudo apt-get update
sudo apt-get install -y build-essential git wget cpio unzip rsync bc \
    libncurses-dev libssl-dev python3 python3-dev python3-setuptools file
```

1. **Clone Buildroot**:

```bash
git clone https://github.com/buildroot/buildroot.git --branch 2024.02.x --depth 1
```

2. **Build the image**:

```bash
./build.sh
```

This configures Buildroot with the external tree and builds everything.

3. **Build script commands**:

| Command | Description |
|---------|-------------|
| `./build.sh` | Build the complete image (default, without demo app) |
| `./build.sh build --demo-app` | Build the image with nfc-lvgl-app demo included |
| `./build.sh config` | Apply NFC Terminal defconfig without building |
| `./build.sh menuconfig` | Open Buildroot interactive configuration menu |
| `./build.sh linux-menuconfig` | Open Linux kernel configuration menu |
| `./build.sh savedefconfig` | Save current config back to defconfig file |
| `./build.sh clean` | Remove build artifacts (keeps config) |
| `./build.sh distclean` | Remove ALL build artifacts and configuration |
| `./build.sh rebuild-kernel` | Rebuild only the Linux kernel |
| `./build.sh rebuild-driver` | Rebuild only the ST7703 display driver |
| `./build.sh flash /dev/sdX` | Flash built image to SD card or eMMC device |
| `./build.sh rpiboot` | Start rpiboot for CM4 eMMC programming mode |

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
Alternatively Raspberry Pi Imager can be used to flash the image.

4. Disconnect IO board USB from host
5. Switch boot jumper back to eMMC boot
6. Connect LAN cable to PoE switch

## Testing with QEMU

The image can be tested in QEMU without real hardware for basic validation of boot and rootfs.

### Prerequisites

Install QEMU for ARM64:

```bash
# Debian/Ubuntu
sudo apt-get install qemu-system-arm

# Fedora
sudo dnf install qemu-system-aarch64
```

### Running

```bash
./qemu-run.sh
```

Press `Ctrl-A X` to exit QEMU.

### How It Works

QEMU doesn't have Raspberry Pi 4/CM4 machine emulation, so the script uses the generic ARM64 `virt` machine with a Cortex-A72 CPU. It performs direct kernel boot with the rootfs mounted as a virtio block device.

**What works in QEMU:**
- Kernel boot and init system
- Network via user-mode networking (SSH available on `localhost:2222`)
- Basic userspace and shell access

**What doesn't work (requires real hardware):**
- NFC (`/dev/pn544` won't exist - nfc-console will timeout after 30s)
- Display (ST7703) and touch
- I2C, GPIO, and other Pi-specific peripherals

SSH into the QEMU instance:

```bash
ssh -p 2222 root@localhost
# Password: nfc-terminal
```

## Configuration

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
| I2C1 (i2c_arm) | 0x28 | PN7150 NFC Controller | pn5xx_i2c |
| I2C1 (i2c_arm) | 0x68 | DS3231 RTC | rtc-ds1307 |

- **I2C0** (GPIO 0/1): Reserved for DSI display peripherals (touch controller)
- **I2C1** (GPIO 2/3): General peripherals (RTC, NFC, user devices)

## NFC Architecture

The NFC subsystem uses NXP's PN7150 chip interfaced via I2C, with kernel-level GPIO control as recommended by NXP's Linux integration guide ([AN11697](docs/AN11697.pdf), sections 3.1 and 4.2).

### Software Stack

```
┌─────────────────────────────────────────────┐
│           nfcDemoApp (poll mode)            │  User Application
├─────────────────────────────────────────────┤
│              libnfc-nci (R2.4)              │  NFC Middleware
│        (dotstartech/linux_libnfc-nci)       │
├─────────────────────────────────────────────┤
│         /dev/pn544 (char device)            │  Device Node
├─────────────────────────────────────────────┤
│           pn5xx_i2c.ko (kernel)             │  Kernel Driver
│      (GPIO control, I2C communication)      │
├─────────────────────────────────────────────┤
│    i2c1 @ 0x28    │    GPIO5 (INT)          │  Hardware
│                   │    GPIO6 (VEN)          │
├───────────────────┴─────────────────────────┤
│              NXP PN7150 Chip                │
└─────────────────────────────────────────────┘
```

**Key design choice:** The `--enable-alt` flag for libnfc-nci (userspace GPIO control via sysfs) does **not** work on modern kernels/CM4 because the GPIO sysfs interface fails for GPIOs managed by pinctrl. The kernel driver approach handles GPIO control internally, which is more reliable.

### pn5xx Kernel Driver (Kernel 6.x Patches)

The pn5xx_i2c driver from NXP required several modifications to compile on Linux kernel 6.x:

| Original Code | Fixed Code | Reason |
|---------------|------------|--------|
| `pr_warning(...)` | `pr_warn(...)` | `pr_warning` removed in kernel 5.x |
| `no_llseek` | `noop_llseek` | `no_llseek` removed in kernel 6.x |
| `of_get_named_gpio_flags(node, name, 0, &flags)` | `of_get_named_gpio(node, name, 0)` | API simplified, flags parameter removed |
| `static int pn54x_probe(struct i2c_client *client, const struct i2c_device_id *id)` | `static int pn54x_probe(struct i2c_client *client)` | `i2c_device_id` parameter removed in 6.3+ |
| `static int pn54x_remove(...)` | `static void pn54x_remove(...)` | Return type changed to void in 6.1+ |

The patched source is located at `package/pn5xx-i2c/pn5xx_i2c.c`.

### Device Tree Overlay

The NFC overlay (`board/nfc-terminal/overlays/nfc-pn7150.dts`) configures:

```dts
pn7150@28 {
    compatible = "nxp,pn547";       // Driver compatible string
    reg = <0x28>;                   // I2C address
    interrupt-gpios = <&gpio 5 0>;  // GPIO5 for data-ready interrupt
    enable-gpios = <&gpio 6 0>;     // GPIO6 for chip enable (VEN)
    interrupts = <5 1>;             // GPIO5, rising edge trigger
};
```

GPIO pin functions:
- **GPIO5 (BCM)**: Interrupt - signals when NFC data is ready to read
- **GPIO6 (BCM)**: Enable (VEN) - powers on/off the NFC chip

### Autostart

The `nfcDemoApp poll` command starts automatically at boot via the init script `/etc/init.d/S95nfc`. The script:
1. Waits for `/dev/pn544` device node (up to 15 seconds)
2. Sets appropriate permissions on the device
3. Launches `nfcDemoApp poll` in background

Logs are written to `/var/log/nfc.log`.

## Touch Integration (FT6336U)

The touchscreen uses a FocalTech FT6336U controller connected via I2C0 at address 0x48. The kernel driver `edt_ft5x06` handles the hardware, exposing touch events through the Linux input subsystem.

### Hardware Configuration

| Parameter | Value |
|-----------|-------|
| Controller | FocalTech FT6336U |
| I2C Bus | I2C0 (i2c_vc) |
| I2C Address | 0x48 |
| Interrupt GPIO | GPIO25 (falling edge) |
| Reset GPIO | GPIO24 (active-low) |
| Kernel Driver | edt_ft5x06 |
| Input Device | /dev/input/event4 |

### Device Tree Overlay

The touch overlay (`ft6336u-gx040hd.dtbo`) configures:
- I2C address and compatible string (`focaltech,ft6236`)
- Interrupt on GPIO25, falling edge trigger
- Reset control on GPIO24

### LVGL Application Integration

Key implementation details for integrating with LVGL v9.x:

1. **Input Device Setup**: Open `/dev/input/event4` with `O_RDONLY | O_NONBLOCK`

2. **Multitouch Protocol B**: The FT6336U uses Linux MT Protocol B with slots:
   - `ABS_MT_POSITION_X` / `ABS_MT_POSITION_Y` for coordinates
   - `ABS_MT_TRACKING_ID >= 0` indicates touch down, `-1` indicates touch up
   - Also sends `BTN_TOUCH` key events

3. **Coordinate Scaling**: Query touch range using `EVIOCGABS(ABS_MT_POSITION_X/Y)` and scale to display resolution (720x720)

4. **Display Refresh**: After UI changes from touch events, call:
   - `lv_obj_invalidate(obj)` to mark objects for redraw
   - `lv_refr_now(display)` to force immediate refresh

### Touch Event Flow

```
FT6336U Hardware
      │ (I2C0)
      ▼
edt_ft5x06 Kernel Driver
      │ (GPIO25 interrupt)
      ▼
Input Subsystem (/dev/input/event4)
      │ (read() with O_NONBLOCK)
      ▼
LVGL touch_read_cb()
      │ (parse EV_ABS events)
      ▼
LVGL Input Device (LV_INDEV_TYPE_POINTER)
      │
      ▼
LVGL Event System (LV_EVENT_CLICKED, etc.)
```

## MQTT Client

The nfc-lvgl-app uses the Eclipse Paho C MQTT library with asynchronous operation and automatic reconnection.

### Configuration

| Parameter | Value |
|-----------|-------|
| Broker | tcp://192.168.188.50:1883 |
| Client ID | nfc-terminal |
| Protocol | MQTT 3.1.1 (async) |
| QoS | 1 |
| Auto-Reconnect | Enabled (1-60s backoff) |

### Topics

| Topic | Description | Retained |
|-------|-------------|----------|
| `data/<MAC>/nfc` | NFC tag events (tagId, type) | No |
| `data/<MAC>/state` | Device state (ON/OFF) | Yes |

The `<MAC>` is the device's eth0 MAC address without colons (e.g., `DCDCA284B7C8`).

### Last Will and Testament (LWT)

On connection, the client registers an LWT message:
- **Topic**: `data/<MAC>/state`
- **Payload**: `{"state":"OFF"}`
- **Retained**: Yes

This ensures the broker publishes the offline state if the device disconnects unexpectedly.

### Auto-Reconnect Behavior

The async MQTT client automatically handles reconnection:
- **Min retry interval**: 1 second
- **Max retry interval**: 60 seconds
- **Backoff**: Exponential between min and max
- **Reconnection trigger**: Automatic on connection loss (no manual intervention required)

When connection is restored, the client automatically publishes `{"state":"ON"}` to the state topic.

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

# Verify overlays in device tree
ls /proc/device-tree/soc/dsi@*/
```

### Touch not responding

```bash
# Check I2C devices
i2cdetect -y 0    # Should show 0x48

# Check touch input device exists
ls -la /dev/input/event*

# Read raw touch events (hex dump)
cat /dev/input/event0 | hexdump -C
```

### I2C Issues

```bash
# List I2C buses
ls /dev/i2c-*

# Scan for devices
i2cdetect -y 1
```

### NFC Issues

```bash
# Check if kernel module loaded
lsmod | grep pn5xx

# Verify device node exists
ls -la /dev/pn544

# Check kernel messages for NFC driver
dmesg | grep -i "pn54x\|nfc"

# View autostart log
cat /var/log/nfc.log

# Verify NFC chip on I2C bus (should show 28)
i2cdetect -y 1

# Manually test NFC polling
/etc/init.d/S95nfc stop
nfcDemoApp poll

# Check GPIO status in sysfs
cat /sys/kernel/debug/gpio | grep -E "gpio-5|gpio-6"
```

## References

- [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html)
- [Carrier Board](https://github.com/dotstartech/wall-panel)
- [ST7703 Display Driver](https://github.com/dotstartech/st7703-gx040hd-driver)
- [libnfc-nci Fork](https://github.com/dotstartech/linux_libnfc-nci)
- [NXP PN7150 Linux Integration (AN11697)](docs/AN11697.pdf)
- [Raspberry Pi Device Trees](https://www.raspberrypi.com/documentation/computers/configuration.html#device-trees-overlays-and-parameters)
- [CM4 Datasheet](https://datasheets.raspberrypi.com/cm4/cm4-datasheet.pdf)
