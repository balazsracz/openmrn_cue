APP_PATH ?= $(realpath ../..)
include $(APP_PATH)/config.mk

export TARGET := linux.x86
export EXECUTABLE := cue
include $(OPENMRNPATH)/etc/prog.mk

#AUTOMATA=convention-logic
#AUTOMATA=bracz-layout3h-logic
AUTOMATA=lcc-layout-logic
#AUTOMATA=stbaker-logic
AUTOMATA=lcc-layout-logic-soft

.PHONY: $(AUTOMATA).cout

$(AUTOMATA).cout:
	$(MAKE) -C $(APP_PATH)/../automata $@
	ln -sf $(APP_PATH)/../automata/$@ $@

$(AUTOMATA).o: %.o: %.cout
	$(CC) $(CFLAGS) -x c $< -o $@
	$(CC) -MM $(CFLAGS) -x c $< > $*.d

OBJS += $(AUTOMATA).o

$(EXECUTABLE)$(EXTENTION): $(AUTOMATA).o

all: $(AUTOMATA).cout
