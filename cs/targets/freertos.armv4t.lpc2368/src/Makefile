OPENMRNPATH ?= $(shell \
sh -c "if [ \"X`printenv OPENMRNPATH`\" != \"X\" ]; then printenv OPENMRNPATH; \
     elif [ -d ~/openmrn/src ]; then echo ~/openmrn; \
     elif [ -d ~/train/openlcb/openmrn/src ]; then echo ~/train/openlcb/openmrn; \
     elif [ -d /opt/openmrn/src ]; then echo /opt/openmrn; \
     else echo OPENMRNPATH not found; fi" \
)

TARGET = freertos.armv4t

include $(OPENMRNPATH)/etc/applib.mk

