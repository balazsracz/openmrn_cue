ifndef APP_PATH
APP_PATH := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
endif
export APP_PATH
-include $(APP_PATH)/openmrnpath.mk
ifndef OPENMRNPATH
OPENMRNPATH := $(shell \
sh -c "if [ \"X`printenv OPENMRNPATH`\" != \"X\" ]; then printenv OPENMRNPATH; \
     elif [ -d /opt/openmrn/src ]; then echo /opt/openmrn; \
     elif [ -d ~/openmrn/src ]; then echo ~/openmrn; \
     elif [ -d ~/train/openlcb/openmrn/src ]; then echo ~/train/openlcb/openmrn; \
     elif [ -d ../../../src ]; then echo ../../..; \
     else echo OPENMRNPATH not found; fi" \
)
endif

ifeq ($(OPENMRNPATH),OPENMRNPATH not found)
$(error Could not find OPENMRN)
endif

export OPENMRNPATH:=$(abspath $(OPENMRNPATH))
