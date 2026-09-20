################################################################################
#
# st7703-gx040hd
#
################################################################################

# Pinned: adds LM3630A I2C backlight driver + split panel dtsi (wall-panel rev >= 5)
ST7703_GX040HD_VERSION = 1c26f1999d36160135d5a78d16f3d57835cd9c34
ST7703_GX040HD_SITE = $(call github,dotstartech,st7703-gx040hd-driver,$(ST7703_GX040HD_VERSION))
ST7703_GX040HD_LICENSE = GPL-2.0
ST7703_GX040HD_LICENSE_FILES = LICENSE

ST7703_GX040HD_DEPENDENCIES = linux

define ST7703_GX040HD_BUILD_CMDS
	$(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) M=$(@D) modules
endef

define ST7703_GX040HD_INSTALL_TARGET_CMDS
	$(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) M=$(@D) \
		INSTALL_MOD_PATH=$(TARGET_DIR) modules_install
endef

# Build and install device tree overlays
# -i $(@D): both panel overlays /include/ the shared st7703-gx040hd-panel.dtsi
define ST7703_GX040HD_BUILD_DT_OVERLAYS
	$(LINUX_DIR)/scripts/dtc/dtc -@ -i $(@D) -I dts -O dtb \
		-o $(@D)/st7703-gx040hd.dtbo \
		$(@D)/st7703-gx040hd-overlay.dts
	$(LINUX_DIR)/scripts/dtc/dtc -@ -i $(@D) -I dts -O dtb \
		-o $(@D)/st7703-gx040hd-i2c.dtbo \
		$(@D)/st7703-gx040hd-i2c-overlay.dts
	$(LINUX_DIR)/scripts/dtc/dtc -@ -i $(@D) -I dts -O dtb \
		-o $(@D)/ft6336u-gx040hd.dtbo \
		$(@D)/ft6336u-gx040hd-overlay.dts
endef

define ST7703_GX040HD_INSTALL_DT_OVERLAYS
	$(INSTALL) -D -m 0644 $(@D)/st7703-gx040hd.dtbo \
		$(BINARIES_DIR)/overlays/st7703-gx040hd.dtbo
	$(INSTALL) -D -m 0644 $(@D)/st7703-gx040hd-i2c.dtbo \
		$(BINARIES_DIR)/overlays/st7703-gx040hd-i2c.dtbo
	$(INSTALL) -D -m 0644 $(@D)/ft6336u-gx040hd.dtbo \
		$(BINARIES_DIR)/overlays/ft6336u-gx040hd.dtbo
endef

# Backlight sysfs permissions for the video group (desktop/kiosk brightness)
define ST7703_GX040HD_INSTALL_UDEV_RULE
	$(INSTALL) -D -m 0644 $(@D)/99-gx040hd-backlight.rules \
		$(TARGET_DIR)/etc/udev/rules.d/99-gx040hd-backlight.rules
endef
ST7703_GX040HD_POST_INSTALL_TARGET_HOOKS += ST7703_GX040HD_INSTALL_UDEV_RULE

ST7703_GX040HD_POST_BUILD_HOOKS += ST7703_GX040HD_BUILD_DT_OVERLAYS
ST7703_GX040HD_POST_INSTALL_TARGET_HOOKS += ST7703_GX040HD_INSTALL_DT_OVERLAYS

$(eval $(generic-package))
