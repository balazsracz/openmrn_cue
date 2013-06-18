export APP_PATH

OPENMRNPATH := $(shell \
sh -c "if [ \"X`printenv OPENMRNPATH`\" != \"X\" ]; then printenv OPENMRNPATH; \
     elif [ -d /opt/openmrn/src ]; then echo /opt/openmrn; \
     elif [ -d ~/openmrn/src ]; then echo ~/openmrn; \
     elif [ -d ~/train/openlcb/openmrn/src ]; then echo ~/train/openlcb/openmrn; \
     elif [ -d ../../../src ]; then echo ../../..; \
     else echo OPENMRNPATH not found; fi" \
)

export OPENMRNPATH

ifeq ($(OPENMRNPATH),OPENMRNPATH not found)
$(error Could not find OPENMRN)
endif
