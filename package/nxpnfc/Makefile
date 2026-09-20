# Out-of-tree build of the NXP PN7160 NFC I2C kernel driver
# common.h guards I2C members with IS_ENABLED(CONFIG_NXP_NFC_I2C)
ccflags-y := -DCONFIG_NXP_NFC_I2C_MODULE
obj-m := nxpnfc_i2c.o
nxpnfc_i2c-objs := common.o i2c_drv.o
