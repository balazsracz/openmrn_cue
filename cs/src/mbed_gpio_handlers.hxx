#ifndef _BRACZ_TRAIN_MBED_GPIO_HANDLERS_HXX_
#define _BRACZ_TRAIN_MBED_GPIO_HANDLERS_HXX_

#include <stdint.h>
#include <string.h>

#include "updater.hxx"
#include "common_event_handlers.hxx"

#include "mbed.h"

class MbedGPIOListener : public GPIOOutToggleEventHandler {
 public:
  MbedGPIOListener(uint64_t event_on, uint64_t event_off,
                   PinName pin)
      : GPIOOutToggleEventHandler(event_on, event_off,
                                  gpio_set(pin),
                                  NULL, NULL) {
    gpio_t gpio;
    gpio_init(&gpio, pin, PIN_OUTPUT);
    memory_off_ = gpio.reg_clr;
    memory_ = gpio.reg_set;
  }
};

class MbedGPIOProducer : public MemoryBitProducer<volatile uint32_t> {
 public:
  MbedGPIOProducer(node_t node, uint64_t event_on, uint64_t event_off,
                   PinName pin, PinMode mode)
      : MemoryBitProducer<volatile uint32_t>(node, event_on, event_off,
                                             gpio_set(pin), NULL) {
    gpio_t gpio;
    gpio_init(&gpio, pin, PIN_INPUT);
    gpio_mode(&gpio, mode);
    memory_ = gpio.reg_in;
  }
};

#endif // _BRACZ_TRAIN_MBED_GPIO_HANDLERS_HXX_
