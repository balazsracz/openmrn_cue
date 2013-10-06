APP_PATH ?= $(realpath ../..)
include $(APP_PATH)/config.mk

TARGET := freertos.mips4k.pic32mx
export TARGET

include $(OPENMRNPATH)/etc/prog.mk

ifeq ($(MISSING_DEPS),)

all: $(EXECUTABLE).hex $(EXECUTABLE).lst 

LDFLAGSEXTRA = -Wl,--wrap=__cxa_pure_virtual  -Wl,--wrap=__cxa_atexit

OBJEXTRA = $(OPENMRNPATH)/targets/freertos.mips4k.pic32mx/freertos_drivers/pic32mx_can.o \


%.lst: %.elf
	$(SIZE) $<
	$(OBJDUMP) -d $< > $@

%.delf: %.elf
	$(OBJCOPY) $$($(OBJDUMP) -h $< | grep ' b[df]' | cut  -b 5- | sed -e 's/^\([^ ]*\) .*$$/--change-section-lma \1-0xA0000000/' | tr '\n' ' ') $< $@
#	$(OBJDUMP) -h $< | grep ' b[df]'
#	cp -f $< $@

%.hex: %.delf
	$(OBJDUMP) -h $<
	$(OBJCOPY) -O ihex $< $@


cg.svg: cg.dot
	dot -Tsvg < $< >$@

cg.dot: $(EXECUTABLE).lst $(EXECUTABLE)$(EXTENTION) $(OPENMRNPATH)/bin/callgraph.py
	$(OPENMRNPATH)/bin/callgraph.py --map $(EXECUTABLE).map < $(EXECUTABLE).lst > $@


$(EXECUTABLE)$(EXTENTION): depmake

.PHONY: depmake

#depmake:
#	+$(MAKE) -C $(OPENMRNPATH)/targets/$(TARGET) all

endif  #MISSING_DEPS
