# NFC LVGL Application

A touch-screen NFC terminal application built with LVGL v9.2.2.

## Features

- **Landing Page**: Navigate to different app modes
- **Simple Check-in**: Direct NFC tag scanning
- **Roles Booking**: Multi-role selection with check-in/check-out
- **EV Charging** *(placeholder)*
- **Settings Modal**: Theme selection, device info (IP, MAC, MQTT status)
- **Theme System**: High Contrast, Catppuccin Mocha (dark), Catppuccin Latte (light)

## Build Types

### Production Build (ARM64 - Buildroot)

The production build targets the Raspberry Pi CM4 with:
- Linux framebuffer (`/dev/fb0`)
- Touch input via evdev (`/dev/input/event4`)
- NFC via linux_nfc_api
- MQTT via Paho Async Client

Build is handled by Buildroot - see the main project README.

### Desktop Build (x86_64 - SDL2)

For rapid UI development and testing without hardware:

```bash
# Prerequisites (Ubuntu/Debian)
sudo apt install build-essential libsdl2-dev

# Build
cd package/nfc-lvgl-app/src
make -f Makefile.desktop

# Run
./desktop_build/nfc-lvgl-app-desktop
```

The desktop build:
- Uses SDL2 for display and mouse input
- Disables NFC (uses stub API)
- Disables MQTT
- Runs in a 720x720 window

Press **Ctrl+C** or close the window to exit.

## Directory Structure

```
src/
├── main.c                    # Main application
├── lv_conf.h                 # LVGL configuration
├── linux_nfc_api_stub.h      # NFC API stub for desktop
├── Makefile.desktop          # Desktop build makefile
├── lvgl/                     # LVGL library (auto-downloaded for desktop)
└── desktop_build/            # Desktop build output (gitignored)
```

## Conditional Compilation

The code uses `DESKTOP_BUILD` define to switch between:

| Feature         | Production        | Desktop          |
|-----------------|-------------------|------------------|
| Display         | Framebuffer       | SDL2 Window      |
| Input           | evdev touch       | SDL2 mouse       |
| NFC             | linux_nfc_api     | Stub (no-op)     |
| MQTT            | Paho Async        | Disabled         |

## Theme Colors

Each theme defines colors for:
- Background (`bg`, `modal_bg`)
- Buttons (`btn_default`, `btn_pressed`, `btn_selected`, `btn_checked`)
- Text (`text`, `text_secondary`, `btn_text`)
- Role states (`role_unselected_bg`, `role_unselected_text`)

Access colors via macros like `THEME_BG`, `THEME_TEXT`, `COLOR_YELLOW`, etc.
