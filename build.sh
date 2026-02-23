#!/bin/bash
# Build script for NFC Terminal image

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDROOT_DIR="${SCRIPT_DIR}/buildroot"
EXTERNAL_DIR="${SCRIPT_DIR}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[*]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[!]${NC} $1"
}

# Apply patches to buildroot
apply_patches() {
    local patches_dir="${SCRIPT_DIR}/patches/buildroot"
    local marker_file="${BUILDROOT_DIR}/.patches_applied"
    
    if [ -d "${patches_dir}" ] && [ -n "$(ls -A ${patches_dir}/*.patch 2>/dev/null)" ]; then
        # Check if patches already applied
        if [ -f "${marker_file}" ]; then
            return 0
        fi
        
        print_status "Applying buildroot patches..."
        for patch in ${patches_dir}/*.patch; do
            if [ -f "${patch}" ]; then
                print_status "  Applying $(basename ${patch})..."
                if ! patch -p1 -N --dry-run < "${patch}" > /dev/null 2>&1; then
                    print_warning "  Patch $(basename ${patch}) already applied or conflicts, skipping"
                else
                    patch -p1 < "${patch}"
                fi
            fi
        done
        touch "${marker_file}"
    fi
}

# Check if buildroot exists
if [ ! -d "${BUILDROOT_DIR}" ]; then
    print_error "Buildroot directory not found. Please clone buildroot first:"
    echo "  git clone https://github.com/buildroot/buildroot.git --branch 2024.02.x --depth 1"
    exit 1
fi

cd "${BUILDROOT_DIR}"

# Apply patches to buildroot if needed
apply_patches

case "${1:-build}" in
    config)
        print_status "Configuring buildroot with NFC Terminal defconfig..."
        make BR2_EXTERNAL="${EXTERNAL_DIR}" nfc_terminal_cm4_defconfig
        print_status "Configuration complete. Run '$0 build' to compile."
        ;;

    menuconfig)
        print_status "Opening Buildroot menuconfig..."
        make menuconfig
        ;;

    linux-menuconfig)
        print_status "Opening Linux kernel menuconfig..."
        make linux-menuconfig
        ;;

    build)
        # Parse --demo-app flag
        DEMO_APP=false
        if [[ "$2" == "--demo-app" ]]; then
            DEMO_APP=true
        fi

        print_status "Building NFC Terminal image..."
        if $DEMO_APP; then
            print_status "Demo app (nfc-lvgl-app) will be included."
        else
            print_status "Demo app (nfc-lvgl-app) will NOT be included."
        fi
        print_warning "This may take 30-60 minutes on first build."
        
        # Ensure configured
        if [ ! -f ".config" ]; then
            print_status "No configuration found, running config first..."
            make BR2_EXTERNAL="${EXTERNAL_DIR}" nfc_terminal_cm4_defconfig
        fi

        # Enable/disable nfc-lvgl-app based on --demo-app flag
        if $DEMO_APP; then
            # Enable the demo app
            if grep -q "# BR2_PACKAGE_NFC_LVGL_APP is not set" .config; then
                sed -i 's/# BR2_PACKAGE_NFC_LVGL_APP is not set/BR2_PACKAGE_NFC_LVGL_APP=y/' .config
            elif ! grep -q "BR2_PACKAGE_NFC_LVGL_APP" .config; then
                echo "BR2_PACKAGE_NFC_LVGL_APP=y" >> .config
            fi
        else
            # Disable the demo app
            if grep -q "BR2_PACKAGE_NFC_LVGL_APP=y" .config; then
                sed -i 's/BR2_PACKAGE_NFC_LVGL_APP=y/# BR2_PACKAGE_NFC_LVGL_APP is not set/' .config
            fi
        fi
        # Update config to resolve dependencies
        make olddefconfig
        
        # Build with parallel jobs
        JOBS=$(nproc)
        print_status "Building with ${JOBS} parallel jobs..."
        make -j${JOBS}
        
        print_status "Build complete!"
        print_status "Output image: ${BUILDROOT_DIR}/output/images/nfc-terminal.img"
        ;;

    clean)
        print_status "Cleaning build..."
        make clean
        ;;

    distclean)
        print_warning "This will remove all build artifacts and configuration!"
        read -p "Are you sure? (y/N) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            make distclean
            print_status "Distclean complete."
        fi
        ;;

    rebuild-kernel)
        print_status "Rebuilding kernel only..."
        make linux-rebuild
        make
        ;;

    rebuild-driver)
        print_status "Rebuilding ST7703 driver..."
        make st7703-gx040hd-rebuild
        make
        ;;

    savedefconfig)
        print_status "Saving current config to defconfig..."
        make savedefconfig BR2_DEFCONFIG="${EXTERNAL_DIR}/configs/nfc_terminal_cm4_defconfig"
        print_status "Saved to ${EXTERNAL_DIR}/configs/nfc_terminal_cm4_defconfig"
        ;;

    flash)
        if [ -z "$2" ]; then
            print_error "Usage: $0 flash /dev/sdX"
            exit 1
        fi
        DEVICE="$2"
        IMAGE="${BUILDROOT_DIR}/output/images/nfc-terminal.img"
        
        if [ ! -f "${IMAGE}" ]; then
            print_error "Image not found: ${IMAGE}"
            print_error "Run '$0 build' first."
            exit 1
        fi
        
        print_warning "This will ERASE all data on ${DEVICE}!"
        read -p "Are you sure? (y/N) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            print_status "Flashing image to ${DEVICE}..."
            sudo dd if="${IMAGE}" of="${DEVICE}" bs=4M status=progress conv=fsync
            sync
            print_status "Flash complete!"
        fi
        ;;

    rpiboot)
        print_status "Starting rpiboot for CM4 eMMC flashing..."
        if [ -x "${BUILDROOT_DIR}/output/host/bin/rpiboot" ]; then
            sudo "${BUILDROOT_DIR}/output/host/bin/rpiboot"
        else
            print_error "rpiboot not found. Build the image first."
            exit 1
        fi
        ;;

    *)
        echo "NFC Terminal Build Script"
        echo ""
        echo "Usage: $0 <command>"
        echo ""
        echo "Commands:"
        echo "  config          - Configure buildroot with NFC Terminal defconfig"
        echo "  menuconfig      - Open Buildroot configuration menu"
        echo "  linux-menuconfig - Open Linux kernel configuration menu"
        echo "  build [--demo-app] - Build the complete image (default, without demo app)"
        echo "  clean           - Clean build artifacts"
        echo "  distclean       - Remove all build artifacts and configuration"
        echo "  rebuild-kernel  - Rebuild only the kernel"
        echo "  rebuild-driver  - Rebuild only the ST7703 display driver"
        echo "  savedefconfig   - Save current config to defconfig file"
        echo "  flash <device>  - Flash image to SD card or eMMC device"
        echo "  rpiboot         - Start rpiboot for CM4 eMMC programming"
        ;;
esac
