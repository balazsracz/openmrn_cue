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
#include "utils/HubDeviceNonBlock.hxx"
#include "executor/Executor.hxx"
#include "can_frame.h"
#include "nmranet_config.h"
#include "os/watchdog.h"

#include "openlcb/IfCan.hxx"
#include "openlcb/If.hxx"
#include "openlcb/AliasAllocator.hxx"
#include "openlcb/EventService.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/DefaultNode.hxx"
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
#include "commandstation/TrainDb.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "commandstation/UpdateProcessor.hxx"
#include "openlcb/TractionTrain.hxx"
#include "dcc/Loco.hxx"
#include "custom/LoggingBit.hxx"
#include "openlcb/SimpleStack.hxx"
#include "openlcb/SimpleNodeInfoMockUserFile.hxx"

// Used to talk to the booster.
OVERRIDE_CONST(can2_bitrate, 250000);

// Forces all trains to be our responsibility.
OVERRIDE_CONST(mobile_station_train_count, 0);

OVERRIDE_CONST(automata_init_backoff, 20000);
OVERRIDE_CONST(node_init_identify, 0);

namespace commandstation {
extern const struct const_traindb_entry_t const_lokdb[];

const struct const_traindb_entry_t const_lokdb[] = {
  // 0
  { 43, //{ 0, 1, 3, 4,  0xff, },
    { LIGHT, TELEX, FN_NONEXISTANT, FNT11, ABV,  0xff, },
    "Am 843 093-6", DCC_28 },
  // 1
  { 22, //{ 0, 3, 4,  0xff, },
    { LIGHT, FN_NONEXISTANT, FN_NONEXISTANT, FNT11, ABV,  0xff, },
    "RE 460 TSR", MARKLIN_NEW }, // todo: there is no beamer here
  // 2 (jim's)
  { 0x0761, //{ 0, 3, 0xff },
    { LIGHT, FN_NONEXISTANT, FN_NONEXISTANT, WHISTLE, 0xff, }, "Jim's steam", OLCBUSER },
  { 0, {0,}, "", 0},
  { 0, {0,}, "", 0},
  { 0, {0,}, "", 0},
};
extern const size_t const_lokdb_size;
const size_t const_lokdb_size = sizeof(const_lokdb) / sizeof(const_lokdb[0]);
}  // namespace commandstation


static const openlcb::NodeID NODE_ID = 0x050101011434ULL;
openlcb::SimpleCanStack stack(NODE_ID);
CanHubFlow can_hub1(stack.service());


openlcb::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char *const openlcb::SNIP_DYNAMIC_FILENAME = openlcb::MockSNIPUserFile::snip_user_file_path;


extern "C" {
extern insn_t automata_code[];
}

void log_output(char* buf, int size) {
    if (size <= 0) return;
    PacketBase pkt(size + 1);
    pkt[0] = CMD_VCOM1;
    memcpy(pkt.buf() + 1, buf, size);
    PacketQueue::instance()->TransmitPacket(pkt);
}


static const uint64_t EVENT_ID = 0x0501010114FF203AULL;
const int main_priority = 0;

extern "C" { void resetblink(uint32_t pattern); }

DigitalIn startpin(p20);

CanIf can1_interface(stack.service(), &can_hub1);
bracz_custom::TrackIfSend track_send(&can_hub1);
commandstation::UpdateProcessor cs_loop(stack.service(), &track_send);
bracz_custom::TrackIfReceive track_recv(&can1_interface, &cs_loop);
static const uint64_t ON_EVENT_ID = 0x0501010114FF0004ULL;
bracz_custom::TrackPowerOnOffBit on_off(ON_EVENT_ID, ON_EVENT_ID+1, &track_send);
openlcb::BitEventConsumer powerbit(&on_off);
openlcb::TrainService traction_service(stack.iface());

dcc::Dcc28Train train_Am843(dcc::DccShortAddress(43));
openlcb::TrainNodeForProxy train_Am843_node(&traction_service, &train_Am843);
dcc::MMNewTrain train_Re460(dcc::MMAddress(22));
openlcb::TrainNodeForProxy train_Re460_node(&traction_service, &train_Re460);

//mobilestation::MobileStationSlave mosta_slave(&g_executor, &can1_interface);
commandstation::TrainDb train_db;
mobilestation::MobileStationTraction mosta_traction(&can1_interface, stack.iface(), &train_db, stack.node());


/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
    start_watchdog(5000);
    add_watchdog_reset_timer(500);
    PacketQueue::initialize(stack.can_hub(), "/dev/serUSB0", true);
    //stack.add_gridconnect_port("/dev/serUSB0");
    stack.add_can_port_async("/dev/can0");  // TODO: select or blocking?
    HubDeviceNonBlock<CanHubFlow> can1_port(&can_hub1, "/dev/can1");
    bracz_custom::init_host_packet_can_bridge(&can_hub1);

    openlcb::Velocity v;
    v.set_mph(29);
    //XXtrain_Re460.set_speed(v);
    //XXtrain_Am843.set_speed(v);

    /*int fd = open("/dev/can0", O_RDWR);
    ASSERT(fd >= 0);
    dcc_can_init(fd);*/

    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    openlcb::BitEventConsumer consumer(&logger);
    stack.loop_executor();
    return 0;

}
