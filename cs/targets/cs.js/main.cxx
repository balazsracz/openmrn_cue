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

#include "commandstation/AllTrainNodes.hxx"
#include "commandstation/TrainDb.hxx"
#include "commandstation/UpdateProcessor.hxx"
#include "custom/LoggingBit.hxx"
#include "dcc/FakeTrackIf.hxx"
#include "executor/PoolToQueueFlow.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "openlcb/SimpleNodeInfoMockUserFile.hxx"
#include "openlcb/SimpleStack.hxx"
#include "openlcb/TractionCvSpace.hxx"
#include "openlcb/TractionTestTrain.hxx"
#include "openlcb/TractionTrain.hxx"
#include "utils/JSSerialPort.hxx"
#include "utils/JSTcpClient.hxx"
#include "utils/JSTcpHub.hxx"

static const openlcb::NodeID NODE_ID = 0x050101011440ULL;
openlcb::SimpleCanStack stack(NODE_ID);
OVERRIDE_CONST(num_memory_spaces, 12);
OVERRIDE_CONST(local_nodes_count, 0);
OVERRIDE_CONST(local_alias_cache_size, 5000);
OVERRIDE_CONST(remote_alias_cache_size, 5000);
// Forces all trains to be our responsibility.
OVERRIDE_CONST(mobile_station_train_count, 0);
OVERRIDE_CONST(gc_generate_newlines, 1);

namespace commandstation {
extern const struct const_traindb_entry_t const_lokdb[];
const struct const_traindb_entry_t const_lokdb[] = {
    // 0
    //  { 51, { LIGHT, TELEX, FN_NONEXISTANT, SHUNT, ABV },
    //  "BR 260417", DCC_28 },  // ESU LokPilot 3.0
    // 1
    //{ 66, { LIGHT },
    //"Re 6/6 11665", DCC_128 },
    {31,
     {
         LIGHT,
         FN_NONEXISTANT,
         TELEX,
         ABV,
     },
     "F7 31",
     DCCMODE_FAKE_DRIVE},
    {123,
     {
         LIGHT,
         FN_NONEXISTANT,
         TELEX,
         ABV,
     },
     "Example 123",
     DCC_128},

    {7000, {0xff}, "Node test 7000", DCC_28},
    {5000, {0xff}, "TT 5000", DCC_28},
    {5104, {0xff}, "TT 5104", DCC_28},
    {5100, {0xff}, "TT 5100", DCC_28},
    {13, {0xff}, "MM 13", MARKLIN_NEW},
    {15, {0xff}, "MM 15", MARKLIN_NEW},
    {0,
     {
         0,
     },
     "",
     0},
    {0,
     {
         0,
     },
     "",
     0},
};
extern const size_t const_lokdb_size;
const size_t const_lokdb_size = sizeof(const_lokdb) / sizeof(const_lokdb[0]);
}  // namespace commandstation

int port = -1;
std::vector<string> device_paths;
int upstream_port = 12021;
const char *upstream_host = nullptr;
bool print_packets = false;
bool have_pgmtrack = false;
/// If nonzero, we are sending heartbeat requests for all trains.
unsigned heartbeat_sec = 0;

void usage(const char *e) {
  fprintf(stderr,
          "Usage: %s [-l] [-g] [-b heartbeat_secs] [-p port] [-d device_path] [-u upstream_host [-q "
          "upstream_port]]\n\n",
          e);
  fprintf(stderr, "Fake command station.\n\nArguments:\n");
  fprintf(stderr,
          "\t-l     Prints all CAN packets to the console.\n");
  fprintf(stderr,
          "\t-g     Simulates a programming track.\n");
  fprintf(stderr,
          "\t-b num     Sends out heartbeats to throttles every `num` seconds.\n");
  fprintf(stderr,
          "\t-p port     If specified, listens on this port number for "
          "GridConnect TCP connections.\n");
  fprintf(stderr,
          "\t-d device   is a path to a physical device doing "
          "serial-CAN or USB-CAN. If specified, opens device and "
          "adds it to the hub.\n");
  fprintf(stderr, "\t-D lists available serial ports.\n");
  fprintf(stderr,
          "\t-u upstream_host   is the host name for an upstream "
          "hub. If specified, this hub will connect to an upstream "
          "hub.\n");
  fprintf(stderr,
          "\t-q upstream_port   is the port number for the upstream hub.\n");
  exit(1);
}

