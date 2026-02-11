################################################################################
#
# pn5xx-i2c
#
################################################################################

# Use local pre-patched source (kernel 6.x compatible)
PN5XX_I2C_VERSION = 1.0
PN5XX_I2C_SITE = $(BR2_EXTERNAL_NFC_TERMINAL_PATH)/package/pn5xx-i2c
PN5XX_I2C_SITE_METHOD = local
PN5XX_I2C_LICENSE = GPL-2.0

PN5XX_I2C_DEPENDENCIES = linux

# Build kernel module and device tree overlay
define PN5XX_I2C_BUILD_CMDS
	$(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) M=$(@D) modules
	$(LINUX_DIR)/scripts/dtc/dtc -@ -I dts -O dtb \
		-o $(@D)/nfc-pn7150.dtbo \
		$(BR2_EXTERNAL_NFC_TERMINAL_PATH)/board/nfc-terminal/overlays/nfc-pn7150.dts
endef

define PN5XX_I2C_INSTALL_TARGET_CMDS
	$(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) M=$(@D) \
		INSTALL_MOD_PATH=$(TARGET_DIR) modules_install
endef

# Install the overlay to boot partition
define PN5XX_I2C_INSTALL_DT_OVERLAY
	$(INSTALL) -D -m 0644 $(@D)/nfc-pn7150.dtbo \
		$(BINARIES_DIR)/overlays/nfc-pn7150.dtbo
endef
PN5XX_I2C_POST_INSTALL_TARGET_HOOKS += PN5XX_I2C_INSTALL_DT_OVERLAY

$(eval $(generic-package))
