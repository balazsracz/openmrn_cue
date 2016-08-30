
OBJS=system.o control-logic.o registry.o

AUTS = rr-crossing.cpp logic-demo.cpp 
OUTPUTS = $(AUTS:.cpp=.cout) convention-logic.cout stbaker-logic.cout lcc-layout-logic.cout lcc-layout-logic-soft.cout bracz-layout3h-logic.cout
BINS = $(AUTS:.cpp=)

DEFAULT_AUT = bracz-layout3h-logic  # lcc-layout-logic
TARGET_NODE = 0x050101011432 # host.tiva

all: lib test-aut bracz-layout3h-logic bin lcc-layout-logic lcc-layout-logic-soft

-include *.d

lib: libautomata.a

dumpcallback: gen_callback
	./gen_callback

callback-specializations.hxx: gen_callback
	./gen_callback > callback-specializations.hxx

gencb: callback-specializations.hxx

#CXXFLAGS := -std=gnu++0x
CXXFLAGS += -std=gnu++0x -MMD -MF $@.d -g -ggdb -Wall -isystem /opt/gmock/default/gtest/include

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

lcc-layout-logic: $(OBJS) lcc-layout-logic.o # control-logic.hxx
	$(CXX) $(CXXFLAGS) -o $@ lcc-layout-logic.o $(OBJS) 

lcc-layout-logic-soft: $(OBJS) lcc-layout-logic-soft.o # control-logic.hxx
	$(CXX) $(CXXFLAGS) -o $@ lcc-layout-logic-soft.o $(OBJS) 

convention-logic: convention-logic.o $(OBJS)  # control-logic.hxx
	$(CXX) $(CXXFLAGS) -o $@ $+

stbaker-logic: stbaker-logic.o $(OBJS)  # control-logic.hxx
	$(CXX) $(CXXFLAGS) -o $@ $+

clean:
	rm -f *.o *.cout $(BINS) test-aut *.d *.bin

flash-test: test-aut
	./test-aut
	aut-flash.sh *bin

.PHONY: flash
flash: bin
	train-reflash-automata -n $(TARGET_NODE)
#	aut-flash.sh *bin

.PHONY: bin
bin: $(DEFAULT_AUT)
	./$(DEFAULT_AUT)

#bin: convention-logic
#	./convention-logic


$(OUTPUTS): %.cout : %
	./$< > $@


$(BINS): $(OBJS)

.PHONY: panel
panel:
	./gen-panel-html.py ~/.jmri/3h-layout-082.xml panel.html

