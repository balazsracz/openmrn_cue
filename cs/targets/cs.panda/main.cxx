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

#include "src/cs_config.h"

#include "os/os.h"
#include "utils/Hub.hxx"
#include "utils/HubDeviceNonBlock.hxx"
#include "executor/Executor.hxx"
#include "can_frame.h"
#include "nmranet_config.h"
#include "os/watchdog.h"

#include "nmranet/IfCan.hxx"
#include "nmranet/If.hxx"
#include "nmranet/AliasAllocator.hxx"
#include "nmranet/EventService.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/DefaultNode.hxx"
#include "freertos_drivers/nxp/11cxx_async_can.hxx"

// for logging implementation
#include "src/host_packet.h"
#include "src/usb_proto.h"
#include "src/can-queue.h"

#include "src/mbed_i2c_update.hxx"
#include "src/automata_runner.h"
#include "src/automata_control.h"

#include "custom/TrackInterface.hxx"
#include "commandstation/UpdateProcessor.hxx"
#include "nmranet/TractionTrain.hxx"

#include "custom/HostPacketCanPort.hxx"
#include "mobilestation/MobileStationSlave.hxx"
#include "mobilestation/TrainDb.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "mobilestation/AllTrainNodes.hxx"


// DEFINE_PIPE(gc_can_pipe, 1);
NO_THREAD nt;
Executor<5> g_executor(nt);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);
CanHubFlow can_hub1(&g_service);

static const nmranet::NodeID NODE_ID = 0x050101011431ULL;

extern "C" {
extern insn_t automata_code[];

  /// @TODO(balazs.racz) these are probably outdated. need to use the constants
  /// library.

const size_t WRITE_FLOW_THREAD_STACK_SIZE = 900;
extern const size_t CAN_TX_BUFFER_SIZE;
extern const size_t CAN_RX_BUFFER_SIZE;
const size_t CAN_RX_BUFFER_SIZE = 32;
const size_t CAN_TX_BUFFER_SIZE = 8;
  extern const size_t SERIAL_RX_BUFFER_SIZE;
  extern const size_t SERIAL_TX_BUFFER_SIZE;
const size_t SERIAL_RX_BUFFER_SIZE = 16;
const size_t SERIAL_TX_BUFFER_SIZE = 16;
const size_t main_stack_size = 1500;
}

void log_output(char* buf, int size) {
    if (size <= 0) return;
    PacketBase pkt(size + 1);
    pkt[0] = CMD_VCOM1;
    memcpy(pkt.buf() + 1, buf, size);
    PacketQueue::instance()->TransmitPacket(pkt);
}

/*VIRTUAL_DEVTAB_ENTRY(canp0v0, can_pipe0, "/dev/canp0v0", 16);
VIRTUAL_DEVTAB_ENTRY(canp0v1, can_pipe0, "/dev/canp0v1", 16);

VIRTUAL_DEVTAB_ENTRY(canp1v0, can_pipe1, "/dev/canp1v0", 16);
VIRTUAL_DEVTAB_ENTRY(canp1v1, can_pipe1, "/dev/canp1v1", 16);*/

//I2C i2c(P0_10, P0_11); for panda CS

nmranet::IfCan g_if_can(&g_executor, &can_hub0, 30, 10, 30);
static nmranet::AddAliasAllocator _alias_allocator(NODE_ID, &g_if_can);
nmranet::DefaultNode g_node(&g_if_can, NODE_ID);
nmranet::EventService g_event_service(&g_if_can);

static const uint64_t EVENT_ID = 0x0501010114FF203AULL;
const int main_priority = 2;
OVERRIDE_CONST(main_thread_priority, 2);
OVERRIDE_CONST(main_stack_size, 1500);

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
        //HASSERT(0);
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

#ifdef STATEFLOW_CS
// Command station objects.
CanIf can1_interface(&g_service, &can_hub1);
bracz_custom::TrackIfSend track_send(&can_hub1);
commandstation::UpdateProcessor cs_loop(&g_service, &track_send);
bracz_custom::TrackIfReceive track_recv(&can1_interface, &cs_loop);
static const uint64_t ON_EVENT_ID = 0x0501010114FF0004ULL;
bracz_custom::TrackPowerOnOffBit on_off(ON_EVENT_ID, ON_EVENT_ID+1, &track_send);
nmranet::BitEventConsumer powerbit(&on_off);
nmranet::TrainService traction_service(&g_if_can);
/*dcc::Dcc28Train train_Am843(dcc::DccShortAddress(43));
nmranet::TrainNode train_Am843_node(&traction_service, &train_Am843);
dcc::Dcc28Train train_ICN(dcc::DccShortAddress(50));
nmranet::TrainNode train_ICN_node(&traction_service, &train_ICN);
dcc::Dcc28Train train_M61(dcc::DccShortAddress(61));
nmranet::TrainNode train_M61_node(&traction_service, &train_M61);
//dcc::Dcc28Train train_Re460TSR(dcc::DccShortAddress(22));
dcc::MMOldTrain train_Re460TSR(dcc::MMAddress(22));
nmranet::TrainNode train_Re460TSR_node(&traction_service, &train_Re460TSR);
dcc::Dcc28Train train_V60(dcc::DccShortAddress(51));
nmranet::TrainNode train_V60_node(&traction_service, &train_V60);
dcc::MMOldTrain train_Re465(dcc::MMAddress(47));
nmranet::TrainNode train_Re465_node(&traction_service, &train_Re465);*/


OVERRIDE_CONST(automata_init_backoff, 10000);

mobilestation::MobileStationSlave mosta_slave(&g_executor, &can1_interface);
mobilestation::TrainDb train_db;
mobilestation::AllTrainNodes all_trains(&train_db, &traction_service);
mobilestation::MobileStationTraction mosta_traction(&can1_interface, &g_if_can, &train_db, &g_node);
#endif

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
  //start_watchdog(5000);
  // add_watchdog_reset_timer(500);
    PacketQueue::initialize("/dev/serUSB0");

    HubDeviceNonBlock<CanHubFlow> can0_port(&can_hub0, "/dev/can0");

#ifndef STATEFLOW_CS
    dcc_can_init(fd);
#else
    HubDeviceNonBlock<CanHubFlow> can1_port(&can_hub1, "/dev/can1");
    bracz_custom::init_host_packet_can_bridge(&can_hub1);
#endif

    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    nmranet::BitEventConsumer consumer(&logger);
    //BlinkerFlow blinker(&g_node);
    g_if_can.add_addressed_message_support();
    // Bootstraps the alias allocation process.
    g_if_can.alias_allocator()->send(g_if_can.alias_allocator()->alloc());

    AutomataRunner runner(&g_node, automata_code);
    resume_all_automata();

    g_executor.thread_body();
    return 0;
}
