TARGET_DIR_PATH = $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
DEFAULT_ADDRESS=0x71
export TARGET:=freertos.armv7m.exc
export BOARD:=BOARD_NUCLEO_IO
export EXECUTABLE:=logic
include hardware.mk

SYSLIBRARIESEXTRA += -lfreertos_drivers_spiffs_stm32f767xx
