/** \copyright
 * Copyright (c) 2013, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file main.cxx
 *
 * A simple application to demonstrate asynchronous interfaces.
 *
 * @author Balazs Racz
 * @date 7 Dec 2013
 */

#include <stdio.h>
#include <unistd.h>

#include "mbed.h"

#include "os/os.h"
#include "utils/pipe.hxx"
#include "executor/executor.hxx"
#include "nmranet_can.h"
#include "nmranet_config.h"
#include "os/watchdog.h"

#include "nmranet/AsyncIfCan.hxx"
#include "nmranet/NMRAnetIf.hxx"
#include "nmranet/AsyncAliasAllocator.hxx"
#include "nmranet/GlobalEventHandler.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/NMRAnetAsyncEventHandler.hxx"
#include "nmranet/NMRAnetAsyncDefaultNode.hxx"

#include "src/event_range_listener.hxx"
#include "src/i2c_extender_flow.hxx"
#include "src/mbed_gpio_handlers.hxx"


// DEFINE_PIPE(gc_can_pipe, 1);

Executor g_executor;

DEFINE_PIPE(can_pipe0, &g_executor, sizeof(struct can_frame));
//DEFINE_PIPE(can_pipe1, &g_executor, sizeof(struct can_frame));

static const NMRAnet::NodeID NODE_ID = 0x050101011441ULL;

extern "C" {
const size_t WRITE_FLOW_THREAD_STACK_SIZE = 900;
extern const size_t CAN_TX_BUFFER_SIZE;
extern const size_t CAN_RX_BUFFER_SIZE;
const size_t CAN_RX_BUFFER_SIZE = 8;
const size_t CAN_TX_BUFFER_SIZE = 8;
const size_t main_stack_size = 1500;
  // This is only needed for usbserial. We should actually try not to link it
  // in.
extern const size_t SERIAL_RX_BUFFER_SIZE;
extern const size_t SERIAL_TX_BUFFER_SIZE;
const size_t SERIAL_RX_BUFFER_SIZE = 16;
const size_t SERIAL_TX_BUFFER_SIZE = 16;
}

/*VIRTUAL_DEVTAB_ENTRY(canp0v0, can_pipe0, "/dev/canp0v0", 16);
VIRTUAL_DEVTAB_ENTRY(canp0v1, can_pipe0, "/dev/canp0v1", 16);

VIRTUAL_DEVTAB_ENTRY(canp1v0, can_pipe1, "/dev/canp1v0", 16);
VIRTUAL_DEVTAB_ENTRY(canp1v1, can_pipe1, "/dev/canp1v1", 16);*/

//I2C i2c(P0_10, P0_11); for panda CS

NMRAnet::AsyncIfCan g_if_can(&g_executor, &can_pipe0, 3, 3, 2);
NMRAnet::DefaultAsyncNode g_node(&g_if_can, NODE_ID);
NMRAnet::GlobalEventFlow g_event_flow(&g_executor, 6);

static const uint64_t EVENT_ID = 0x0501010114FF203CULL;
const int main_priority = 0;
extern const int WAIT_FOR_START_PIN;
const int WAIT_FOR_START_PIN = 0;

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

I2CDriver g_i2c_driver;
I2cExtenderBoard brd_21(0x21, &g_executor, &g_node);
I2cExtenderBoard brd_23(0x23, &g_executor, &g_node);
I2cExtenderBoard brd_27(0x27, &g_executor, &g_node);


MbedGPIOListener led_1(BRACZ_LAYOUT | 0x2030,
                       BRACZ_LAYOUT | 0x2031,
                       P0_23);
MbedGPIOListener led_2(BRACZ_LAYOUT | 0x2032,
                       BRACZ_LAYOUT | 0x2033,
                       P0_24);
MbedGPIOListener led_3(BRACZ_LAYOUT | 0x2034,
                       BRACZ_LAYOUT | 0x2035,
                       P0_25);
MbedGPIOListener led_4(BRACZ_LAYOUT | 0x2036,
                       BRACZ_LAYOUT | 0x2037,
                       P0_26);
MbedGPIOListener led_5(BRACZ_LAYOUT | 0x2038,
                       BRACZ_LAYOUT | 0x2039,
                       P1_30);
MbedGPIOListener led_6(BRACZ_LAYOUT | 0x203a,
                       BRACZ_LAYOUT | 0x203b,
                       P1_31);


/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
    start_watchdog(5000);
    add_watchdog_reset_timer(500);
    can_pipe0.AddPhysicalDeviceToPipe("/dev/can0", "can0_rx_thread", 512);

    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    NMRAnet::BitEventConsumer consumer(&logger);
    //BlinkerFlow blinker(&g_node);
    g_if_can.AddWriteFlows(1, 1);
    g_if_can.set_alias_allocator(
        new NMRAnet::AsyncAliasAllocator(NODE_ID, &g_if_can));
    NMRAnet::AliasInfo info;
    g_if_can.alias_allocator()->empty_aliases()->Release(&info);
    NMRAnet::AddEventHandlerToIf(&g_if_can);
    g_executor.ThreadBody();
    return 0;
}
