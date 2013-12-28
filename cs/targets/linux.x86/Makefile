APP_PATH ?= $(realpath ../..)
include $(APP_PATH)/config.mk

TEST_EXTRA_OBJS += error.o

include $(OPENMRNPATH)/etc/prog.mk

SUBDIRS += automata
#SYSLIBRARIES += -lautomata

$(EXECUTABLE)$(EXTENTION): lib/libautomata.a depmake
#$(EXECUTABLE)$(EXTENTION): depmake

#lib/libautomata.a:

.PHONY: depmake

#depmake:
#	make -C $(OPENMRNPATH)/targets/$(TARGET) all

rr-crossing.c : rr-crossing.cout

.PHONY: rr-crossing.cout

rr-crossing.cout :
	+$(MAKE) -C $(APP_PATH)/../automata rr-crossing.cout
	cp -f $(APP_PATH)/../automata/rr-crossing.cout $@

cleangen:
	rm -f rr-crossing.cout

veryclean clean: cleangen
