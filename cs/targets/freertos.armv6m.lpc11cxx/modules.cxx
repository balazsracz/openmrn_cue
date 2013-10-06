
#include "mbed.h"
#include "utils/pipe.hxx"
#include "can.h"
#include "FreeRTOSConfig.h"
#include "src/mbed_i2c_update.hxx"
#include "src/base.h"
#include "src/automata_control.h"

extern const unsigned long long NODE_ADDRESS;
const unsigned long long NODE_ADDRESS = 0x050101011432ULL;


extern "C" void resetblink(uint32_t pattern);


/*
DEFINE_PIPE(can_pipe0, sizeof(struct can_frame));
VIRTUAL_DEVTAB_ENTRY(canp0v0, can_pipe0, "/dev/canp0v0", 16);
VIRTUAL_DEVTAB_ENTRY(canp0v1, can_pipe0, "/dev/canp0v1", 16);
*/

class I2CCheckedInUpdater : public I2CInUpdater {
 public:
  I2CCheckedInUpdater(I2C* port, int address,
                      std::initializer_list<uint8_t> preamble,
                      uint8_t* data_offset, int data_length)
      : I2CInUpdater(port, address, preamble, data_offset, data_length) {}

 protected:
  virtual void OnFailure() {
    I2CInUpdater::OnFailure();
    resetblink(0x80000A02);
  }

  virtual void OnSuccess() {
    I2CInUpdater::OnFailure();
    resetblink(1);
  }
};

I2C i2c(P0_5, P0_4);

I2COutUpdater extender0(&i2c, 0x25, {}, get_state_byte(1, OFS_IOA), 2);
I2COutUpdater extender1(&i2c, 0x26, {}, get_state_byte(2, OFS_IOA), 2);

I2CInUpdater in_extender0(&i2c, 0x25, {}, get_state_byte(1, OFS_IOB), 1);
I2CInUpdater in_extender1(&i2c, 0x26, {}, get_state_byte(2, OFS_IOB), 1);

SynchronousUpdater i2c_updater({&extender0, &extender1, &in_extender0, &in_extender1});

extern "C" {
void modules_init() {}
}
