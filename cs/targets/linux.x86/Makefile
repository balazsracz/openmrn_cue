APP_PATH ?= $(realpath ../..)
include $(APP_PATH)/config.mk

TEST_EXTRA_OBJS += error.o

include $(OPENMRNPATH)/etc/prog.mk

SYSLIBRARIES += -lautomata

$(EXECUTABLE)$(EXTENTION): lib/libautomata.a

lib/libautomata.a:
	$(MAKE) -C ../../../automata
	ln -sf $(realpath ../../../automata/libautomata.a) lib/

