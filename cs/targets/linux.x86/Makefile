APP_PATH ?= $(realpath ../..)
include $(APP_PATH)/config.mk

TEST_EXTRA_OBJS += error.o
TESTOBJSEXTRA += error.o 

include $(OPENMRNPATH)/etc/prog.mk

#SYSLIBRARIES += -lautomata

$(EXECUTABLE)$(EXTENTION): lib/libautomata.a logic-demo.o
#$(EXECUTABLE)$(EXTENTION): depmake

#lib/libautomata.a:

OBJS += logic-demo.o

logic-demo.o: logic-demo.cout
	$(CC) $(CFLAGS) -x c $< -o $@
	$(CC) -MM $(CFLAGS) -x c $< > $*.d

.PHONY: logic-demo.dep

logic-demo.dep:
	+$(MAKE) -C $(APP_PATH)/../automata logic-demo.cout

logic-demo.cout: $(APP_PATH)/../automata/logic-demo.cout logic-demo.dep
	ln -sf $< $@

#	
#	+$(MAKE) -C $(APP_PATH)/../automata logic-demo.cout
#	ln -sf $(APP_PATH)/../automata/logic-demo.cout $@

cleangen:
	rm -f logic-demo.cout

veryclean: cleangen
