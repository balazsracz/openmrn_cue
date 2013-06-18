APP_PATH := $(realpath .)
export APP_PATH

include $(APP_PATH)/config.mk

SUBDIRS = targets

include $(OPENMRNPATH)/etc/recurse.mk

.PHONY: docs
docs:
	make -C doc docs || exit 1;

