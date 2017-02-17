#ifndef _BRACZ_CS_TIVASMALL_HARDWARE_HXX_
#define _BRACZ_CS_TIVASMALL_HARDWARE_HXX_

#include "TivaGPIO.hxx"
#include "DummyGPIO.hxx"


#define LED_RED GPIO_PORTF_BASE, GPIO_PIN_1
#define LED_GREEN GPIO_PORTF_BASE, GPIO_PIN_3
#define LED_BLUE GPIO_PORTF_BASE, GPIO_PIN_2

//#define USE_WII_CHUCK

GPIO_PIN(SW1, GpioInputPU, F, 4);
GPIO_PIN(LED_3, LedPin, F, 3);


GPIO_HWPIN(BOOSTER_H, GpioHwPin, B, 6, T0CCP0, Timer);
GPIO_HWPIN(BOOSTER_L, GpioHwPin, B, 7, T0CCP1, Timer);

GPIO_PIN(RAILCOM_TRIGGER, GpioOutputSafeHigh, D, 6);

GPIO_HWPIN(RAILCOM_CH1, GpioHwPin, B, 0, U1RX, UART);



DECL_PIN(PIN_H_GPIO, B, 6);


struct Debug {
  // High between start_cutout and end_cutout from the TivaRailcom driver.
  typedef DummyPin RailcomDriverCutout;
  // Flips every time an uart byte is received error.
  typedef DummyPin RailcomError;
  // Flips every time an 'E0' byte is received in the railcom driver.
  typedef DummyPin RailcomE0;
  typedef DummyPin RailcomDataReceived;
  typedef DummyPin RailcomAnyData;
  typedef DummyPin RailcomCh2Data;
  typedef DummyPin RailcomPackets;
  typedef DummyPin RailcomRxActivate;
};
namespace TDebug {
    typedef DummyPin Resync;
    typedef DummyPin NextPacket;
};

#endif // _BRACZ_CS_TIVASMALL_HARDWARE_HXX_
