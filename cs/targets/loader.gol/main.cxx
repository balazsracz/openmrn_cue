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
#include "openlcb/BootloaderClient.hxx"
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
openlcb::BootloaderClient bootloader_client(
    &g_node, &g_datagram_can, &g_if_can);

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
// if true, sends global enter bootloader command.
bool global_enter_bootloader = true;
vector<openlcb::NodeID> reboot_nodes;
openlcb::NodeID nodeid_base = 0x050101018500;
bool osc_test = false;
unsigned osc_period = 100;
unsigned osc_count = 10;

void usage(const char *e)
{
    fprintf(stderr,
        "Usage: %s ([-i destination_host] [-p port] | [-d device_path])  "
        " -c num_clients "
        " [-r] [-o osc_test_count]"
        " -f filename [reboot_node reboot_node ...]\n",
        e);
    fprintf(stderr, "Connects to an openlcb bus and performs the "
                    "bootloader protocol using the broadcast method.\n");
    fprintf(stderr,
        "The bus connection will be through an OpenLCB HUB on "
        "destination_host:port with OpenLCB over TCP "
        "(in GridConnect format) protocol, or through the CAN-USB device "
        "(also in GridConnect protocol) found at device_path. Device takes "
        "precedence over TCP host:port specification.");
    fprintf(stderr, "The default target is localhost:12021.\n");
    fprintf(stderr,
            "\n\tnum_clients defines how many clients should be reporting "
            "their status after every block.\n");
    fprintf(stderr,
        "\n\t-r will skip the global enter bootloader command\n");
    fprintf(stderr,
        "\n\treboot_node are one-byte hex suffixes of node IDs that will be rebooted into bootloader mode. The first 5 bytes are fixed.\n");
    fprintf(stderr,
        "\n\n\t -o count will run no bootload, just an oscillator calibration test, with count packets of 100 msec each.\n");
    exit(1);
}

void parse_args(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "hp:i:d:f:c:ro:")) >= 0)
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
            case 'r':
                global_enter_bootloader = false;
                break;
            case 'c':
                num_client = atoi(optarg);
                break;
            case 'o':
                osc_test = true;
                osc_count = atoi(optarg);
                break;
            default:
              fprintf(stderr, "Unknown option %c\n", opt);
              usage(argv[0]);
        }
    }
    if (!osc_test && !filename)
    {
        usage(argv[0]);
    }
    extern int optind;
    while (optind < argc) {
      openlcb::NodeID nid = nodeid_base;
      auto ending = strtol(argv[optind], nullptr, 16);
      reboot_nodes.push_back(nid | ending);
      ++optind;
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

    SyncNotifiable n;

    if (osc_test) {
      printf("Doing osc calibration with %d packets of %d msec apart...\n",
             osc_count, osc_period);
      class OscTimer : public ::Timer {
       public:
        OscTimer(Notifiable* done)
            : ::Timer(g_executor.active_timers()),
            done_(done) {
          start(MSEC_TO_NSEC(osc_period));
        }

        long long timeout() override {
          uint64_t ev = 0x09000DFF00000000 | 30;
          if (remaining < 0) {
            // first.
            remaining = osc_count;
            ev |= (0x80) << 16;
          } else if (remaining == 0) {
            done_->notify();
            return NONE;
          }
          --remaining;
          ev |= (remaining) << 16;
          ev |= (osc_period / 10) << 24;
          openlcb::send_event(&g_node, ev);
          return RESTART;
        }

       private:
        int remaining{-1};
        Notifiable* done_;
      } osc_timer(&n);
      n.wait_for_notification();

      printf("Done\n");
      return 0;
    }
    
    openlcb::BootloaderResponse response;
    for (const auto &id : reboot_nodes) {
      Buffer<openlcb::BootloaderRequest> *b;
      mainBufferPool->alloc(&b);
      b->data()->dst.alias = 0;
      b->data()->dst.id = id;
      b->data()->memory_space = openlcb::MemoryConfigDefs::SPACE_FIRMWARE;
      b->data()->offset = 0;
      b->data()->response = &response;
      b->data()->request_reboot = true;
      b->data()->request_reboot_after = false;
      b->data()->skip_pip = true;
      b->data()->data.clear();
      BarrierNotifiable bn(&n);
      b->set_done(&bn);

      fprintf(stderr, "Rebooting 0x%012" PRIx64 "... ", id);
      bootloader_client.send(b);
      n.wait_for_notification();

      fprintf(stderr, " %04x\n", response.error_code);
    }

    string payload = read_file_to_string(filename);
    printf("Read %" PRIdPTR " bytes from file %s.\n", payload.size(),
        filename);

    static constexpr uint64_t kEventPrefix = UINT64_C(0x09000DF900000000);
    static constexpr uint64_t kGlobalEvent = UINT64_C(0x09000DFF00000000);
    g_event_handler.add_entry(kGlobalEvent | 17,
                              openlcb::CallbackEventHandler::IS_CONSUMER);

    if (global_enter_bootloader) {
      // enter bootloader
      openlcb::send_event(&g_node, kGlobalEvent | 14);
    }

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