void parse_args(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, "hgp:u:q:d:lb:D")) >= 0) {
    switch (opt) {
      case 'h':
        usage(argv[0]);
        break;
      case 'd':
        device_paths.push_back(optarg);
        break;
      case 'D':
        JSSerialPort::list_ports();
        break;
      case 'g':
        have_pgmtrack = true;
        break;
      case 'b':
        heartbeat_sec = atoi(optarg);
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'u':
        upstream_host = optarg;
        break;
      case 'q':
        upstream_port = atoi(optarg);
        break;
      case 'l':
        print_packets = true;
        break;
      default:
        fprintf(stderr, "Unknown option %c\n", opt);
        usage(argv[0]);
    }
  }
}

openlcb::MockSNIPUserFile snip_user_file("Fake CS", "");
const char *const openlcb::SNIP_DYNAMIC_FILENAME =
    openlcb::MockSNIPUserFile::snip_user_file_path;

dcc::FakeTrackIf track_if(stack.service(), 2);
commandstation::UpdateProcessor cs_loop(stack.service(), &track_if);
PoolToQueueFlow<Buffer<dcc::Packet>> pool_translator(stack.service(),
                                                     track_if.pool(), &cs_loop);

LoggingBit track_on_off(openlcb::Defs::EMERGENCY_OFF_EVENT,
                        openlcb::Defs::CLEAR_EMERGENCY_OFF_EVENT,
                        "emergency_stop");
openlcb::BitEventConsumer powerbit(&track_on_off);

openlcb::TrainService traction_service(stack.iface());

commandstation::TrainDb train_db;
commandstation::AllTrainNodes all_train_nodes(&train_db, &traction_service,
                                              stack.info_flow(),
                                              stack.memory_config_handler());

/*
dcc::RailcomHubFlow railcom_hub(stack.service());
openlcb::TractionCvSpace traction_cv(stack.memory_config_handler(), &track_if,
                                     &railcom_hub,
                                     openlcb::MemoryConfigDefs::SPACE_DCC_CV);
*/

class ReinitImpl : public ::Timer {
 public:
  ReinitImpl() : ::Timer(stack.executor()->active_timers()) {
    start(MSEC_TO_NSEC(100));
  }

  long long timeout() override {
    if (needReinit_ && stack.node()->is_initialized()) {
      needReinit_ = false;
      stack.if_can()->send_global_alias_enquiry(stack.node());
      // Sends out initialization completes. We could call
      // stack.restart_stack() but we'd rather not throw away all
      // the aliases because we typically have a lot of them.
      new openlcb::ReinitAllNodes(stack.if_can());
    }
    return RESTART;
  }

  bool needReinit_{false};

} reinitImpl;

class HeartbeatTimer : public ::Timer {
 public:
  HeartbeatTimer() : ::Timer(stack.executor()->active_timers()) {
  }

  long long timeout() {
    // iterates over all train nodes.
    for (openlcb::Node *n = stack.iface()->first_local_node(); n;
         n = stack.iface()->next_local_node(n->node_id())) {
      if (!traction_service.is_known_train_node(n)) {
        continue;
      }
      openlcb::TrainNode* tn = (openlcb::TrainNode*)n;
      auto ctrl = tn->get_controller();
      if (!ctrl.id && !ctrl.alias) {
        // no controller
        continue;
      }
      if (static_cast<int>(tn->train()->get_speed().mph() + 0.5) == 0) {
        // not moving
        continue;
      }
      // Sends heartbeat
      openlcb::send_message(
          n, openlcb::Defs::MTI_TRACTION_CONTROL_REPLY, ctrl,
          openlcb::TractionDefs::heartbeat_request_payload(heartbeat_sec));
    }
    return RESTART;
  }
} heartbeatTimer_;

void connect_callback() { reinitImpl.needReinit_ = true; }

class FakePgmTrack : public openlcb::MemorySpace, private ::Timer {
 public:
  FakePgmTrack() : ::Timer(stack.executor()->active_timers()) {
    /// Initializes storage with some random data.
    for (unsigned i = 0; i < sizeof(storage_); i++) {
      storage_[i] = ((i+1) % 100 + 100);
    }
    storage_[OFS_ERR] = 0;
    storage_[OFS_ERR+1] = 0;
    storage_[OFS_DELAY] = 1;
    stack.memory_config_handler()->registry()->insert(
        nullptr, openlcb::MemoryConfigDefs::SPACE_DCC_CV, this);
  }

