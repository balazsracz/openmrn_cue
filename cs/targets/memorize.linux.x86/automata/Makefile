
all: automata-lib

tests: automata-lib

.PHONY: automata-lib

automata-lib:
	$(MAKE) -C ../../../../automata
	ln -sf $(realpath ../../../../automata/libautomata.a) ../lib/

clean veryclean mksubdirs:
