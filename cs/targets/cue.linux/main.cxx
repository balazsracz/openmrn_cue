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
#include <termios.h> /* tc* functions */

#include "nmranet/SimpleStack.hxx"
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"
#include "custom/AutomataControl.hxx"
#include "utils/Ewma.hxx"

using nmranet::MemoryConfigDefs;
using nmranet::DatagramDefs;
using nmranet::DatagramClient;

static const nmranet::NodeID NODE_ID = 0x05010101143EULL;
nmranet::SimpleCanStack stack(NODE_ID);

nmranet::MockSNIPUserFile snip_user_file(
    "Signal logic", "This node is loaded with the signaling logic");
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

bracz_custom::AutomataControl automatas(stack.node(), stack.dg_service(),
                                        (const insn_t *)automata_code);

class ConnectionClient {
 public:
  /** Test the connection whether it is alive; establish the connection if it
   * is dead. */
  virtual void ping() = 0;
};

class DeviceClosedNotify : public Notifiable {
 public:
  DeviceClosedNotify(int *fd, string name) : fd_(fd), name_(name) {}
  void notify() override {
    LOG_ERROR("Connection to %s closed.", name_.c_str());
    *fd_ = -1;
  }

 private:
  int *fd_;
  string name_;
};

class FdConnectionClient : public ConnectionClient {
 public:
  FdConnectionClient(const string &name) : closedNotify_(&fd_, name) {}

  void ping() OVERRIDE {
    if (fd_ < 0) {
      try_connect();
    }
  }

 protected:
  virtual void try_connect() = 0;

  int fd_{-1};
  DeviceClosedNotify closedNotify_;
};

class DeviceConnectionClient : public FdConnectionClient {
 public:
  DeviceConnectionClient(const string &name, const string &dev)
      : FdConnectionClient(name), dev_(dev) {}

 private:
  void try_connect() OVERRIDE {
    fd_ = ::open(dev_.c_str(), O_RDWR);
    if (fd_ >= 0) {
      // Sets up the terminal in raw mode. Otherwise linux might echo
      // characters coming in from the device and that will make
      // packets go back to where they came from.
      HASSERT(!tcflush(fd_, TCIOFLUSH));
      struct termios settings;
      HASSERT(!tcgetattr(fd_, &settings));
      cfmakeraw(&settings);
      cfsetspeed(&settings, B115200);
      HASSERT(!tcsetattr(fd_, TCSANOW, &settings));
      LOG(INFO, "Opened device %s.\n", device_path);
      create_gc_port_for_can_hub(stack.can_hub(), fd_, &closedNotify_);
    } else {
      LOG_ERROR("Failed to open device %s: %s\n", device_path, strerror(errno));
      fd_ = -1;
    }
  }

  string dev_;
};

class UpstreamConnectionClient : public FdConnectionClient {
 public:
  UpstreamConnectionClient(const string &name, const string &host, int port)
      : FdConnectionClient(name), host_(host), port_(port) {}

 private:
  void try_connect() OVERRIDE {
    fd_ = ConnectSocket(host_.c_str(), port_);
    if (fd_ >= 0) {
      LOG_ERROR("Connected to %s:%d\n", host_.c_str(), port_);
      create_gc_port_for_can_hub(stack.can_hub(), fd_, &closedNotify_);
    } else {
      LOG_ERROR("Failed to connect to %s:%d: %s\n", host_.c_str(), port_,
                strerror(errno));
      fd_ = -1;
    }
  }

  string host_;
  int port_;
};

int appl_main(int argc, char *argv[]) {
  parse_args(argc, argv);

  vector<std::unique_ptr<ConnectionClient>> connections;

  if (device_path) {
    connections.emplace_back(new DeviceConnectionClient("device", device_path));
  } else {
    connections.emplace_back(
        new UpstreamConnectionClient("upstream", host, port));
  }

  stack.start_executor_thread("stack", 0, 0);

  while (1) {
    for (const auto &p : connections) {
      p->ping();
    }
    sleep(1);
  }

  return 0;
}
