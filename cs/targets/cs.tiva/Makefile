export BOARD := BOARD_BRACZ_CS_TIVA

include hardware.mk

ifeq ($(MISSING_DEPS),)

#AUTOMATA=convention-logic
AUTOMATA=bracz-layout3h-logic

.PHONY: $(AUTOMATA).cout

$(AUTOMATA).cout:
	$(MAKE) -C $(APP_PATH)/../automata $@
	ln -sf $(APP_PATH)/../automata/$@ $@

$(AUTOMATA).o: %.o: %.cout
	$(CC) $(CFLAGS) -x c $< -o $@
	$(CC) -MM $(CFLAGS) -x c $< > $*.d

OBJS += $(AUTOMATA).o
ifdef HWVER
CXXFLAGS+= -D$(HWVER)
endif

$(EXECUTABLE)$(EXTENTION): $(AUTOMATA).o

all: $(AUTOMATA).cout

endif
