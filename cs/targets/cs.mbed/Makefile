include hardware.mk

ifeq ($(MISSING_DEPS),)

.PHONY: rr-crossing.cout

rr-crossing.cout:
	$(MAKE) -C $(APP_PATH)/../automata $@
	ln -sf $(APP_PATH)/../automata/$@ $@

rr-crossing.o: %.o: %.cout
	$(CC) $(CFLAGS) -x c $< -o $@
	$(CC) -MM $(CFLAGS) -x c $< > $*.d

OBJS += rr-crossing.o

$(EXECUTABLE)$(EXTENTION): rr-crossing.o

all: rr-crossing.cout

endif
