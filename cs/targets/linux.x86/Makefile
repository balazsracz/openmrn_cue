APP_PATH ?= $(realpath ../..)
include $(APP_PATH)/config.mk

TEST_EXTRA_OBJS += error.o

include $(OPENMRNPATH)/etc/prog.mk

SYSLIBRARIES += -lautomata

$(EXECUTABLE)$(EXTENTION): lib/libautomata.a depmake

lib/libautomata.a:
	$(MAKE) -C ../../../automata
	ln -sf $(realpath ../../../automata/libautomata.a) lib/

.PHONY: depmake

depmake:
	make -C $(OPENMRNPATH)/targets/$(TARGET) all

