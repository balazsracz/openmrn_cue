export BOARD := BOARD_BRACZ_RAILCOM
APP_PATH ?= $(realpath ../..)
-include $(APP_PATH)/config.mk
-include $(OPENMRNPATH)/etc/config.mk
LINKCORELIBS = -lopenlcb -lutils

TARGET := bare.armv7m

include hardware.mk

ARCHOPTIMIZATION += -Os


SYSLIB_SUBDIRS=
OBJEXTRA=


payload.hxxout: bootloader/cs.bin Makefile
	echo -n "const " > $@
	xxd -i -n header_arr -l 128 $< >> $@
	echo -n "const " >> $@
	xxd -i -n data_arr -s 0x1e000 $< >> $@

NodeId.o: payload.hxxout
