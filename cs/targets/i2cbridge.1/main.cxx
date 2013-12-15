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

// DEFINE_PIPE(gc_can_pipe, 1);

Executor g_executor;

DEFINE_PIPE(can_pipe, &g_executor, sizeof(struct can_frame));

static const NMRAnet::NodeID NODE_ID = 0x050101011433ULL;

extern "C" {
const size_t WRITE_FLOW_THREAD_STACK_SIZE = 900;
extern const size_t CAN_TX_BUFFER_SIZE;
extern const size_t CAN_RX_BUFFER_SIZE;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 2;
const size_t main_stack_size = 900;
}

NMRAnet::AsyncIfCan g_if_can(&g_executor, &can_pipe, 3, 3, 2);
NMRAnet::DefaultAsyncNode g_node(&g_if_can, NODE_ID);
NMRAnet::GlobalEventFlow g_event_flow(&g_executor, 4);

static const uint64_t EVENT_ID = 0x0501010114FF4100ULL;
const int main_priority = 0;

extern "C" {

#ifdef __FreeRTOS__ //TARGET_LPC11Cxx
#endif
  
}

Executor* DefaultWriteFlowExecutor() {
    return &g_executor;
}

extern "C" { void resetblink(uint32_t pattern); }

class LoggingBit : public NMRAnet::BitEventInterface
{
public:
    LoggingBit(uint64_t event_on, uint64_t event_off, const char* name)
        : BitEventInterface(event_on, event_off), name_(name), state_(false)
    {
    }

    virtual bool GetCurrentState()
    {
        return state_;
    }
    virtual void SetState(bool new_value)
    {
        state_ = new_value;
        //HASSERT(0);
#ifdef __linux__
        LOG(INFO, "bit %s set to %d", name_, state_);
#else
        resetblink(state_ ? 1 : 0);
#endif
    }

    virtual NMRAnet::AsyncNode* node()
    {
        return &g_node;
    }

private:
    const char* name_;
    bool state_;
};


class I2CCheckedInUpdater : public I2CInUpdater {
 public:
  I2CCheckedInUpdater(I2C* port, int address,
                      std::initializer_list<uint8_t> preamble,
                      uint8_t* data_offset, int data_length, int listener_offset)
      : I2CInUpdater(port, address, preamble, data_offset, data_length, listener_offset) {}

 protected:
  virtual void OnFailure() {
    I2CInUpdater::OnFailure();
    resetblink(0x80000A02);
  }

  virtual void OnSuccess() {
    I2CInUpdater::OnSuccess();
    resetblink(1);
  }
};

I2C i2c(P0_5, P0_4);
FlowUpdater updater(&g_executor, {});

// TODO(bracz): this is horribly complicated. Simplify these classes.
class I2CIOBoard {
 public:
  I2CIOBoard(uint8_t address)
      : out_extender_(&i2c, address, {}, (uint8_t*)&state_, 2),
        in_extender_(&i2c, address, {}, ((uint8_t*)&state_) + 2, 1, 2),
        pc_(&g_node, BRACZ_LAYOUT | (address << 8), &state_, 24),
        listener_(&pc_)
  {
    updater.queue()->AddRepeatingEntry(&out_extender_);
    updater.queue()->AddRepeatingEntry(&in_extender_);
    in_extender_.SetListener(&listener_);
  }

 private:
  uint32_t state_;
  I2COutUpdater out_extender_;
  I2CInUpdater in_extender_;
  NMRAnet::BitRangeEventPC pc_;
  ListenerToEventProxy listener_;
};


I2CIOBoard brd_27(0x27);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
#ifdef TARGET_LPC11Cxx
    lpc11cxx::CreateCanDriver(&can_pipe);
#endif
    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    NMRAnet::BitEventConsumer consumer(&logger);
    g_if_can.AddWriteFlows(1, 1);
    g_if_can.set_alias_allocator(
        new NMRAnet::AsyncAliasAllocator(NODE_ID, &g_if_can));
    NMRAnet::AliasInfo info;
    g_if_can.alias_allocator()->empty_aliases()->Release(&info);
    NMRAnet::AddEventHandlerToIf(&g_if_can);
    g_executor.ThreadBody();
    return 0;
}
