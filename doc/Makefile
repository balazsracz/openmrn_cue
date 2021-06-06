SRCS = $(wildcard *.dox) $(wildcard *.md) \
       Doxyfile

all:

.PHONY:
docs: html

html: $(SRCS)
	doxygen Doxyfile
	echo "warnings:"
	cat warnings

clean::
	rm -rf latex html warnings openmrn.html

veryclean:: clean

tests:

mksubdirs:
