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


#include "os/os.h"
#include "utils/Hub.hxx"
#include "utils/HubDevice.hxx"
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
#include "custom/HostPacketCanPort.hxx"
#include "custom/TrackInterface.hxx"
#include "mobilestation/MobileStationSlave.hxx"
#include "mobilestation/TrainDb.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "commandstation/UpdateProcessor.hxx"
#include "nmranet/TractionTrain.hxx"
#include "dcc/Loco.hxx"
#include "mobilestation/TrainDb.hxx"

// Used to talk to the booster.
//OVERRIDE_CONST(can2_bitrate, 250000);

// Forces all trains to be our responsibility.
OVERRIDE_CONST(mobile_station_train_count, 0);

OVERRIDE_CONST(automata_init_backoff, 20000);
OVERRIDE_CONST(node_init_identify, 0);

namespace mobilestation {
extern const struct const_loco_db_t const_lokdb[];

const struct const_loco_db_t const_lokdb[] = {
  // 0
  { 43, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, FNT11, ABV,  0xff, },
    "Am 843 093-6", DCC_28 },
  // 1
  { 22, { 0, 3, 4,  0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "RE 460 TSR", MARKLIN_NEW }, // todo: there is no beamer here
  // 2 (jim's)
  { 0x0761, { 0, 3, 0xff }, { LIGHT, WHISTLE, 0xff, }, "Jim's steam", OLCBUSER },
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
};
extern const size_t const_lokdb_size;
const size_t const_lokdb_size = sizeof(const_lokdb) / sizeof(const_lokdb[0]);
}  // namespace mobilestation

NO_THREAD nt;
Executor<1> g_executor(nt);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);
//CanHubFlow can_hub1(&g_service);

static const nmranet::NodeID NODE_ID = 0x050101011432ULL;

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


nmranet::IfCan g_if_can(&g_executor, &can_hub0, 3, 3, 3);
static nmranet::AddAliasAllocator _alias_allocator(NODE_ID, &g_if_can);
nmranet::DefaultNode g_node(&g_if_can, NODE_ID);
nmranet::EventService g_event_service(&g_if_can);

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

//CanIf can1_interface(&g_service, &can_hub1);

//bracz_custom::TrackIfSend track_send(&can_hub1);
//commandstation::UpdateProcessor cs_loop(&g_service, &track_send);
//bracz_custom::TrackIfReceive track_recv(&can1_interface, &cs_loop);
static const uint64_t ON_EVENT_ID = 0x0501010114FF0004ULL;
//bracz_custom::TrackPowerOnOffBit on_off(ON_EVENT_ID, ON_EVENT_ID+1, &track_send);
//nmranet::BitEventConsumer powerbit(&on_off);
nmranet::TrainService traction_service(&g_if_can);

dcc::Dcc28Train train_Am843(dcc::DccShortAddress(43));
nmranet::TrainNode train_Am843_node(&traction_service, &train_Am843);
dcc::MMNewTrain train_Re460(dcc::MMAddress(22));
nmranet::TrainNode train_Re460_node(&traction_service, &train_Re460);

//mobilestation::MobileStationSlave mosta_slave(&g_executor, &can1_interface);
mobilestation::TrainDb train_db;
//mobilestation::MobileStationTraction mosta_traction(&can1_interface, &g_if_can, &train_db, &g_node);


/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
    start_watchdog(5000);
    add_watchdog_reset_timer(500);
    //PacketQueue::initialize("/dev/serUSB0");
    HubDeviceNonBlock<CanHubFlow> can0_port(&can_hub0, "/dev/can0");
    //HubDeviceNonBlock<CanHubFlow> can1_port(&can_hub1, "/dev/can1");
    //bracz_custom::init_host_packet_can_bridge(&can_hub1);

    nmranet::Velocity v;
    v.set_mph(29);
    train_Re460.set_speed(v);
    train_Am843.set_speed(v);

    /*int fd = open("/dev/can0", O_RDWR);
    ASSERT(fd >= 0);
    dcc_can_init(fd);*/

    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    nmranet::BitEventConsumer consumer(&logger);
    g_if_can.add_addressed_message_support();
    g_if_can.set_alias_allocator(
        new nmranet::AliasAllocator(NODE_ID, &g_if_can));
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