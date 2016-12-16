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

#include "src/event_range_listener.hxx"
#define EXT_SIGNAL_COUNT 4
#include "src/i2c_extender_flow.hxx"

// DEFINE_PIPE(gc_can_pipe, 1);

Executor g_executor;

DEFINE_PIPE(can_pipe, &g_executor, sizeof(struct can_frame));

static const openlcb::NodeID NODE_ID = 0x050101011433ULL;

extern "C" {
const size_t WRITE_FLOW_THREAD_STACK_SIZE = 900;
extern const size_t CAN_TX_BUFFER_SIZE;
extern const size_t CAN_RX_BUFFER_SIZE;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 2;
const size_t main_stack_size = 900;
}

openlcb::AsyncIfCan g_if_can(&g_executor, &can_pipe, 3, 3, 2, 1, 2);
openlcb::DefaultNode g_node(&g_if_can, NODE_ID);
openlcb::EventIteratorFlow g_event_flow(&g_executor, 5);

static const uint64_t EVENT_ID = 0x0501010114FF2400ULL;
const int main_priority = 0;

extern "C" {

#ifdef __FreeRTOS__ //TARGET_LPC11Cxx
#endif
  
}

Executor* DefaultWriteFlowExecutor() {
    return &g_executor;
}

extern "C" { void resetblink(uint32_t pattern); }

class LoggingBit : public openlcb::BitEventInterface
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

    virtual openlcb::Node* node()
    {
        return &g_node;
    }

private:
    const char* name_;
    bool state_;
};


class BlinkerFlow : public ControlFlow
{
public:
    BlinkerFlow(openlcb::Node* node)
        : ControlFlow(node->interface()->dispatcher()->executor(), nullptr),
          state_(1),
          bit_(node, EVENT_ID, EVENT_ID + 1, &state_, (uint8_t)1),
          producer_(&bit_)
    {
        StartFlowAt(ST(handle_sleep));
    }

private:
    ControlFlowAction blinker()
    {
        state_ = !state_;
#ifdef __linux__
        LOG(INFO, "blink produce %d", state_);
#endif
        producer_.Update(&helper_, this);
        return WaitAndCall(ST(handle_sleep));
    }

    ControlFlowAction handle_sleep()
    {
        return Sleep(&sleepData_, MSEC_TO_NSEC(1000), ST(blinker));
    }

    uint8_t state_;
    openlcb::MemoryBit<uint8_t> bit_;
    openlcb::BitEventProducer producer_;
    openlcb::WriteHelper helper_;
    SleepData sleepData_;
};

I2CDriver g_i2c_driver;
I2cExtenderBoard brd_22(&g_i2c_driver, 0x22, &g_executor, &g_node);
//I2cExtenderBoard brd_25(&g_i2c_driver, 0x25, &g_executor, &g_node);
//I2cExtenderBoard brd_27(&g_i2c_driver, 0x27, &g_executor, &g_node);

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
  //BlinkerFlow blinker(&g_node);
    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    openlcb::BitEventConsumer consumer(&logger);
    g_if_can.set_alias_allocator(
        new openlcb::AsyncAliasAllocator(NODE_ID, &g_if_can));
    openlcb::AliasInfo info;
    g_if_can.alias_allocator()->empty_aliases()->Release(&info);
    openlcb::AddEventHandlerToIf(&g_if_can);
    g_executor.ThreadBody();
    return 0;
}
