include $(OPENMRNPATH)/etc/applib.mk

clean: clean-bison

Parser.o Scanner.o : INCLUDES+= -D_POSIX_C_SOURCE -DYY_BUF_SIZE=64

clean-bison:
	rm -f {Parser,Scanner}.cxxout {location,position,stack}.hh Parser.hxxout
