#ifndef _BRACZ_CS_TIVASMALL_HARDWARE_HXX_
#define _BRACZ_CS_TIVASMALL_HARDWARE_HXX_

#include "TivaGPIO.hxx"
#include "DummyGPIO.hxx"


#define LED_RED GPIO_PORTF_BASE, GPIO_PIN_1
#define LED_GREEN GPIO_PORTF_BASE, GPIO_PIN_3
#define LED_BLUE GPIO_PORTF_BASE, GPIO_PIN_2

//#define USE_WII_CHUCK

GPIO_PIN(SW1, GpioInputPU, F, 4);

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
};
namespace TDebug {
    typedef DummyPin Resync;
    typedef DummyPin NextPacket;
};

#endif // _BRACZ_CS_TIVASMALL_HARDWARE_HXX_
