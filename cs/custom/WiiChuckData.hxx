#ifndef _BRACZ_CUSTOM_WIICHUCKDATA_HXX_
#define _BRACZ_CUSTOM_WIICHUCKDATA_HXX_

#include "executor/StateFlow.hxx"

namespace bracz_custom {

struct WiiChuckData {
  uint8_t data[6];

  uint8_t joy_x() { return data[0]; }
  uint8_t joy_y() { return data[1]; }
  uint16_t accel_x() { return (data[2] << 2) | ((data[5] >> 2) & 3); }
  uint16_t accel_y() { return (data[3] << 2) | ((data[5] >> 4) & 3); }
  uint16_t accel_z() { return (data[4] << 2) | ((data[5] >> 6) & 3); }
  bool button_c() { return data[5] & 2; }
  bool button_z() { return data[5] & 1; }
};

typedef FlowInterface<Buffer<WiiChuckData> > WiiChuckConsumerFlowInterface;

} // namespace bracz_custom

#endif // _BRACZ_CUSTOM_WIICHUCKDATA_HXX_
