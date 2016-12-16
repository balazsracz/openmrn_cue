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

#include "custom/MemorizingEventHandler.hxx"
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"
#include "nmranet/SimpleStack.hxx"
#include "utils/ClientConnection.hxx"

static const openlcb::NodeID NODE_ID = 0x050101011442ULL;
openlcb::SimpleCanStack stack(NODE_ID);

openlcb::MockSNIPUserFile snip_user_file(
    "Memorizing node",
    "Provides persistent state information for train location events.");
const char *const openlcb::SNIP_DYNAMIC_FILENAME =
    openlcb::MockSNIPUserFile::snip_user_file_path;

extern const openlcb::SimpleNodeStaticValues openlcb::SNIP_STATIC_DATA = {
    4, "Balazs Racz", "Memorizing node", "linux.x86", "1.2"};

static const uint64_t BRACZ_LAYOUT = 0x0501010114FF0000ULL;

openlcb::MemorizingHandlerManager g_permabits(stack.node(),
                                              BRACZ_LAYOUT | 0xC000, 1024, 2);

void usage(const char *e) {
  fprintf(stderr,
          "Usage: %s [-p port] [-d device_path] [-u upstream_host] "
          "[-q upstream_port] [-t]\n\n",
          e);
  fprintf(stderr,
          "Memorizing node. Keeps state of a certain set of events in RAM, "
          "providing prior state information for nodes that reboot on the "
          "OpenLCB network.\n\nArguments:\n");
  fprintf(stderr,
          "\t-u upstream_host   is the host name for a GridConnect TCP "
          "hub. Required.\n");
  fprintf(stderr,
          "\t-q upstream_port   is the port number for the GC hub. Default "
          "12021.\n");
  exit(1);
}

int upstream_port = 12021;
const char *upstream_host = nullptr;

void parse_args(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, "hu:q:")) >= 0) {
    switch (opt) {
      case 'h':
        usage(argv[0]);
        break;
      case 'u':
        upstream_host = optarg;
        break;
      case 'q':
        upstream_port = atoi(optarg);
        break;
      default:
        fprintf(stderr, "Unknown option %c\n", opt);
        usage(argv[0]);
    }
  }
  if (!upstream_host) {
    fprintf(stderr, "Must specify -u for upstream host.\n");
    usage(argv[0]);
  }
}

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[]) {
  parse_args(argc, argv);

  std::unique_ptr<ConnectionClient> connection(new UpstreamConnectionClient(
      "hub", stack.can_hub(), upstream_host, upstream_port));

  while (!connection->ping()) {
    sleep(1);
  }
  stack.start_executor_thread("nmranet_exec", 0, 0);

  while (1) {
    connection->ping();
    sleep(1);
  }

  return 0;
}
