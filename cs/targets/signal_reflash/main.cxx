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
 * An application that sends an OpenLCB datagram from command line.
 *
 * @author Balazs Racz
 * @date 27 Feb 2015
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include <memory>

#include "os/os.h"
#include "utils/constants.hxx"
#include "utils/Hub.hxx"
#include "utils/GridConnectHub.hxx"
#include "utils/GcTcpHub.hxx"
#include "utils/Crc.hxx"
#include "executor/Executor.hxx"
#include "executor/Service.hxx"

#include "nmranet/IfCan.hxx"
#include "nmranet/DatagramCan.hxx"
#include "nmranet/BootloaderClient.hxx"
#include "nmranet/If.hxx"
#include "nmranet/AliasAllocator.hxx"
#include "nmranet/DefaultNode.hxx"
#include "utils/socket_listener.hxx"

#include "freertos/bootloader_hal.h"
#include "src/base.h"

NO_THREAD nt;
Executor<1> g_executor(nt);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);

static const nmranet::NodeID NODE_ID = 0x05010101181EULL;

nmranet::IfCan g_if_can(&g_executor, &can_hub0, 3, 3, 2);
nmranet::CanDatagramService g_datagram_can(&g_if_can, 10, 2);
static nmranet::AddAliasAllocator g_alias_allocator(NODE_ID, &g_if_can);
nmranet::DefaultNode g_node(&g_if_can, NODE_ID);

namespace nmranet {
Pool *const g_incoming_datagram_allocator = mainBufferPool;
}

int port = 12021;
const char *host = "localhost";
const char *device_path = nullptr;
uint64_t destination_nodeid = 0;
unsigned destination_alias = 0;
string payload_string;
const char *filename = nullptr;

unsigned signal_address = 0;
unsigned signal_offset = 0;
const unsigned FLAGS_signal_flash_sleep = 50000;

bool noreset = false;


void usage(const char *e) {
  fprintf(stderr,
          "Usage: %s ([-i destination_host] [-p port] | [-d device_path]) "
          "(-n nodeid | -a alias) [-k] -f filename -s signal_address -o offset\n",
          e);
  fprintf(stderr,
          "Connects to an openlcb bus and sends a datagram to a "
          "specific node on the bus.\n");
  fprintf(stderr,
          "The bus connection will be through an OpenLCB HUB on "
          "destination_host:port with OpenLCB over TCP "
          "(in GridConnect format) protocol, or through the CAN-USB device "
          "(also in GridConnect protocol) found at device_path. Device takes "
          "precedence over TCP host:port specification.");
  fprintf(stderr, "The default target is localhost:12021.\n");
  fprintf(stderr,
          "\nnodeid should be a 12-char hex string with 0x prefix "
          "and no separators, like '-b 0x05010101141F'\n");
  fprintf(stderr,
          "\nalias should be a 3-char hex string with 0x prefix and "
          "no separators, like '-a 0x3F9'\n");
  fprintf(stderr,
          "\n-k will prevent resetting the signals before flashing.\n");
  fprintf(stderr, "\nfilename contains the binary to flash.\n");
  fprintf(stderr,
          "\nsignal_address is the decimal or hex address of the signal on the "
          "bus to flash. example -s 31 or -s 0x1F.\n");
  fprintf(stderr, "\noffset is the offset at which to start flashing.\n");

  exit(1);
}

void parse_args(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, "hi:p:d:n:a:g:f:s:o:k")) >= 0) {
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
      case 'n':
        destination_nodeid = strtoll(optarg, nullptr, 16);
        break;
      case 'a':
        destination_alias = strtoul(optarg, nullptr, 16);
        break;
      case 'f':
        filename = optarg;
        break;
      case 's':
        signal_address = strtoul(optarg, nullptr, 0);
        break;
      case 'o':
        signal_offset = strtoul(optarg, nullptr, 0);
        break;
      case 'k':
        noreset = true;
        break;
      default:
        fprintf(stderr, "Unknown option %c\n", opt);
        usage(argv[0]);
    }
  }
  if (!filename || (!destination_nodeid && !destination_alias)) {
    usage(argv[0]);
  }
}

using nmranet::DatagramPayload;
using nmranet::DatagramClient;
using nmranet::Defs;
using nmranet::NodeHandle;

SyncNotifiable n;
BarrierNotifiable bn;

void send_datagram(const string &dg) {
  DatagramClient *client = g_datagram_can.client_allocator()->next_blocking();

  Buffer<nmranet::NMRAnetMessage> *b;
  mainBufferPool->alloc(&b);

  NodeHandle dst;
  dst.alias = destination_alias;
  dst.id = destination_nodeid;
  b->data()->reset(Defs::MTI_DATAGRAM, g_node.node_id(), dst, dg);
  b->set_done(bn.reset(&n));
  client->write_datagram(b);
  n.wait_for_notification();
  g_datagram_can.client_allocator()->typed_insert(client);
}

