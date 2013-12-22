#include <stdio.h>
#include <unistd.h>

#include "mbed.h"
#include "os/os.h"
#include "utils/pipe.hxx"
#include "executor/executor.hxx"
#include "nmranet_can.h"
#include "nmranet_config.h"

#include "nmranet/AsyncIfCan.hxx"
#include "nmranet/NMRAnetIf.hxx"
#include "nmranet/AsyncAliasAllocator.hxx"
#include "nmranet/GlobalEventHandler.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/NMRAnetAsyncEventHandler.hxx"
#include "nmranet/NMRAnetAsyncDefaultNode.hxx"
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
    return Allocate(g_i2c_driver.allocator(), ST(SendMessage));
  }

  ControlFlowAction SendMessage() {
    static int count = 0;
    uint8_t* buf = g_i2c_driver.write_buffer();
    buf[0] = (count++ >> 9) & 1 ? 1 : 2;
    buf[1] = 0;
    g_i2c_driver.StartWrite(0x24, 2, this);
    return WaitAndCall(ST(WriteDone));
  }

  ControlFlowAction WriteDone() {
    if (g_i2c_driver.success()) {
      resetblink(0);
    } else {
      resetblink(1);
    }
    g_i2c_driver.Release();
    return Sleep(&sleep_, MSEC_TO_NSEC(1), ST(GetHardware));
  }

 private:
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
