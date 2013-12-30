include $(APP_PATH)/config.mk
SUBDIRS = cs.panda cs.mbed i2cbridge.1 linux.x86 i2ctest

ifdef PROD
SUBDIRS += prod.i2cbridge.27
SUBDIRS += prod.i2cbridge.25
endif

# freertos.armv6m.lpc11cxx freertos.armv7m.mbed-lpc1768 freertos.mips4k.pic32mx.duinomitemega linux.x86
include $(OPENMRNPATH)/etc/recurse.mk
