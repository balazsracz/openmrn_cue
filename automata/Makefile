CXXFLAGS+= -std=c++0x -MMD -MF $@.d -g -ggdb

OBJS=system.o

all: test-aut

include *.d

test-aut: $(OBJS)

clean:
	rm *.o 