  bool read_only() override { return false; }

    /** @returns the largest valid address for this block.  A read of 1 from
     *  this address should succeed in returning the last byte.
     */
  address_t max_address() override {
    return 1023;
  }

  size_t write(address_t destination, const uint8_t *data, size_t len,
               errorcode_t *error, Notifiable *again) override {
    // Checks if we need to wait first.
    if (!check_wait(destination, error, again)) {
      return 0;
    }
    // Execute. We only write 1 byte at a time.
    storage_[destination] = *data;
    return 1;
  }

  size_t read(address_t source, uint8_t *dst, size_t len, errorcode_t *error,
              Notifiable *again) override {
    // Checks if we need to wait first.
    if (!check_wait(source, error, again)) {
      return 0;
    }
    // Execute. We only read 1 byte at a time.
    *dst = storage_[source];
    return 1;
  }
  
 private:
  /// Performs bounds check, performs wait and handling of stored error codes.
  /// @return true if processing should continue, false to return.
  bool check_wait(address_t destination, errorcode_t *error, Notifiable *again) {
    if (destination >= sizeof(storage_)) {
      *error = openlcb::MemoryConfigDefs::ERROR_OUT_OF_BOUNDS;
      return false;
    }
    if (destination == OFS_ERR || destination == OFS_ERR + 1 ||
        destination == OFS_DELAY) {
      // These should be executed immediately.
      return true;
    }
    // Load the error persisted.
    auto err = openlcb::data_to_error(storage_ + OFS_ERR);    
    if (!waiting_) {
      waiting_ = true;
      HASSERT(waitDone_ == nullptr);
      waitDone_ = again;
      *error = ERROR_AGAIN;
      start(MSEC_TO_NSEC(delay_msec()));
      return false;
    } else if (err) {
      // wait done but we have an error to return.
      waiting_ = false;
      *error = err;
      return false;
    } else {
      // Wait done + success.
      waiting_ = false;
      return true;
    }
  }

  /// Timer callback
  long long timeout() override {
    if (waitDone_) {
      auto n = waitDone_;
      waitDone_ = nullptr;
      n->notify();
    }
    return NONE;
  }

  /// @return how much delay should we set for each operation.
  unsigned delay_msec() { return storage_[OFS_DELAY] * 100; }

  /// At this CV (0-based) there is an openlcb error that will get injected for
  /// any operations. Two bytes, order is Hi, Lo.
  static constexpr unsigned OFS_ERR = 1020-1;
  /// At this CV (0-based) there is a number on how long each operation is
  /// going to take. Unit is 100 msec.
  static constexpr unsigned OFS_DELAY = 1022-1;
  /// Backing store for CVs.
  uint8_t storage_[1024];
  /// Set to the notifiable that needs to be called after wait.
  Notifiable* waitDone_ = nullptr;
  /// True if we 
  bool waiting_{false};
};


/// Instantiates the fake programming track object.
void create_pgmtrack() {
  new FakePgmTrack();
  new openlcb::FixedEventProducer<0x090099FEFFFF0000U | 2>(stack.node());
}

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[]) {
  parse_args(argc, argv);
  if (have_pgmtrack) {
    create_pgmtrack();
  }
  if (heartbeat_sec) {
    heartbeatTimer_.start(SEC_TO_NSEC(heartbeat_sec));
  }
  std::unique_ptr<JSTcpHub> hub;
  if (port >= 0) {
    hub.reset(new JSTcpHub(stack.can_hub(), port));
  }
  std::unique_ptr<JSTcpClient> client;
  if (upstream_host) {
    client.reset(new JSTcpClient(stack.can_hub(), upstream_host, upstream_port,
                                 &connect_callback));
  }
  std::vector<std::unique_ptr<JSSerialPort>> devices;
  for (auto &d : device_paths) {
    devices.emplace_back(new JSSerialPort(stack.can_hub(), d, &connect_callback));
  }
  if (print_packets) {
    stack.print_all_packets();
  }
  stack.loop_executor();
  return 0;
}
