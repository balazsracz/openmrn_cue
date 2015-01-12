#ifndef _ACC_TIVA_3_HARDWARE_HXX_
#define _ACC_TIVA_3_HARDWARE_HXX_

#include "TivaGPIO.hxx"

#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/timer.h"

GPIO_PIN(LED_RED, LedPin, D, 6);
GPIO_PIN(LED_GREEN, LedPin, D, 5);
GPIO_PIN(LED_BLUE, LedPin, G, 1);
GPIO_PIN(LED_YELLOW, LedPin, B, 0);

GPIO_PIN(LED_BLUE_SW, LedPin, B, 6);
GPIO_PIN(LED_GOLD_SW, LedPin, B, 7);

GPIO_PIN(DBG_SIGNAL, GpioOutputSafeLow, B, 1);

typedef LED_RED_Pin BlinkerLed;

struct Debug {
  // One for the duration of the first byte successfully read from the uart
  // device for railcom until the next 1 msec when the receiver gets disabled.
  typedef DummyPin RailcomUartReceived;
  // Measures the time from the end of a DCC packet until the entry of the
  // user-space state flow.
  typedef DummyPin DccPacketDelay;

  // High between start_cutout and end_cutout from the TivaRailcom driver.
  typedef DBG_SIGNAL_Pin RailcomDriverCutout;
  //typedef DummyPin/*LED_GREEN_Pin*/ RailcomDriverCutout;

  // Flips every timer capture interrupt from the dcc deocder flow.
  typedef LED_BLUE_SW_Pin DccDecodeInterrupts;
  //typedef DummyPin DccDecodeInterrupts;
 
  // Flips every timer capture interrupt from the dcc deocder flow.
  //typedef DBG_SIGNAL_Pin RailcomE0;
  typedef LED_GREEN_Pin RailcomE0;

  // Flips every timer capture interrupt from the dcc deocder flow.
  typedef DummyPin RailcomError;
  //typedef LED_BLUE_SW_Pin RailcomError;


};

struct DCCDecode
{
    static const auto TIMER_BASE = WTIMER4_BASE;
    static const auto TIMER_PERIPH = SYSCTL_PERIPH_WTIMER4;
    static const auto TIMER_INTERRUPT = INT_WTIMER4A;
    static const auto TIMER = TIMER_A;
    static const auto CFG_CAP_TIME_UP =
        TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_TIME_UP | TIMER_CFG_B_ONE_SHOT;
    // Interrupt bits.
    static const auto TIMER_CAP_EVENT = TIMER_CAPA_EVENT;
    static const auto TIMER_TIM_TIMEOUT = TIMER_TIMA_TIMEOUT;

    static const auto OS_INTERRUPT = INT_WTIMER4B;
    DECL_HWPIN(NRZPIN, D, 4, WT4CCP0);

    static const uint32_t TIMER_MAX_VALUE = 0x8000000UL;
    static const uint32_t PS_MAX = 0;

    static const int Q_SIZE = 32;
};

#endif // _ACC_TIVA_3_HARDWARE_HXX_
