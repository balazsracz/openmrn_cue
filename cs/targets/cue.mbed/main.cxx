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
#include "nmranet/SimpleStack.hxx"

#include "nmranet/PolledProducer.hxx"
#include "custom/AutomataControl.hxx"
#include "custom/HostPacketCanPort.hxx"
#include "custom/HostProtocol.hxx"
#include "nmranet/EventHandlerTemplates.hxx"
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"
#include "nmranet/SimpleStack.hxx"
#include "os/watchdog.h"
#include "utils/HubDeviceSelect.hxx"
#include "utils/Debouncer.hxx"


OVERRIDE_CONST(automata_init_backoff, 20000);
OVERRIDE_CONST(node_init_identify, 0);

OVERRIDE_CONST(num_datagram_registry_entries, 3);

static const openlcb::NodeID NODE_ID = 0x050101011437ULL;
static const uint64_t EVENT_ID = (NODE_ID << 16);

openlcb::SimpleCanStack stack(NODE_ID);
CanHubFlow can_hub1(stack.service());  // this CANbus will have no hardware.

openlcb::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char *const openlcb::SNIP_DYNAMIC_FILENAME = openlcb::MockSNIPUserFile::snip_user_file_path;

extern "C" {
extern insn_t automata_code[];
}

bracz_custom::HostClient host_client(stack.dg_service(), stack.node(), &can_hub1);

extern "C" {
#ifdef STANDALONE

Executor<1> stdout_exec("logger", 1, 1000);
Service stdout_service(&stdout_exec);
HubFlow stdout_hub(&stdout_service);

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
    host_client.log_output(buf, size);
}

#endif
} // extern c

namespace bracz_custom {
void send_host_log_event(HostLogEvent e) {
  host_client.send_host_log_event(e);
}
}

OVERRIDE_CONST(local_nodes_count, 30);
OVERRIDE_CONST(local_alias_cache_size, 30);
OVERRIDE_CONST(num_memory_spaces, 5);


//static const uint64_t EVENT_ID = 0x0501010114FF203AULL;
const int main_priority = 0;

DigitalIn startpin(p20);

bracz_custom::AutomataControl automatas(stack.node(), stack.dg_service(), (const insn_t*) automata_code);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
    start_watchdog(5000);
    add_watchdog_reset_timer(500);
    stack.add_can_port_async("/dev/can0");
    stack.loop_executor();
    return 0;
}
