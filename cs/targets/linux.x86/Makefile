APP_PATH ?= $(realpath ../..)
include $(APP_PATH)/config.mk

TEST_EXTRA_OBJS += error.o

include $(OPENMRNPATH)/etc/prog.mk

#SYSLIBRARIES += -lautomata

$(EXECUTABLE)$(EXTENTION): lib/libautomata.a logic-demo.o depmake
#$(EXECUTABLE)$(EXTENTION): depmake

#lib/libautomata.a:

.PHONY: depmake

#depmake:
#	make -C $(OPENMRNPATH)/targets/$(TARGET) all

OBJS += logic-demo.o

logic-demo.o: logic-demo.cout
	$(CC) $(CFLAGS) -x c $< -o $@
	$(CC) -MM $(CFLAGS) -x c $< > $*.d

.PHONY: logic-demo.cout

logic-demo.cout:
	+$(MAKE) -C $(APP_PATH)/../automata logic-demo.cout
	ln -sf $(APP_PATH)/../automata/logic-demo.cout $@

cleangen:
	rm -f logic-demo.c

veryclean: cleangen
