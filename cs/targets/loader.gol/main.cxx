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
 * An application for updating the firmware of a remote node on the bus.
 *
 * @author Balazs Racz
 * @date 3 Aug 2013
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include <memory>

#include "executor/Executor.hxx"
#include "executor/Service.hxx"
#include "openlcb/Defs.hxx"
#include "openlcb/Node.hxx"
#include "openlcb/IfCan.hxx"
#include "openlcb/AliasAllocator.hxx"
#include "openlcb/DefaultNode.hxx"
#include "openlcb/EventService.hxx"
#include "openlcb/CallbackEventHandler.hxx"
#include "openlcb/NodeInitializeFlow.hxx"
#include "openlcb/DatagramCan.hxx"
#include "utils/socket_listener.hxx"
#include "utils/FileUtils.hxx"
#include "utils/Hub.hxx"
#include "utils/GridConnectHub.hxx"
#include "utils/GcTcpHub.hxx"
#include "os/sleep.h"


NO_THREAD nt;
Executor<1> g_executor(nt);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);

static const openlcb::NodeID NODE_ID = 0x05010101181FULL;

openlcb::IfCan g_if_can(&g_executor, &can_hub0, 3, 3, 2);
openlcb::InitializeFlow g_init_flow{&g_service};
openlcb::CanDatagramService g_datagram_can(&g_if_can, 10, 2);
static openlcb::AddAliasAllocator g_alias_allocator(NODE_ID, &g_if_can);
openlcb::DefaultNode g_node(&g_if_can, NODE_ID);
openlcb::EventService g_event_service{&g_if_can};

namespace openlcb
{
Pool *const g_incoming_datagram_allocator = mainBufferPool;
extern long long DATAGRAM_RESPONSE_TIMEOUT_NSEC;
}

int port = 12021;
const char *host = "localhost";
const char *device_path = nullptr;
const char *filename = nullptr;
int num_client = 0;

void usage(const char *e)
{
    fprintf(stderr,
        "Usage: %s ([-i destination_host] [-p port] | [-d device_path]) [-s "
        "memory_space_id] [-c csum_algo [-m hw_magic] [-M hw_magic2]] [-r] [-t] [-x] "
        "[-w dg_timeout] [-W stream_timeout] [-D dump_filename] "
        "(-n nodeid | -a alias) -f filename\n",
        e);
    fprintf(stderr, "Connects to an openlcb bus and performs the "
                    "bootloader protocol on openlcb node with id nodeid with "
                    "the contents of a given file.\n");
    fprintf(stderr,
        "The bus connection will be through an OpenLCB HUB on "
        "destination_host:port with OpenLCB over TCP "
        "(in GridConnect format) protocol, or through the CAN-USB device "
        "(also in GridConnect protocol) found at device_path. Device takes "
        "precedence over TCP host:port specification.");
    fprintf(stderr, "The default target is localhost:12021.\n");
    fprintf(stderr,
        "\n\tnodeid should be a 12-char hex string with 0x prefix and "
        "no separators, like '-b 0x05010101141F'\n");
    fprintf(stderr,
        "\n\talias should be a 3-char hex string with 0x prefix and no "
        "separators, like '-a 0x3F9'\n");
    fprintf(stderr,
        "\n\tmemory_space_id defines which memory space to write the "
        "data into. Default is '-s 0xEF'.\n");
    fprintf(stderr,
        "\n\tcsum_algo defines the checksum algorithm to use. If "
        "omitted, no checksumming is done before writing the "
        "data. hw_magic and hw_magic2 are arguments to the checksum.\n");
    fprintf(stderr,
        "\n\t-r request the target to enter bootloader mode before sending "
        "data\n");
    fprintf(stderr,
        "\n\tUnless -t is specified the target will be rebooted after "
        "flashing complete.\n");
    fprintf(stderr, "\n\t-x skips the PIP request and uses streams.\n");
    fprintf(stderr,
        "\n\t-w dg_timeout sets how many seconds to wait for a datagram "
        "reply.\n");
    fprintf(stderr,
        "\n\t-D filename  writes the checksummed payload to the given file.\n");
    exit(1);
}

void parse_args(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "hp:i:d:f:c:")) >= 0)
    {
        switch (opt)
        {
            case 'h':
                usage(argv[0]);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'i':
                host = optarg;
                break;
            case 'd':
                device_path = optarg;
                break;
            case 'f':
                filename = optarg;
                break;
            case 'c':
                num_client = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Unknown option %c\n", opt);
                usage(argv[0]);
        }
    }
    if (!filename)
    {
        usage(argv[0]);
    }
}

OSSem sem;

void event_report(const openlcb::EventRegistryEntry &registry_entry,
                  openlcb::EventReport *report, BarrierNotifiable *done) {
  sem.post();
}

openlcb::CallbackEventHandler g_event_handler(&g_node, &event_report, nullptr);

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[])
{
    parse_args(argc, argv);
    int conn_fd = 0;
    if (device_path)
    {
        conn_fd = ::open(device_path, O_RDWR);
    }
    else
    {
        conn_fd = ConnectSocket(host, port);
    }
    HASSERT(conn_fd >= 0);
    create_gc_port_for_can_hub(&can_hub0, conn_fd);

    g_if_can.add_addressed_message_support();
    // Bootstraps the alias allocation process.
    g_if_can.alias_allocator()->send(g_if_can.alias_allocator()->alloc());

    g_executor.start_thread("g_executor", 0, 1024);
    usleep(400000);

    string payload = read_file_to_string(filename);
    printf("Read %" PRIdPTR " bytes from file %s.\n", payload.size(),
        filename);

    static constexpr uint64_t kEventPrefix = UINT64_C(0x09000DF900000000);
    static constexpr uint64_t kGlobalEvent = UINT64_C(0x09000DFF00000000);
    g_event_handler.add_entry(kGlobalEvent | 17,
                              openlcb::CallbackEventHandler::IS_CONSUMER);

    // enter bootloader
    openlcb::send_event(&g_node, kGlobalEvent | 14);

    microsleep(350000);
    openlcb::send_event(&g_node, kGlobalEvent | 15);

    microsleep(100000);

    unsigned ofs = 0;
    static constexpr unsigned kBlockSize = 2048;
    unsigned next_block = ofs + kBlockSize;
    while(ofs < payload.size()) {
      uint32_t d = -1;
      unsigned len = 4;
      memcpy(&d, &payload[ofs], len);
      d = htobe32(d);
      openlcb::send_event(&g_node, kEventPrefix | d);
      ofs += len;
      if (ofs >= next_block) {
        printf("Written %u bytes\n", ofs);
        if (num_client > 0) {
          for (int i = 0; i < num_client; ++i) {
            HASSERT(0 == sem.timedwait(SEC_TO_NSEC(2)));
          }
        } else {
          microsleep(2000000);
        }
        next_block += kBlockSize;
      }
    }
    openlcb::send_event(&g_node, kGlobalEvent | 16);
    microsleep(1000000);

    printf("Done\n");

    exit(0);
    return 0;
}
