################################################################################
#
# nxpnfc
#
################################################################################

# NXP PN7160 official kernel driver (NXPNFCLinux/nxpnfc), vendored locally
# with kernel 6.x API fixes. Creates /dev/nxpnfc used by libnfc-nci NCI2.0.
NXPNFC_VERSION = 1.0
NXPNFC_SITE = $(BR2_EXTERNAL_NFC_TERMINAL_PATH)/package/nxpnfc
NXPNFC_SITE_METHOD = local
NXPNFC_LICENSE = GPL-2.0

NXPNFC_DEPENDENCIES = linux

# Build kernel module and device tree overlay
define NXPNFC_BUILD_CMDS
	$(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) M=$(@D) modules
	$(LINUX_DIR)/scripts/dtc/dtc -@ -I dts -O dtb \
		-o $(@D)/nfc-pn7160.dtbo \
		$(BR2_EXTERNAL_NFC_TERMINAL_PATH)/board/nfc-terminal/overlays/nfc-pn7160.dts
endef

define NXPNFC_INSTALL_TARGET_CMDS
	$(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) M=$(@D) \
		INSTALL_MOD_PATH=$(TARGET_DIR) modules_install
endef

# Install the overlay to boot partition
define NXPNFC_INSTALL_DT_OVERLAY
	$(INSTALL) -D -m 0644 $(@D)/nfc-pn7160.dtbo \
		$(BINARIES_DIR)/overlays/nfc-pn7160.dtbo
endef
NXPNFC_POST_INSTALL_TARGET_HOOKS += NXPNFC_INSTALL_DT_OVERLAY

$(eval $(generic-package))
