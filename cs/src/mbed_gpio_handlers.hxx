#ifndef _BRACZ_TRAIN_MBED_GPIO_HANDLERS_HXX_
#define _BRACZ_TRAIN_MBED_GPIO_HANDLERS_HXX_

#include <stdint.h>
#include <string.h>

//#include "updater.hxx"
//#include "common_event_handlers.hxx"

#include "mbed.h"

#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/NMRAnetAsyncNode.hxx"

namespace NMRAnet {
class AsyncNode;
}
extern NMRAnet::DefaultAsyncNode g_node;

class MbedGPIOListener : public NMRAnet::BitEventInterface, public NMRAnet::BitEventConsumer {
 public:
  MbedGPIOListener(uint64_t event_on, uint64_t event_off,
                   PinName pin)
      : BitEventInterface(event_on, event_off),
        BitEventConsumer(this) {
    mask_ = gpio_set(pin);
    gpio_t gpio;
    gpio_init(&gpio, pin, PIN_OUTPUT);
    memory_off_ = gpio.reg_clr;
    memory_on_ = gpio.reg_set;
    memory_read_ = gpio.reg_in;
  }

  virtual bool GetCurrentState() {
    return (*memory_read_) & mask_;
  }
  virtual void SetState(bool new_value) {
    if (new_value) *memory_on_ = mask_; else *memory_off_ = mask_;
  }

  virtual NMRAnet::AsyncNode* node() {
    return &g_node;
  }

 private:
  volatile uint32_t* memory_on_;
  volatile uint32_t* memory_off_;
  volatile uint32_t* memory_read_;
  uint32_t mask_;
};


/*class MbedGPIOListener : public GPIOOutToggleEventHandler {
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
  };*/

#endif // _BRACZ_TRAIN_MBED_GPIO_HANDLERS_HXX_
