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
#include "custom/HostProtocol.hxx"

#include "commandstation/UpdateProcessor.hxx"
#include "nmranet/TractionTrain.hxx"

#include "custom/HostPacketCanPort.hxx"
#include "custom/LoggingBit.hxx"
#include "custom/AutomataControl.hxx"
#include "mobilestation/MobileStationSlave.hxx"
#include "commandstation/TrainDb.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "commandstation/AllTrainNodes.hxx"
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"

static const nmranet::NodeID NODE_ID = 0x050101011431ULL;

nmranet::SimpleCanStack stack(NODE_ID);
CanHubFlow can_hub1(stack.service());  // this CANbus will have no hardware.

nmranet::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char *const nmranet::SNIP_DYNAMIC_FILENAME = nmranet::MockSNIPUserFile::snip_user_file_path;

extern char __automata_start[];
extern char __automata_end[];

nmranet::FileMemorySpace automata_space("/etc/automata", __automata_end - __automata_start);

bracz_custom::HostClient host_client(stack.dg_service(), stack.node(), &can_hub1);

void log_output(char* buf, int size) {
    if (size <= 0) return;
    host_client.log_output(buf, size);
}

namespace bracz_custom {
void send_host_log_event(HostLogEvent e) {
  host_client.send_host_log_event(e);
}
}

OVERRIDE_CONST(local_nodes_count, 30);
OVERRIDE_CONST(local_alias_cache_size, 30);
OVERRIDE_CONST(num_memory_spaces, 5);

static const uint64_t EVENT_ID = 0x0501010114FF203AULL;
const int main_priority = 2;
OVERRIDE_CONST(main_thread_priority, 2);
OVERRIDE_CONST(main_stack_size, 1500);

bracz_custom::AutomataControl automatas(stack.node(), stack.dg_service(), (const insn_t*) __automata_start);

// Command station objects.
CanIf can1_interface(stack.service(), &can_hub1);
bracz_custom::TrackIfSend track_send(&can_hub1);
commandstation::UpdateProcessor cs_loop(stack.service(), &track_send);
bracz_custom::TrackIfReceive track_recv(&can1_interface, &cs_loop);
static const uint64_t ON_EVENT_ID = 0x0501010114FF0004ULL;
bracz_custom::TrackPowerOnOffBit on_off(ON_EVENT_ID, ON_EVENT_ID+1, &track_send);
nmranet::BitEventConsumer powerbit(&on_off);
nmranet::TrainService traction_service(stack.iface());
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

mobilestation::MobileStationSlave mosta_slave(stack.executor(), &can1_interface);
commandstation::TrainDb train_db;
commandstation::AllTrainNodes all_trains(&train_db, &traction_service, stack.info_flow(), stack.memory_config_handler());
mobilestation::MobileStationTraction mosta_traction(&can1_interface, stack.iface(), &train_db, stack.node());

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[]) {
  // start_watchdog(5000);
  // add_watchdog_reset_timer(500);
  // Does NXP usb have select support?
  // new HubDeviceSelect<HubFlow>(stack.gridconnect_hub(), "/dev/serUSB0");
  stack.add_gridconnect_port("/dev/serUSB0");
  stack.add_can_port_async("/dev/can0");  // TODO: select or blocking?
  HubDeviceNonBlock<CanHubFlow> can1_port(&can_hub1, "/dev/can1");

  LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
  nmranet::BitEventConsumer consumer(&logger);

  stack.loop_executor();
  return 0;
}
