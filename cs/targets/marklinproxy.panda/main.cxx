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
 * A throttle proxy that talks to a Marklin Mobile Station (as a throttle) and
 * translates requests coming in to OpenLCB train commands.
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

#include "mobilestation/MobileStationSlave.hxx"
#include "commandstation/TrainDb.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "custom/HostLogging.hxx"
#include "nmranet/SimpleStack.hxx"
#include "custom/LoggingBit.hxx"
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"


static const nmranet::NodeID NODE_ID = 0x050101011435ULL;
nmranet::SimpleCanStack stack(NODE_ID);
CanHubFlow can_hub1(stack.service());

nmranet::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char *const nmranet::SNIP_DYNAMIC_FILENAME = nmranet::MockSNIPUserFile::snip_user_file_path;

/*void log_output(char* buf, int size) {
    if (size <= 0) return;
    PacketBase pkt(size + 1);
    pkt[0] = CMD_VCOM1;
    memcpy(pkt.buf() + 1, buf, size);
    PacketQueue::instance()->TransmitPacket(pkt);
    }*/

namespace bracz_custom {
void send_host_log_event(HostLogEvent e) {
  log_output((char*)&e, 1);
}
}

OVERRIDE_CONST(local_nodes_count, 30);
OVERRIDE_CONST(local_alias_cache_size, 30);
OVERRIDE_CONST(main_thread_priority, 2);
OVERRIDE_CONST(main_stack_size, 1500);

// Command station objects.
CanIf can1_interface(stack.service(), &can_hub1);

mobilestation::MobileStationSlave mosta_slave(stack.executor(), &can1_interface);
mobilestation::TrainDb train_db;
mobilestation::MobileStationTraction mosta_traction(&can1_interface, stack.iface(), &train_db, stack.node());

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
  //start_watchdog(5000);
  // add_watchdog_reset_timer(500);

  stack.add_can_port_async("/dev/can0");
    HubDeviceNonBlock<CanHubFlow> can1_port(&can_hub1, "/dev/can1");

    static const uint64_t EVENT_ID = 0x0501010114FF203AULL;
    LoggingBit logger(EVENT_ID, EVENT_ID + 1, "blinker");
    nmranet::BitEventConsumer consumer(&logger);

    stack.loop_executor();
    return 0;
}
