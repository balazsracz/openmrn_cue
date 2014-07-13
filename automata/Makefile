CXXFLAGS += -std=c++0x -MMD -MF $@.d -g -ggdb -m32 -Wall

OBJS=system.o control-logic.o registry.o

AUTS = rr-crossing.cpp logic-demo.cpp
OUTPUTS = $(AUTS:.cpp=.cout)
BINS = $(AUTS:.cpp=)

all: lib test-aut bracz-layout3h-logic bin

-include *.d

lib: libautomata.a

dumpcallback: gen_callback
	./gen_callback

callback-specializations.hxx: gen_callback
	./gen_callback > callback-specializations.hxx

gencb: callback-specializations.hxx

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $(abspath $<)

.cxx.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $(abspath $<)

.cc.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $(abspath $<)


libautomata.a: $(OBJS)
	$(AR) cr $@ $+

test-aut: $(OBJS) test-aut.o
	$(CXX) $(CXXFLAGS) -o $@ test-aut.o $(OBJS) 

bracz-layout3h-logic: $(OBJS) bracz-layout3h-logic.o # control-logic.hxx
	$(CXX) $(CXXFLAGS) -o $@ bracz-layout3h-logic.o $(OBJS) 

convention-logic: convention-logic.o $(OBJS)  # control-logic.hxx
	$(CXX) $(CXXFLAGS) -o $@ $+

clean:
	rm -f *.o *.cout $(BINS) test-aut *.d *.bin

flash-test: test-aut
	./test-aut
	aut-flash.sh *bin

.PHONY: flash
flash: bracz-layout3h-logic
	./bracz-layout3h-logic
	aut-flash.sh *bin

.PHONY: bin
#bin: bracz-layout3h-logic
#	./bracz-layout3h-logic

bin: convention-logic
	./convention-logic


$(OUTPUTS): %.cout : %
	./$< > $@


$(BINS): $(OBJS)
