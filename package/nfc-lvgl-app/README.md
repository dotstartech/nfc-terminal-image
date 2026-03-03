# NFC LVGL Application

A touch-screen NFC terminal application built with LVGL v9.2.2. Both the production (ARM64) and desktop (x86_64) builds support real NFC hardware and MQTT connectivity.

## Features

- **Landing Page**: Navigate to different app modes
- **Simple Check-in**: Direct NFC tag scanning
- **Roles Booking**: Multi-role selection with check-in/check-out
- **EV Charging** *(placeholder)*
- **Settings Modal**: Theme selection, device info (IP, MAC, MQTT status)
- **Theme System**: High Contrast, Catppuccin Mocha (dark), Catppuccin Latte (light)
- **Header Status Indicators**: NFC (U+F519) and MQTT (U+F6FF) icons show connection state in real time

## Build Types

### Production Build (ARM64 - Buildroot)

The production build targets the Raspberry Pi CM4 with:
- Linux framebuffer (`/dev/fb0`)
- Touch input via evdev (`/dev/input/event4`)
- NFC via linux_nfc_api (PN7150 over I2C)
- MQTT via Paho Async Client

Build is handled by Buildroot — see the main project README.

```bash
# From the repository root
./build.sh rebuild-demoapp
```

### Desktop Build (x86_64 - SDL2)

The desktop build uses the same NFC and MQTT code as production. NFC is provided by a USB dongle through `linux_libnfc-nci` built with LPCUSBSIO support.

#### Prerequisites

```bash
# Build tools and libraries
sudo apt install build-essential libsdl2-dev libpaho-mqtt-dev libudev-dev \
    libssl-dev autotools-dev autoconf automake libtool pkg-config
```

#### NFC USB Dongle Setup

The desktop build uses the [Mikroe NFC USB Dongle](https://www.mikroe.com/nfc-usb-dongle) (NXP PN7150 + LPC11U24, USB HID, VID `1fc9` PID `0088`).

**Build and install linux_libnfc-nci with LPCUSBSIO transport:**

```bash
git clone https://github.com/nickg803/linux_libnfc-nci.git
cd linux_libnfc-nci
./bootstrap
./configure --enable-lpcusbsio
make -j$(nproc)
sudo make install
sudo ldconfig
```

**Copy NFC configuration files:**

```bash
sudo mkdir -p /etc
sudo cp conf/libnfc-nci.conf /etc/
sudo cp conf/libnfc-nxp.conf /etc/
```

**Verify USB dongle detection:**

```bash
# Plug in the dongle and check
lsusb | grep 1fc9
# Expected: Bus XXX Device XXX: ID 1fc9:0088 NXP Semiconductors

ls /dev/hidraw*
# The dongle appears as a /dev/hidrawN device
```

**udev rules (optional, for non-root access):**

```bash
echo 'SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1fc9", ATTRS{idProduct}=="0088", MODE="0666"' \
    | sudo tee /etc/udev/rules.d/99-nfc-usb-dongle.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

#### Building and Running

```bash
# From the repository root
./build.sh desktop-build

# Run (may need sudo if udev rules are not set)
./package/nfc-lvgl-app/src/desktop_build/nfc-lvgl-app-desktop
```

The desktop build:
- Uses SDL2 for display and mouse input
- Uses `linux_libnfc-nci` (LPCUSBSIO TML) for real NFC via USB dongle
- Uses Paho MQTT Async Client for MQTT
- Runs in a 720×720 window

Press **Ctrl+C** or close the window to exit.

## How It Works

```
┌─────────────────────────────────────────────────────┐
│                  nfc-lvgl-app                        │
│                                                     │
│   LVGL UI ──► SDL2 window (desktop)                 │
│            ──► /dev/fb0    (production)              │
│                                                     │
│   NFC     ──► linux_libnfc-nci                      │
│               ├── I2C TML (/dev/pn5xx) (production) │
│               └── LPCUSBSIO TML (/dev/hidrawN)      │
│                   (desktop, via USB dongle)          │
│                                                     │
│   MQTT    ──► Paho Async Client ──► broker          │
└─────────────────────────────────────────────────────┘
```

## Directory Structure

```
src/
├── main.c                    # Main application (~2550 lines)
├── lv_conf.h                 # LVGL configuration
├── Makefile.desktop          # Desktop build makefile
├── fonts/
│   ├── fa_solid_48.c         # Font Awesome 48px (8 glyphs)
│   └── fa-solid-900.otf      # Font Awesome source OTF
├── lvgl/                     # LVGL library (auto-downloaded for desktop)
└── desktop_build/            # Desktop build output (gitignored)
```

## Conditional Compilation

The `DESKTOP_BUILD` define switches display and input backends. NFC and MQTT are shared:

| Feature         | Production           | Desktop                      |
|-----------------|----------------------|------------------------------|
| Display         | Framebuffer          | SDL2 Window                  |
| Input           | evdev touch          | SDL2 mouse                   |
| NFC hardware    | PN7150 (I2C)         | PN7150 via USB dongle (HID)  |
| NFC library     | linux_nfc_api        | linux_nfc_api                |
| MQTT            | Paho Async           | Paho Async                   |

## Header Status Indicators

The header bar shows two Font Awesome status icons:

| Icon | Glyph  | Meaning |
|------|--------|---------|
| NFC  | U+F519 | Grey = not ready, Green = NFC initialized |
| MQTT | U+F6FF | Grey = disconnected, Green = connected |

## Theme Colors

Each theme defines colors for:
- Background (`bg`, `modal_bg`)
- Buttons (`btn_default`, `btn_pressed`, `btn_selected`, `btn_checked`)
- Text (`text`, `text_secondary`, `btn_text`)
- Role states (`role_unselected_bg`, `role_unselected_text`)

Access colors via macros like `THEME_BG`, `THEME_TEXT`, `COLOR_YELLOW`, etc.
