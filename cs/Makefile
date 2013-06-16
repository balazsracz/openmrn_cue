APP_PATH := $(realpath .)
export APP_PATH

include $(APP_PATH)/config.mk

SUBDIRS = targets

include 

all:
	$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) all || exit 1;)

clean::
	$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) clean || exit 1;)

veryclean:: clean
	$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) veryclean || exit 1;)



.PHONY:
docs:
	make -C doc docs || exit 1;

