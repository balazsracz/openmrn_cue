#include <stdio.h>
#include <unistd.h>

#include "mbed.h"
#include "os/os.h"
#include "utils/Hub.hxx"
#include "utils/HubDevice.hxx"
#include "executor/Executor.hxx"
#include "can_frame.h"
#include "nmranet_config.h"

#include "nmranet/IfCan.hxx"
#include "nmranet/If.hxx"
#include "nmranet/AliasAllocator.hxx"
#include "nmranet/EventService.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/DefaultNode.hxx"
#include "freertos_drivers/nxp/11cxx_async_can.hxx"

#include "src/event_range_listener.hxx"
#define EXT_SIGNAL_COUNT 24
#include "src/i2c_extender_flow.hxx"
#include "src/lpc11c_watchdog.hxx"

// DEFINE_PIPE(gc_can_pipe, 1);

NO_THREAD nt;
Executor<1> g_executor(nt);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);

static const openlcb::NodeID NODE_ID = 0x050101011447ULL;

extern "C" {
const size_t WRITE_FLOW_THREAD_STACK_SIZE = 900;
extern const size_t CAN_TX_BUFFER_SIZE;
extern const size_t CAN_RX_BUFFER_SIZE;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 2;
const size_t main_stack_size = 900;
}

openlcb::IfCan g_if_can(&g_executor, &can_hub0, 1, 3, 2);
static openlcb::AddAliasAllocator _alias_allocator(NODE_ID, &g_if_can);
openlcb::DefaultNode g_node(&g_if_can, NODE_ID);
openlcb::EventService g_event_service(&g_if_can);
WatchDogEventHandler g_watchdog(&g_node, WATCHDOG_EVENT_ID);

static const uint64_t EVENT_ID = 0x0501010114FF2400ULL;
const int main_priority = 0;

extern "C" {

#ifdef __FreeRTOS__ //TARGET_LPC11Cxx
#endif
  
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

I2CDriver g_i2c_driver(&g_service);
I2cExtenderBoard brd_22(&g_i2c_driver, 0x22, &g_node);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
#ifdef TARGET_LPC11Cxx
  lpc11cxx::CreateCanDriver(&can_hub0);
#endif
  //BlinkerFlow blinker(&g_node);
    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    openlcb::BitEventConsumer consumer(&logger);
    openlcb::AliasInfo info;
    g_if_can.alias_allocator()->send(g_if_can.alias_allocator()->alloc());
    g_executor.thread_body();
    return 0;
}
