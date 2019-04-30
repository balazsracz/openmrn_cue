
all: automata-lib

tests: automata-lib

.PHONY: automata-lib

automata-lib:
	$(MAKE) -C ../../../../automata
	ln -sf $(realpath ../../../../automata/libautomata.a) ../lib/
	if [ ../lib/libautomata.a -nt ../lib/timestamp ] ; then touch ../lib/timestamp ; fi

clean veryclean mksubdirs:
