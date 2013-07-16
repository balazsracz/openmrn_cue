CXXFLAGS+= -std=c++0x -MMD -MF $@.d -g -ggdb -m32

OBJS=system.o

all: lib test-aut

-include *.d

lib: libautomata.a


libautomata.a: $(OBJS)
	$(AR) cr $@ $+

test-aut: $(OBJS)

clean:
	rm *.o 

flash: test-aut
	./test-aut
	aut-flash.sh *bin
