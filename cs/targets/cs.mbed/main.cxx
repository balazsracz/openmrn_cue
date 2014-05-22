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
#include "utils/Hub.hxx"
#include "utils/HubDevice.hxx"
#include "executor/Executor.hxx"
#include "nmranet_can.h"
#include "nmranet_config.h"
#include "os/watchdog.h"

#include "nmranet/IfCan.hxx"
#include "nmranet/NMRAnetIf.hxx"
#include "nmranet/AliasAllocator.hxx"
#include "nmranet/GlobalEventHandler.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/NMRAnetAsyncEventHandler.hxx"
#include "nmranet/NMRAnetAsyncDefaultNode.hxx"
#include "freertos_drivers/nxp/11cxx_async_can.hxx"

// for logging implementation
#include "src/host_packet.h"
#include "src/usb_proto.h"
#include "src/can-queue.h"

#include "src/mbed_i2c_update.hxx"
#include "src/automata_runner.h"
#include "src/automata_control.h"

NO_THREAD nt;
Executor<1> g_executor(nt);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);
CanHubFlow can_hub1(&g_service);

static const nmranet::NodeID NODE_ID = 0x050101011434ULL;

extern "C" {
extern insn_t automata_code[];

const size_t WRITE_FLOW_THREAD_STACK_SIZE = 900;
extern const size_t CAN_TX_BUFFER_SIZE;
extern const size_t CAN_RX_BUFFER_SIZE;
const size_t CAN_RX_BUFFER_SIZE = 8;
const size_t CAN_TX_BUFFER_SIZE = 8;
  extern const size_t SERIAL_RX_BUFFER_SIZE;
  extern const size_t SERIAL_TX_BUFFER_SIZE;
const size_t SERIAL_RX_BUFFER_SIZE = 16;
const size_t SERIAL_TX_BUFFER_SIZE = 16;
const size_t main_stack_size = 900;
}

void log_output(char* buf, int size) {
    if (size <= 0) return;
    PacketBase pkt(size + 1);
    pkt[0] = CMD_VCOM1;
    memcpy(pkt.buf() + 1, buf, size);
    PacketQueue::instance()->TransmitPacket(pkt);
}


nmranet::AsyncIfCan g_if_can(&g_executor, &can_hub0, 3, 3, 2);
static nmranet::AddAliasAllocator _alias_allocator(NODE_ID, &g_if_can);
nmranet::DefaultNode g_node(&g_if_can, NODE_ID);
nmranet::GlobalEventService g_event_service(&g_if_can);

static const uint64_t EVENT_ID = 0x0501010114FF203AULL;
const int main_priority = 0;

extern "C" { void resetblink(uint32_t pattern); }

class LoggingBit : public nmranet::BitEventInterface
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
#ifdef __linux__
        LOG(INFO, "bit %s set to %d", name_, state_);
#else
        resetblink(state_ ? 1 : 0);
#endif
    }

    virtual nmranet::Node* node()
    {
        return &g_node;
    }

private:
    const char* name_;
    bool state_;
};

DigitalIn startpin(p20);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
    start_watchdog(5000);
    add_watchdog_reset_timer(500);
    PacketQueue::initialize("/dev/serUSB0");
    int can_fd = open("/dev/can1", O_RDWR);
    FdHubPort<CanHubFlow> can_hub_port(&can_hub0, can_fd, EmptyNotifiable::DefaultInstance());
    //can_pipe1.AddPhysicalDeviceToPipe(, "can1_rx_thread", 512);

    /*int fd = open("/dev/can0", O_RDWR);
    ASSERT(fd >= 0);
    dcc_can_init(fd);*/

    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    nmranet::BitEventConsumer consumer(&logger);
    g_if_can.add_addressed_message_support();
    g_if_can.set_alias_allocator(
        new nmranet::AsyncAliasAllocator(NODE_ID, &g_if_can));
    // Bootstraps the alias allocation process.
    g_if_can.alias_allocator()->send(g_if_can.alias_allocator()->alloc());

    //AutomataRunner runner(&g_node, automata_code);
    //resume_all_automata();

    g_executor.thread_body();
    return 0;

    /*
    startpin.mode(PullUp);
    PacketQueue::initialize("/dev/serUSB0");
    can_pipe0.AddPhysicalDeviceToPipe("/dev/can1", "can0_rx_thread", 512);
    can_pipe1.AddPhysicalDeviceToPipe("/dev/can0", "can1_rx_thread", 512);
#ifdef TARGET_LPC11Cxx
    lpc11cxx::CreateCanDriver(&can_pipe);
#endif
    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    nmranet::BitEventConsumer consumer(&logger);
    BlinkerFlow blinker(&g_node);
    g_if_can.AddWriteFlows(1, 1);
    g_if_can.set_alias_allocator(
        new nmranet::AsyncAliasAllocator(NODE_ID, &g_if_can));
    nmranet::AliasInfo info;
    g_if_can.alias_allocator()->empty_aliases()->Release(&info);
    nmranet::AddEventHandlerToIf(&g_if_can);
    resetblink(0x80002A);
    while(startpin) {}
    resetblink(0);
    g_executor.ThreadBody();
    return 0;*/
}
