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
 * Cue node running under linux.
 *
 * @author Balazs Racz
 * @date 20 Sep 2015
 */

#include <stdio.h>
#include <unistd.h>

#include "nmranet/SimpleStack.hxx"
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"
#include "custom/AutomataControl.hxx"
#include "utils/Ewma.hxx"

using nmranet::MemoryConfigDefs;
using nmranet::DatagramDefs;
using nmranet::DatagramClient;

static const nmranet::NodeID NODE_ID = 0x05010101143EULL;
nmranet::SimpleCanStack stack(NODE_ID);

nmranet::MockSNIPUserFile snip_user_file("Signal logic",
                                         "This node is loaded with the signaling logic");
const char *const nmranet::SNIP_DYNAMIC_FILENAME =
    nmranet::MockSNIPUserFile::snip_user_file_path;

int port = 12021;
const char *host = "localhost";
const char *device_path = nullptr;

void usage(const char *e) {
  fprintf(stderr,
          "Usage: %s ([-i destination_host] [-p port] | [-d device_path])\n",
          e);
  fprintf(stderr,
          "Connects to an openlcb bus and operates the loaded logic.\n");
  fprintf(stderr,
          "The bus connection will be through an OpenLCB HUB on "
          "destination_host:port with OpenLCB over TCP "
          "(in GridConnect format) protocol, or through the CAN-USB device "
          "(also in GridConnect protocol) found at device_path. Device takes "
          "precedence over TCP host:port specification.");
  fprintf(stderr, "The default target is localhost:12021.\n");
  HASSERT(0);
}

void parse_args(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, "hi:p:d:")) >= 0) {
    switch (opt) {
      case 'h':
        usage(argv[0]);
        break;
      case 'i':
        host = optarg;
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'd':
        device_path = optarg;
        break;
      default:
        fprintf(stderr, "Unknown option %c\n", opt);
        usage(argv[0]);
    }
  }
}

extern char automata_code[];

bracz_custom::AutomataControl automatas(stack.node(), stack.dg_service(), (const insn_t*) automata_code);


int appl_main(int argc, char *argv[]) {
  parse_args(argc, argv);

  if (device_path) {
    stack.add_gridconnect_port(device_path);
  } else {
    stack.connect_tcp_gridconnect_hub(host, port);
  }

  stack.loop_executor();
  return 0;
}
