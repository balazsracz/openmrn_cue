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
#include "can_frame.h"
#include "nmranet_config.h"

#include "os/TempFile.hxx"
#include "nmranet/SimpleStack.hxx"
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"
#include "utils/socket_listener.hxx"
#include "utils/StringPrintf.hxx"
#include "server/TrainControlService.hxx"

static const nmranet::NodeID NODE_ID = 0x050101011472ULL;
OVERRIDE_CONST(num_memory_spaces, 4);
OVERRIDE_CONST(gc_generate_newlines, 1);
int port = 12021;
const char *host = "localhost";
const char *device_path = nullptr;

int proxy_port = 50001;
const char *proxy_host = "28k.ch";
uint64_t destination_nodeid = 0;
uint16_t destination_alias = 0;
const char *lokdb_path;
string lokdb = "LokDb { }";

nmranet::SimpleCanStack stack(NODE_ID);

nmranet::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char *const nmranet::SNIP_DYNAMIC_FILENAME =
    nmranet::MockSNIPUserFile::snip_user_file_path;

namespace nmranet {
const SimpleNodeStaticValues SNIP_STATIC_DATA = {
    4, "OpenMRN", "Host server", "No hardware here", "0.92"};
}  // namespace nmranet

server::TrainControlService control_server(stack.executor());

void usage(const char *e) {
  fprintf(stderr, "Usage: %s", e);
  fprintf(stderr, R"usage(
 [-i destination_host] [-p port] [-d device_path] -l lokdb_path
            (-n nodeid | -a alias) [-R proxy_host] [-P proxy_port]

Host server for android train connections.

The bus connection will be through an OpenLCB HUB on destination_host:port with
OpenLCB over TCP (in GridConnect format) protocol, and/or through the CAN-USB
device (also in GridConnect protocol) found at device_path. If both are
specified, then the two connections will be bridged.

        -i destination_host -p port specifies the connection to GridConnect CAN
           hub over TCP. This defaults to localhost:12021. Specify port=-1 to
           disable.

        -n 0x0501010114FF or -a 0x123 defines the node ID or the alias to reach
           the command station through.

        -R host -P port specifies the location of the proxy server for the
           android connections.

        -l lokdb_path specifies the file from which to read the ascii lokdb
           from.
)usage");
  exit(1);
}

void parse_args(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, "hp:i:d:n:a:R:P:l:")) >= 0) {
    switch (opt) {
      case 'h':
        usage(argv[0]);
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'i':
        host = optarg;
        break;
      case 'P':
        proxy_port = atoi(optarg);
        break;
      case 'R':
        proxy_host = optarg;
        break;
      case 'd':
        device_path = optarg;
        break;
      case 'l':
        lokdb_path = optarg;
        break;
      case 'n':
        destination_nodeid = strtoll(optarg, nullptr, 16);
        break;
      case 'a':
        destination_alias = strtoul(optarg, nullptr, 16);
        break;
      default:
        fprintf(stderr, "Unknown option %c\n", opt);
        usage(argv[0]);
    }
  }
}

void load_lokdb(const char *filename) {
  int fd = ::open(filename, O_RDONLY);
  if (fd < 0) {
    LOG(FATAL, "Failed to open lokdb file %s.", filename);
    return;
  }
  string data;
  char buf[1000];
  do {
    int rd = ::read(fd, buf, sizeof(buf));
    if (rd <= 0) break;
    data.append(buf, rd);
  } while (true);
  lokdb = data;
}

class CrashExitNotifiable : public Notifiable {
 public:
  CrashExitNotifiable(const string &msg) : msg_(msg) {}

  void notify() OVERRIDE {
    LOG(FATAL, "%s", msg_.c_str());
    HASSERT(0);
  }

 private:
  const string msg_;
};

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[]) {
  parse_args(argc, argv);
  if (lokdb_path) {
    LOG(INFO, "Loading lokdb from %s.", lokdb_path);
    load_lokdb(lokdb_path);
  }
  // stack.print_all_packets();
  if (port >= 0) {
    stack.connect_tcp_gridconnect_hub(host, port);
  }
  if (device_path) {
    stack.add_gridconnect_tty(
        device_path, new CrashExitNotifiable(StringPrintf(
                         "Connection to device %s terminated.", device_path)));
  }
  control_server.initialize(
      stack.dg_service(), stack.node(),
      nmranet::NodeHandle(destination_nodeid, destination_alias), lokdb);
  int proxy_fd = ConnectSocket(proxy_host, proxy_port);
  if (proxy_fd < 0) {
    DIE("Could not connect to proxy.");
  }
  control_server.set_channel(proxy_fd, proxy_fd);
  server::PacketStreamKeepalive keepalive(&control_server,
                                          control_server.reply_target());
  LOG(INFO, "Stack initalized.");

  stack.loop_executor();
  return 0;
}
