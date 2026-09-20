################################################################################
#
# libnfc-nci
#
################################################################################

# NCI2.0-R1.1: NXP NFC stack for PN7160 (NCI 2.0). For PN7150/PN7120 use R2.4.
LIBNFC_NCI_VERSION = NCI2.0-R1.1
LIBNFC_NCI_SITE = $(call github,NXPNFCLinux,linux_libnfc-nci,$(LIBNFC_NCI_VERSION))
LIBNFC_NCI_LICENSE = Apache-2.0
LIBNFC_NCI_LICENSE_FILES = LICENSE.txt
LIBNFC_NCI_INSTALL_STAGING = YES
LIBNFC_NCI_DEPENDENCIES = openssl host-automake host-autoconf host-libtool nxpnfc

# Fix for GCC 10+ where -fno-common is default (causes multiple definition errors)
# Fix for GCC 14+ where many previously-warning C patterns are now hard errors:
#   -Wimplicit-function-declaration, -Wimplicit-int, -Wint-conversion,
#   -Wincompatible-pointer-types, -Wreturn-mismatch (GCC 14 split from -Wreturn-type)
# This old NFC library has many of these issues; use -fpermissive-equivalent flags
# -include cstdint: GCC 13+ libstdc++ no longer transitively includes <cstdint>,
#   the NXP code relies on uint8_t etc. being available without explicit include
LIBNFC_NCI_CONF_ENV = \
	CFLAGS="$(TARGET_CFLAGS) -fcommon -Wno-error=implicit-function-declaration -Wno-error=implicit-int -Wno-error=int-conversion -Wno-error=incompatible-pointer-types -Wno-error=return-mismatch -Wno-error=return-type" \
	CXXFLAGS="$(TARGET_CXXFLAGS) -fcommon -include cstdint"

# Use kernel driver mode - /dev/pn544 interface created by pn5xx-i2c kernel module
# Do NOT use --enable-alt (that's for userspace GPIO which doesn't work on CM4)
LIBNFC_NCI_CONF_OPTS =

define LIBNFC_NCI_RUN_BOOTSTRAP
	cd $(@D) && ./bootstrap
endef

LIBNFC_NCI_PRE_CONFIGURE_HOOKS += LIBNFC_NCI_RUN_BOOTSTRAP

# Tune libnfc-nci.conf for the PN7160 reader-only terminal:
# - NFA_PROPRIETARY_CFG: NXP protocol/discovery mappings (upstream only ships
#   them in libnfc-nxp.conf which the NFA layer does not read on Linux);
#   without it the NFA uses AOSP defaults that the PN7160 rejects with
#   SYNTAX_ERROR on RF_DISCOVER_CMD.
# - Passive-only polling (A|B|F|V), no P2P/host listen (NCI2.0 dropped the
#   legacy active modes contained in the 0xCF default).
define LIBNFC_NCI_TUNE_NCI_CONF
	$(SED) 's/^POLLING_TECH_MASK=.*/POLLING_TECH_MASK=0x0F/' \
		$(TARGET_DIR)/etc/libnfc-nci.conf
	$(SED) 's/^P2P_LISTEN_TECH_MASK=.*/P2P_LISTEN_TECH_MASK=0x00/' \
		$(TARGET_DIR)/etc/libnfc-nci.conf
	$(SED) 's/^HOST_LISTEN_TECH_MASK=.*/HOST_LISTEN_TECH_MASK=0x00/' \
		$(TARGET_DIR)/etc/libnfc-nci.conf
	grep -q '^NFA_PROPRIETARY_CFG' $(TARGET_DIR)/etc/libnfc-nci.conf || \
		printf '\nNFA_PROPRIETARY_CFG={05:FF:FF:06:81:80:70:FF:FF}\n' \
			>> $(TARGET_DIR)/etc/libnfc-nci.conf
endef
LIBNFC_NCI_POST_INSTALL_TARGET_HOOKS += LIBNFC_NCI_TUNE_NCI_CONF

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
