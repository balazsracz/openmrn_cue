include $(APP_PATH)/config.mk
SUBDIRS = \
	acc.tiva.2 \
	acc.tiva.3 \
	androidproxy \
	bootloadertest.tiva \
	cs.mbed \
	cs.panda  \
	cs.tiva \
	cs.tivasmall \
	cue.linux \
	cue.mbed \
	cue.tiva \
	dcctest.129 \
	host \
	linux.x86 \
	marklinproxy.panda \
	memorize.linux.x86 \
	railcom \
	reflash_automata \
	signal_reflash \


#freertos.armv6m.lpc11cxx
#freertos.armv7m.mbed-lpc1768
#freertos.mips4k.pic32mx.duinomitemega
# i2cbridge.1
# i2cbridge.panda
# i2ctest
# nativeio11c24.1
# prod.i2cbridge.25
# prod.i2cbridge.27

ifdef PROD
SUBDIRS += prod.i2cbridge.27
SUBDIRS += prod.i2cbridge.25
endif

export HWVER=HW_V2
export ADDRESS=0x4D

# freertos.armv6m.lpc11cxx freertos.armv7m.mbed-lpc1768 freertos.mips4k.pic32mx.duinomitemega linux.x86
include $(OPENMRNPATH)/etc/recurse.mk
