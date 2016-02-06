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
#include "executor/PoolToQueueFlow.hxx"
#include "can_frame.h"
#include "nmranet_config.h"
#include "os/watchdog.h"

#include "nmranet/IfCan.hxx"
#include "nmranet/If.hxx"
#include "nmranet/AliasAllocator.hxx"
#include "nmranet/EventService.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/NodeInitializeFlow.hxx"
#include "nmranet/DefaultNode.hxx"
#include "nmranet/TractionTestTrain.hxx"
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
#include "commandstation/TrainDb.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "commandstation/UpdateProcessor.hxx"
#include "nmranet/TractionTrain.hxx"
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"
#include "dcc/Loco.hxx"
#include "commandstation/TrainDb.hxx"
#include "dcc/LocalTrackIf.hxx"
#include "commandstation/AllTrainNodes.hxx"

#include "custom/TivaShortDetection.hxx"
#include "custom/WiiChuckThrottle.hxx"
#include "custom/WiiChuckReader.hxx"
#include "custom/BlinkerFlow.hxx"
#include "custom/LoggingBit.hxx"

#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "dcc_control.hxx"
#include "DccHardware.hxx"
#include "TivaDCC.hxx"
#include "dcc/RailCom.hxx"
#include "hardware.hxx"

#define STANDALONE

// Used to talk to the booster.
//OVERRIDE_CONST(can2_bitrate, 250000);

// Forces all trains to be our responsibility.
OVERRIDE_CONST(mobile_station_train_count, 0);

OVERRIDE_CONST(automata_init_backoff, 20000);
OVERRIDE_CONST(node_init_identify, 0);

OVERRIDE_CONST(dcc_packet_min_refresh_delay_ms, 1);

namespace mobilestation {

extern const struct const_loco_db_t const_lokdb[];

const struct const_loco_db_t const_lokdb[] = {
  // 0
  { 44, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, FNT11, ABV,  0xff, },
    "Am3 843 093-6", DCC_28 },
  // 1
  { 52, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, SHUNT, ABV,  0xff, },
    "BR3 260417", DCC_28 },  // ESU LokPilot 3.0
  // 2
  { 23, { 0, 3, 4,  0xff, }, { LIGHT, FNT11, ABV,  0xff, },
    "RE3 460 TSR", MARKLIN_NEW }, // todo: there is no beamer here
  // id 3
  { 38, { 0, 3, 4, 0xff, }, { LIGHT, FNT11, ABV, 0xff, },
    "BDe 4/4 1640", DCC_128 | PUSHPULL },  // Tams LD-G32, DC motor
  // 3 (jim's)
  //{ 0x0761, { 0, 3, 0xff }, { LIGHT, WHISTLE, 0xff, }, "Jim's steam", OLCBUSER },
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
  { 0, {0, }, {0,}, "", 0},
};
extern const size_t const_lokdb_size;
const size_t const_lokdb_size = sizeof(const_lokdb) / sizeof(const_lokdb[0]);

}  // namespace mobilestation


static const nmranet::NodeID NODE_ID = 0x050101011432ULL;
nmranet::SimpleCanStack stack(NODE_ID);
CanHubFlow can_hub1(stack.service());  // this CANbus will have no hardware.
nmranet::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char *const nmranet::SNIP_DYNAMIC_FILENAME = nmranet::MockSNIPUserFile::snip_user_file_path;


extern "C" {
extern insn_t automata_code[];
}

HubFlow stdout_hub(stack.service());
//auto* g_gc_adapter = GCAdapterBase::CreateGridConnectAdapter(&stdout_hub, &can_hub0, false);

extern "C" {

#ifdef STANDALONE

void log_output(char* buf, int size) {
    if (size <= 0) return;
    auto* b = stdout_hub.alloc();
    HASSERT(b);
    b->data()->assign(buf, size);
    if (size > 1) b->data()->push_back('\n');
    stdout_hub.send(b);
}

#else

void log_output(char* buf, int size) {
    if (size <= 0) return;
    PacketBase pkt(size + 1);
    pkt[0] = CMD_VCOM1;
    memcpy(pkt.buf() + 1, buf, size);
    PacketQueue::instance()->TransmitPacket(pkt);
}

#endif

}

#ifdef STANDALONE
namespace bracz_custom {
void send_host_log_event(HostLogEvent e) {
  log_output((char*)&e, 1);
}
}
#endif

OVERRIDE_CONST(local_nodes_count, 30);
OVERRIDE_CONST(local_alias_cache_size, 30);
OVERRIDE_CONST(num_memory_spaces, 5);

static const uint64_t EVENT_ID = 0x0501010114FF203AULL;
const int main_priority = 0;

#ifdef USE_WII_CHUCK
bracz_custom::WiiChuckThrottle wii_throttle(stack.node(), 0x060100000000ULL | 51);
#endif

extern TivaDCC<DccHwDefs> dcc_hw;

