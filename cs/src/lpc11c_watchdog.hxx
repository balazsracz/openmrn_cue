#ifndef BRACZ_TRAIN_LPC11C_WATCHDOG
#define BRACZ_TRAIN_LPC11C_WATCHDOG
#ifdef TARGET_LPC11Cxx

#include "openlcb/EventHandlerTemplates.hxx"
#include "LPC11xx.h"

static const uint64_t WATCHDOG_EVENT_ID = 0x0501010114FF0010ULL;

extern "C" {
void WDT_IRQHandler(void) { NVIC_SystemReset(); }
}

#undef OVERRIDE
#define OVERRIDE override

class WatchDogEventHandler : public openlcb::SimpleEventHandler {
 public:
  WatchDogEventHandler(openlcb::Node* node, uint64_t event)
      : node_(node), event_(event) {
    init_watchdog();
    reset_watchdog();
    openlcb::EventRegistry::instance()->register_handlerr(this, 0, 0);
  }

  void HandleEventReport(openlcb::EventReport* event,
                         BarrierNotifiable* done) OVERRIDE {
    if (event->event == event_) {
      reset_watchdog();
    }
    done->notify();
  }

  void handle_identify_global(openlcb::EventReport* event,
                            BarrierNotifiable* done) OVERRIDE {
    openlcb::event_write_helper1.WriteAsync(
        node_, openlcb::Defs::MTI_CONSUMER_IDENTIFIED_UNKNOWN,
        openlcb::WriteHelper::global(), openlcb::eventid_to_buffer(event_), done);
  }
  void handle_identify_consumer(openlcb::EventReport* event,
                              BarrierNotifiable* done) OVERRIDE {
    if (event->event == event_) {
      handle_identify_global(event, done);
    } else {
      done->notify();
    }
  }

 private:
  void init_watchdog() {
    // Turns on clock to wachdog timer.
    LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 15);

    // Watchdog clock source = main clock.
    LPC_SYSCON->WDTCLKSEL = 1;
    LPC_SYSCON->WDTCLKUEN = 0;
    LPC_SYSCON->WDTCLKUEN = 1;

    // Divide wdt clk by 48 (1 MHz)
    LPC_SYSCON->WDTCLKDIV = 48;

    // Timeout
    LPC_WDT->TC = 8 * 1000000 / 4;  // 8 seconds

    // WDT on, cause an interrupt. In effect when the first wdt event shows up.
    LPC_WDT->MOD = 1;
    // Enables the watchdog interrupt at priority zero.
    NVIC_SetPriority(WDT_IRQn, 0);
    NVIC_EnableIRQ(WDT_IRQn);
  }

  void reset_watchdog() {
    LPC_WDT->FEED = 0xAA;
    LPC_WDT->FEED = 0x55;
  }

  openlcb::Node* node_;
  uint64_t event_;
};

#endif  // target=lpc11c
#endif  // BRACZ_TRAIN_LPC11C_WATCHDOG
