export BOARD := BOARD_BRACZ_CS_TIVA_SMALL

include hardware.mk

ifeq ($(MISSING_DEPS),)

AUTOMATA=convention-logic

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

SYSLIBRARIES += -Wl,--wrap=malloc \


endif