/*
class RailcomDebugFlow : public StateFlowBase {
 public:
  RailcomDebugFlow() : StateFlowBase(&stack.service()) {
    start_flow(STATE(register_and_sleep));
  }

 private:
  Action register_and_sleep() {
    dcc_hw.railcom_buffer_notify_ = this;
    return wait_and_call(STATE(railcom_arrived));
  }

  Action railcom_arrived() {
    char buf[200];
    int ofs = 0;
    for (int i = 0; i < dcc_hw.railcom_buffer_len_; ++i) {
      ofs += sprintf(buf+ofs, "0x%02x (0x%02x), ", dcc_hw.railcom_buffer_[i],
                     dcc::railcom_decode[dcc_hw.railcom_buffer_[i]]);
    }
    uint8_t type = (dcc::railcom_decode[dcc_hw.railcom_buffer_[0]] >> 2);
    if (dcc_hw.railcom_buffer_len_ == 2) {
      uint8_t payload = dcc::railcom_decode[dcc_hw.railcom_buffer_[0]] & 0x3;
      payload <<= 6;
      payload |= dcc::railcom_decode[dcc_hw.railcom_buffer_[1]];
      switch(type) {
        case dcc::RMOB_ADRLOW:
          ofs += sprintf(buf+ofs, "adrlow=%d", payload);
          break;
        case dcc::RMOB_ADRHIGH:
          ofs += sprintf(buf+ofs, "adrhigh=%d", payload);
          break;
        case dcc::RMOB_EXT:
          ofs += sprintf(buf+ofs, "ext=%d", payload);
          break;
        case dcc::RMOB_DYN:
          ofs += sprintf(buf+ofs, "dyn=%d", payload);
          break;
        case dcc::RMOB_SUBID:
          ofs += sprintf(buf+ofs, "subid=%d", payload);
          break;
        default:
          ofs += sprintf(buf+ofs, "type-%d=%d", type, payload);
      }
    }
    LOG(INFO, "Railcom data: %s", buf);
    return call_immediately(STATE(register_and_sleep));
  }
} railcom_debug;
*/

dcc::LocalTrackIf track_if(stack.service(), 2);
commandstation::UpdateProcessor cs_loop(stack.service(), &track_if);
PoolToQueueFlow<Buffer<dcc::Packet>> pool_translator(stack.service(), track_if.pool(), &cs_loop);
TivaTrackPowerOnOffBit on_off(stack.node(),
                              nmranet::TractionDefs::CLEAR_EMERGENCY_STOP_EVENT,
                              nmranet::TractionDefs::EMERGENCY_STOP_EVENT);
nmranet::BitEventConsumer powerbit(&on_off);
nmranet::TrainService traction_service(stack.iface());

//dcc::Dcc28Train train_Am843(dcc::DccShortAddress(43));
//nmranet::TrainNode train_Am843_node(&traction_service, &train_Am843);
//dcc::Dcc28Train train_Re460(dcc::DccShortAddress(22));
//dcc::MMNewTrain train_Re460(dcc::MMAddress(22));
//nmranet::TrainNode train_Re460_node(&traction_service, &train_Re460);

//mobilestation::MobileStationSlave mosta_slave(stack.executor(), &can1_interface);
mobilestation::TrainDb train_db;
CanIf can1_interface(stack.service(), &can_hub1);
mobilestation::MobileStationTraction mosta_traction(&can1_interface, stack.iface(), &train_db, stack.node());

mobilestation::AllTrainNodes all_trains(&train_db, &traction_service, stack.info_flow(), stack.memory_config_handler());


void mydisable()
{
  IntDisable(INT_TIMER5A);
  IntDisable(INT_TIMER0A);
  asm("BKPT 0");
}

TivaShortDetectionModule<DccHwDefs> g_short_detector(stack.node());

extern "C" {
void adc0_seq3_interrupt_handler(void) {
  // COMPILE_ASSERT(ADC_BASE == ADC0_BASE && SEQUENCER == 3)
  g_short_detector.interrupt_handler();
}
}

BlinkerFlow blinker(stack.node(), EVENT_ID);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
  disable_dcc();
  //start_watchdog(5000);
  //  add_watchdog_reset_timer(500);

    setblink(0x800A02);
#ifdef STANDALONE
#else
    PacketQueue::initialize(stack.can_hub(), "/dev/serUSB0", true);
#endif
    setblink(0);
    stack.add_can_port_select("/dev/can0");
    //HubDeviceNonBlock<CanHubFlow> can0_port(&can_hub0, "/dev/can0");
    //HubDeviceNonBlock<CanHubFlow> can1_port(&can_hub1, "/dev/can1");
    bracz_custom::init_host_packet_can_bridge(&can_hub1);
    //FdHubPort<HubFlow> stdout_port(&stdout_hub, 0, EmptyNotifiable::DefaultInstance());

#ifdef USE_WII_CHUCK
    bracz_custom::WiiChuckReader wii_reader("/dev/i2c1", &wii_throttle);
    wii_reader.start();
#endif

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

    //AutomataRunner runner(stack.node(), automata_code);
    //resume_all_automata();

#ifdef STANDALONE
    // Start dcc output
    enable_dcc();
#else
    // Do not start dcc output.
    disable_dcc();
#endif

    stack.loop_executor();
    return 0;
}
