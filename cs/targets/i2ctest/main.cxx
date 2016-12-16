#include <stdio.h>
#include <unistd.h>

#include "mbed.h"
#include "os/os.h"
#include "utils/pipe.hxx"
#include "executor/executor.hxx"
#include "can_frame.h"
#include "nmranet_config.h"

#include "openlcb/IfCan.hxx"
#include "openlcb/NMRAnetIf.hxx"
#include "openlcb/AliasAllocator.hxx"
#include "openlcb/EventService.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/NMRAnetAsyncEventHandler.hxx"
#include "openlcb/DefaultNode.hxx"
#include "freertos_drivers/nxp/11cxx_async_can.hxx"

#include "src/updater.hxx"
#include "src/updater.hxx"
#include "src/mbed_i2c_update.hxx"
#include "src/event_range_listener.hxx"
#include "src/i2c_driver.hxx"

Executor g_executor;
extern Executor* DefaultWriteFlowExecutor();

Executor* DefaultWriteFlowExecutor() {
    return &g_executor;
}

extern "C" {
const size_t WRITE_FLOW_THREAD_STACK_SIZE = 900;
extern const size_t CAN_TX_BUFFER_SIZE;
extern const size_t CAN_RX_BUFFER_SIZE;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 2;
const size_t main_stack_size = 900;
}

const int main_priority = 0;

extern "C" { void resetblink(uint32_t pattern); }

I2CDriver g_i2c_driver;

class TestBlinker : public ControlFlow {
 public:
  TestBlinker() : ControlFlow(&g_executor, nullptr) {
    StartFlowAt(ST(GetHardware));
  }

  ControlFlowAction GetHardware() {
    return Allocate(g_i2c_driver.allocator(), ST(StartReceive));
  }

  ControlFlowAction StartReceive() {
    g_i2c_driver.StartRead(0x24, 0, 1, this);
    return WaitAndCall(ST(ReadDone));
  }

  ControlFlowAction ReadDone() {
    if (g_i2c_driver.success()) {
      data_read_ = g_i2c_driver.read_buffer()[0];
      return YieldAndCall(ST(StartSend));
    } else {
      data_read_ = 0;
      g_i2c_driver.Release();
      return YieldAndCall(ST(GetHardware));
    }
  }

  ControlFlowAction StartSend() {
    static int count = 0;
    uint8_t* buf = g_i2c_driver.write_buffer();
    buf[0] = (count++ >> 9) & 1;
    resetblink(buf[0] & 1);
    if (data_read_ & 4) buf[0] |= 2;
    buf[1] = 0;
    g_i2c_driver.StartWrite(0x24, 2, this);
    return WaitAndCall(ST(WriteDone));
  }

  ControlFlowAction WriteDone() {
    /*if (g_i2c_driver.success()) {
      resetblink(0);
    } else {
      resetblink(1);
      }*/
    g_i2c_driver.Release();
    return Sleep(&sleep_, MSEC_TO_NSEC(1), ST(GetHardware));
  }

 private:
  uint8_t data_read_;
  SleepData sleep_;
};

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
  TestBlinker i2c_blinker;
  g_executor.ThreadBody();
  return 0;
}