void send_signal_dg(uint8_t cmd, const string& arg = "") {
  string dg;
  dg.push_back(0x2F);
  dg.push_back(cmd);
  dg += arg;
  send_datagram(dg);
}

void send_packet(const string &packet) {
  send_signal_dg(0x10, packet);
}

class Crc32 {
 public:
  typedef uint32_t crc32_t;

  Crc32() {
    state_ = 0xFFFFFFFF;
  }

  void Add(uint8_t data) {
    state_ ^= ((crc32_t)data) << 24;
    for (uint8_t b = 8; b > 0; --b) {
      if (state_ & (1UL<<31)) {
        state_ = (state_ << 1) ^ 0x04C11DB7;
      } else {
        state_ <<= 1;
      }
    }
  }

  crc32_t Get() {
    state_ ^= 0xFFFFFFFF;
    return state_;
  }

 private:
  crc32_t state_;
};

void FlashSignal(unsigned request_offset, const string& data) {
  LOG(INFO, "Flashing %u bytes at offset 0x%x", data.size(), request_offset);
  const int kNumBytesPerRequest = 8;
  const int kNumBytesPerRow = 32;
  unsigned current = 0;
  send_signal_dg(CMD_SIGNAL_PAUSE);

  // resets the signal to flash
  if (!noreset) {
    string s;
    s.push_back(signal_address);
    s.push_back(3);
    s.push_back(SCMD_RESET);
    s.push_back(0x55);
    send_packet(s);
    usleep(10000);
  }

  while (current < data.size()) {
    LOG(INFO, "Writing at offset %x", current);
    for (int i = 0; i < kNumBytesPerRow/kNumBytesPerRequest; ++i) {
      string s;
      s.push_back(SCMD_FLASH);
      int offset = (request_offset + current) >> 1;
      s.push_back(offset & 0xff);
      s.push_back((offset>>8) & 0xff);
      for (int j = 0; j < kNumBytesPerRequest; ++j) {
        if (current + j < data.size()) {
          s.push_back(data[current+j]);
        } else {
          s.push_back(0xff);
        }
      }
      string k;
      k.push_back(signal_address);
      k.push_back(s.size() + 1);
      usleep(FLAGS_signal_flash_sleep);
      send_packet(k + s);
      current += kNumBytesPerRequest;
    } // for requests
  } // for rows.

  Crc32 crc;
  for (unsigned i = 0; i < data.size(); ++i) {
    if (i & 1) {
      crc.Add(data[i] & 0x3f);
    } else {
      crc.Add(data[i]);
    }
  }
  string s;
  s.push_back(SCMD_CRC);
  s.push_back((request_offset >> 1) & 0xff);
  s.push_back((request_offset >> 9) & 0xff);
  s.push_back(data.size() & 0xff);
  s.push_back((data.size()>>8) & 0xff);
  Crc32::crc32_t c = crc.Get();
  s.push_back((c >> 0) & 0xff);
  s.push_back((c >> 8) & 0xff);
  s.push_back((c >> 16) & 0xff);
  s.push_back((c >> 24) & 0xff);
  string k;
  k.push_back(signal_address);
  k.push_back(s.size() + 1);
  usleep(10000);
  send_packet(k + s);

  //send_signal_dg(CMD_SIGNAL_RESUME);
}


string read_file_to_string(const char *filename) {
  string d;
  FILE *f = fopen(filename, "rb");
  if (!f) {
    fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
    exit(1);
  }
  char buf[1024];
  size_t nr;
  while ((nr = fread(buf, 1, sizeof(buf), f)) > 0) {
    d.append(buf, nr);
  }
  fclose(f);
  return d;
}

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[]) {
  parse_args(argc, argv);
  int conn_fd = 0;
  if (device_path) {
    conn_fd = ::open(device_path, O_RDWR);
  } else {
    conn_fd = ConnectSocket(host, port);
  }
  HASSERT(conn_fd >= 0);
  create_gc_port_for_can_hub(&can_hub0, conn_fd);

  g_if_can.add_addressed_message_support();
  // Bootstraps the alias allocation process.
  g_if_can.alias_allocator()->send(g_if_can.alias_allocator()->alloc());

  g_executor.start_thread("g_executor", 0, 1024);

  // Waits for stack to be up.
  while (!g_node.is_initialized()) usleep(10000);
  LOG(INFO, "Node initialized.");

  string hex = read_file_to_string(filename);

  string payload;
  unsigned ofs = 0;
  while (ofs + 2 <= hex.size()) {
    payload.push_back(
        strtoul(hex.substr(ofs, 2).c_str(), nullptr, 16));
    ofs += 2;
  }

  FlashSignal(signal_offset, payload);

  exit(0);  // do not call destructors.
  return 0;
}
