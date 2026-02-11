################################################################################
#
# libnfc-nci
#
################################################################################

LIBNFC_NCI_VERSION = R2.4
LIBNFC_NCI_SITE = $(call github,dotstartech,linux_libnfc-nci,$(LIBNFC_NCI_VERSION))
LIBNFC_NCI_LICENSE = Apache-2.0
LIBNFC_NCI_LICENSE_FILES = LICENSE.txt
LIBNFC_NCI_INSTALL_STAGING = YES
LIBNFC_NCI_DEPENDENCIES = openssl host-automake host-autoconf host-libtool pn5xx-i2c

# Fix for GCC 10+ where -fno-common is default (causes multiple definition errors)
LIBNFC_NCI_CONF_ENV = CFLAGS="$(TARGET_CFLAGS) -fcommon" CXXFLAGS="$(TARGET_CXXFLAGS) -fcommon"

# Use kernel driver mode - /dev/pn544 interface created by pn5xx-i2c kernel module
# Do NOT use --enable-alt (that's for userspace GPIO which doesn't work on CM4)
LIBNFC_NCI_CONF_OPTS =

define LIBNFC_NCI_RUN_BOOTSTRAP
	cd $(@D) && ./bootstrap
endef

LIBNFC_NCI_PRE_CONFIGURE_HOOKS += LIBNFC_NCI_RUN_BOOTSTRAP

ifeq ($(BR2_PACKAGE_LIBNFC_NCI_DEMOAPP),y)
define LIBNFC_NCI_INSTALL_DEMOAPP
	$(INSTALL) -D -m 0755 $(TARGET_DIR)/usr/sbin/nfcDemoApp \
		$(TARGET_DIR)/usr/bin/nfcDemoApp
endef
LIBNFC_NCI_POST_INSTALL_TARGET_HOOKS += LIBNFC_NCI_INSTALL_DEMOAPP
endif

ifeq ($(BR2_PACKAGE_LIBNFC_NCI_DEMOAPP_AUTOSTART),y)
define LIBNFC_NCI_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 $(LIBNFC_NCI_PKGDIR)/S95nfc \
		$(TARGET_DIR)/etc/init.d/S95nfc
endef
endif

$(eval $(autotools-package))
