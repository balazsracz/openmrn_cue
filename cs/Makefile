APP_PATH := $(realpath .)
export APP_PATH

include $(APP_PATH)/config.mk

SUBDIRS = targets

include $(OPENMRNPATH)/etc/recurse.mk

.PHONY: docs
docs:
	$(MAKE) -C doc docs || exit 1;


tests:
	$(MAKE) -C targets/test tests
