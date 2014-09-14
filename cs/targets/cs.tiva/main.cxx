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

#define LOGLEVEL INFO

#include <stdio.h>
#include <unistd.h>


#include "os/os.h"
#include "utils/Hub.hxx"
#include "utils/HubDevice.hxx"
#include "utils/HubDeviceNonBlock.hxx"
#include "utils/GridConnectHub.hxx"
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
#include "custom/HostLogging.hxx"
#include "mobilestation/MobileStationSlave.hxx"
#include "mobilestation/TrainDb.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "commandstation/UpdateProcessor.hxx"
#include "nmranet/TractionTrain.hxx"
#include "dcc/Loco.hxx"
#include "mobilestation/TrainDb.hxx"
#include "dcc/LocalTrackIf.hxx"
#include "mobilestation/AllTrainNodes.hxx"

#include "TivaDCC.hxx"
#include "ShortDetection.hxx"

#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "dcc_control.hxx"

TivaDCC dcc_hw("/dev/mainline", TIMER1_BASE, TIMER0_BASE, INT_TIMER0A,
               INT_TIMER1A, (uint8_t*)(GPIO_PORTQ_BASE + (GPIO_PIN_0 << 2)), 16,
               500, 2500);

#define STANDALONE

extern "C" {
/** Timer interrupt for DCC packet handling.
 */
void timer0a_interrupt_handler(void)
{
    dcc_hw.interrupt_handler();
}

void timer1a_interrupt_handler(void)
{
  dcc_hw.os_interrupt_handler();
}

}  // extern "C"

// Used to talk to the booster.
//OVERRIDE_CONST(can2_bitrate, 250000);

// Forces all trains to be our responsibility.
OVERRIDE_CONST(mobile_station_train_count, 0);

OVERRIDE_CONST(automata_init_backoff, 20000);
OVERRIDE_CONST(node_init_identify, 0);

OVERRIDE_CONST(dcc_packet_min_refresh_delay_ms, 1);


namespace mobilestation {
/*
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
*/
}  // namespace mobilestation

NO_THREAD nt;
Executor<1> g_executor(nt);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);
CanHubFlow can_hub1(&g_service);  // this CANbus will have no hardware.

static const nmranet::NodeID NODE_ID = 0x050101011432ULL;

extern "C" {
extern insn_t automata_code[];
}


HubFlow stdout_hub(&g_service);
//auto* g_gc_adapter = GCAdapterBase::CreateGridConnectAdapter(&stdout_hub, &can_hub0, false);

extern "C" {
void log_output(char* buf, int size) {
    if (size <= 0) return;
    auto* b = stdout_hub.alloc();
    HASSERT(b);
    b->data()->assign(buf, size);
    if (size > 1) b->data()->push_back('\n');
    stdout_hub.send(b);
}
}

namespace bracz_custom {
void xx_send_host_log_event(HostLogEvent e) {
  log_output((char*)&e, 1);
}
}

static const int kLocalNodesCount = 30;
nmranet::IfCan g_if_can(&g_executor, &can_hub0, kLocalNodesCount, 3,
                        kLocalNodesCount);
static nmranet::AddAliasAllocator _alias_allocator(NODE_ID, &g_if_can);
nmranet::DefaultNode g_node(&g_if_can, NODE_ID);
nmranet::EventService g_event_service(&g_if_can);

static const uint64_t EVENT_ID = 0x0501010114FF203AULL;
const int main_priority = 0;

extern "C" { void resetblink(uint32_t pattern); }

class BlinkerFlow : public StateFlowBase
{
public:
    BlinkerFlow(nmranet::Node* node)
        : StateFlowBase(node->interface()),
          state_(1),
          bit_(node, EVENT_ID, EVENT_ID + 1, &state_, (uint8_t)1),
          producer_(&bit_),
          sleepData_(this)
    {
        start_flow(STATE(blinker));
    }

private:
    Action blinker()
    {
        state_ = !state_;
#ifdef __linux__
        LOG(INFO, "blink produce %d", state_);
#endif
        producer_.Update(&helper_, n_.reset(this));
        return wait_and_call(STATE(handle_sleep));
    }

    Action handle_sleep()
    {
        return sleep_and_call(&sleepData_, MSEC_TO_NSEC(1000), STATE(blinker));
    }

    uint8_t state_;
    nmranet::MemoryBit<uint8_t> bit_;
    nmranet::BitEventProducer producer_;
    nmranet::WriteHelper helper_;
    StateFlowTimer sleepData_;
    BarrierNotifiable n_;
};

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


dcc::LocalTrackIf track_if(&g_service, 2);
commandstation::UpdateProcessor cs_loop(&g_service, &track_if);
commandstation::PoolToQueueFlow<Buffer<dcc::Packet>> pool_translator(&g_service, track_if.pool(), &cs_loop);
TivaTrackPowerOnOffBit on_off(nmranet::TractionDefs::CLEAR_EMERGENCY_STOP_EVENT,
                              nmranet::TractionDefs::EMERGENCY_STOP_EVENT);
nmranet::BitEventConsumer powerbit(&on_off);
nmranet::TrainService traction_service(&g_if_can);

//dcc::Dcc28Train train_Am843(dcc::DccShortAddress(43));
//nmranet::TrainNode train_Am843_node(&traction_service, &train_Am843);
//dcc::Dcc28Train train_Re460(dcc::DccShortAddress(22));
//dcc::MMNewTrain train_Re460(dcc::MMAddress(22));
//nmranet::TrainNode train_Re460_node(&traction_service, &train_Re460);


//mobilestation::MobileStationSlave mosta_slave(&g_executor, &can1_interface);
mobilestation::TrainDb train_db;
CanIf can1_interface(&g_service, &can_hub1);
mobilestation::MobileStationTraction mosta_traction(&can1_interface, &g_if_can, &train_db, &g_node);

mobilestation::AllTrainNodes all_trains(&train_db, &traction_service);


void mydisable()
{
  IntDisable(INT_TIMER5A);
  IntDisable(INT_TIMER0A);
  asm("BKPT 0");
}

TivaShortDetectionModule g_short_det(&g_service, MSEC_TO_NSEC(1));
BlinkerFlow blinker(&g_node);


/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
  //  mydisable();
    start_watchdog(5000);
    add_watchdog_reset_timer(500);
#ifdef STANDALONE
    PacketQueue::initialize("/dev/serUSB0");
#else
    PacketQueue::initialize("/dev/serUSB0");
#endif
    //HubDeviceNonBlock<CanHubFlow> can0_port(&can_hub0, "/dev/can0");
    //HubDeviceNonBlock<CanHubFlow> can1_port(&can_hub1, "/dev/can1");
    bracz_custom::init_host_packet_can_bridge(&can_hub1);
    FdHubPort<HubFlow> stdout_port(&stdout_hub, 0, EmptyNotifiable::DefaultInstance());

    int mainline = open("/dev/mainline", O_RDWR);
    HASSERT(mainline > 0);
    track_if.set_fd(mainline);

    nmranet::Velocity v;
    v.set_mph(29);
    //XXtrain_Re460.set_speed(v);
    /*train_Am843.set_speed(v);
    train_Am843.set_fn(0, 1);
    train_Re460.set_fn(0, 1);*/

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

    AutomataRunner runner(&g_node, automata_code);
    resume_all_automata();

#ifdef STANDALONE
    // Start dcc output
    enable_dcc();
#else
    // Do not start dcc output.
    disable_dcc();
#endif

    g_executor.thread_body();
    return 0;
}
