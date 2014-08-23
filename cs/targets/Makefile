include $(APP_PATH)/config.mk
SUBDIRS = cs.panda cs.mbed cs.tiva linux.x86 # i2cbridge.1  i2ctest i2cbridge.panda nativeio11c24.1

ifdef PROD
SUBDIRS += prod.i2cbridge.27
SUBDIRS += prod.i2cbridge.25
endif

# freertos.armv6m.lpc11cxx freertos.armv7m.mbed-lpc1768 freertos.mips4k.pic32mx.duinomitemega linux.x86
include $(OPENMRNPATH)/etc/recurse.mk
