CXXFLAGS += -std=c++0x -MMD -MF $@.d -g -ggdb -m32

OBJS=system.o

AUTS = rr-crossing.cpp
OUTPUTS = $(AUTS:.cpp=.cout)
BINS = $(AUTS:.cpp=)

all: lib test-aut

-include *.d

lib: libautomata.a


libautomata.a: $(OBJS)
	$(AR) cr $@ $+

test-aut: $(OBJS) test-aut.cpp control-logic.hxx

clean:
	rm -f *.o *.cout $(BINS) test-aut *.d *.bin

flash: test-aut
	./test-aut
	aut-flash.sh *bin

$(OUTPUTS): %.cout : %
	./$< > $@


$(BINS): $(OBJS)
