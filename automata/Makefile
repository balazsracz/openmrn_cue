CXXFLAGS += -std=c++0x -MMD -MF $@.d -g -ggdb -m32 -Wall

OBJS=system.o control-logic.o

AUTS = rr-crossing.cpp
OUTPUTS = $(AUTS:.cpp=.cout)
BINS = $(AUTS:.cpp=)

all: lib test-aut bracz-layout3h-logic

-include *.d

lib: libautomata.a

dumpcallback: gen_callback
	./gen_callback

callback-specializations.hxx: gen_callback
	./gen_callback > callback-specializations.hxx

gencb: callback-specializations.hxx


libautomata.a: $(OBJS)
	$(AR) cr $@ $+

test-aut: $(OBJS) test-aut.o
	$(CXX) $(CXXFLAGS) -o $@ test-aut.o $(OBJS) 

bracz-layout3h-logic: $(OBJS) bracz-layout3h-logic.o # control-logic.hxx
	$(CXX) $(CXXFLAGS) -o $@ bracz-layout3h-logic.o $(OBJS) 

clean:
	rm -f *.o *.cout $(BINS) test-aut *.d *.bin

flash-test: test-aut
	./test-aut
	aut-flash.sh *bin

flash: bracz-layout3h-logic
	./bracz-layout3h-logic
	aut-flash.sh *bin

$(OUTPUTS): %.cout : %
	./$< > $@


$(BINS): $(OBJS)
