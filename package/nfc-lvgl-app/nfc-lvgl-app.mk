################################################################################
#
# nfc-lvgl-app
#
################################################################################

NFC_LVGL_APP_VERSION = 1.0.0
NFC_LVGL_APP_SITE_METHOD = local
NFC_LVGL_APP_SITE = $(BR2_EXTERNAL_NFC_TERMINAL_PATH)/package/nfc-lvgl-app/src
NFC_LVGL_APP_LICENSE = MIT
NFC_LVGL_APP_DEPENDENCIES = libnfc-nci libdrm paho-mqtt-c

# LVGL version to download
NFC_LVGL_APP_LVGL_VERSION = v9.2.2
NFC_LVGL_APP_LVGL_TARBALL = lvgl-$(NFC_LVGL_APP_LVGL_VERSION).tar.gz
NFC_LVGL_APP_LVGL_URL = https://github.com/lvgl/lvgl/archive/refs/tags/$(NFC_LVGL_APP_LVGL_VERSION).tar.gz

define NFC_LVGL_APP_DOWNLOAD_LVGL
	@echo "Downloading LVGL $(NFC_LVGL_APP_LVGL_VERSION)..."
	if [ ! -f $(DL_DIR)/$(NFC_LVGL_APP_LVGL_TARBALL) ]; then \
		wget -O $(DL_DIR)/$(NFC_LVGL_APP_LVGL_TARBALL) $(NFC_LVGL_APP_LVGL_URL); \
	fi
	mkdir -p $(@D)/lvgl
	tar -xzf $(DL_DIR)/$(NFC_LVGL_APP_LVGL_TARBALL) -C $(@D)/lvgl --strip-components=1
endef

NFC_LVGL_APP_PRE_BUILD_HOOKS += NFC_LVGL_APP_DOWNLOAD_LVGL

define NFC_LVGL_APP_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/include -I$(STAGING_DIR)/usr/include/libdrm" \
		LDFLAGS="$(TARGET_LDFLAGS) -L$(STAGING_DIR)/usr/lib" \
		STAGING_DIR="$(STAGING_DIR)"
endef

define NFC_LVGL_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/nfc-lvgl-app $(TARGET_DIR)/usr/bin/nfc-lvgl-app
endef

ifeq ($(BR2_PACKAGE_NFC_LVGL_APP_AUTOSTART),y)
define NFC_LVGL_APP_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 $(NFC_LVGL_APP_PKGDIR)/nfc-console \
		$(TARGET_DIR)/usr/bin/nfc-console
endef
endif

$(eval $(generic-package))
