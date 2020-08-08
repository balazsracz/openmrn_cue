#include "commandstation/AllTrainNodes.hxx"
#include "commandstation/UpdateProcessor.hxx"
#include "commandstation/TrainDb.hxx"
#include "utils/async_traction_test_helper.hxx"
#include "openlcb/SimpleInfoProtocol.hxx"
#include "openlcb/SimpleNodeInfoMockUserFile.hxx"
#include "openlcb/ConfigUpdateFlow.hxx"
#include "openlcb/DatagramCan.hxx"
#include "dcc/PacketFlowInterface.hxx"

openlcb::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char* const openlcb::SNIP_DYNAMIC_FILENAME =
    openlcb::MockSNIPUserFile::snip_user_file_path;

namespace openlcb {
extern Pool* const g_incoming_datagram_allocator = init_main_buffer_pool();
}

namespace commandstation {

class DccPacketSink : public dcc::PacketFlowInterface {
 public:
  void send(Buffer<dcc::Packet>* b, unsigned prio) {
    b->unref();
  }
};

// These components of the command station are needed in order to instantiate
// real dcc train impl objects.
DccPacketSink g_hardware;
UpdateProcessor g_update_processor{&g_service, &g_hardware};

const struct const_traindb_entry_t const_lokdb[] = {
    // 0
    {43,
     {
      LIGHT, TELEX, FNT11, ABV, 0xff,
     },
     "Am 843 093-6",
     DCC_128_LONG_ADDRESS},
    {22,
     {
      LIGHT, FNT11, ABV, 0xff,
     },
     "RE 460 TSR",
     DCC_128_LONG_ADDRESS},  // todo: there is no beamer here // LD-32 decoder
    {465,
     {
      LIGHT, SPEECH, 0xff,
     },
     "Jim's steam",
     DCC_128_LONG_ADDRESS},
    {0,
     {
      0,
     },
     "",
     0},
};

extern const size_t const_lokdb_size =
    sizeof(const_lokdb) / sizeof(const_lokdb[0]);

extern const char TRAINCDI_DATA[] = "Test cdi data";
extern const size_t TRAINCDI_SIZE = sizeof(TRAINCDI_DATA);
extern const char TRAINTMPCDI_DATA[] = "Test cdi tmp data";
extern const size_t TRAINTMPCDI_SIZE = sizeof(TRAINTMPCDI_DATA);



class AllTrainNodesTestBase : public openlcb::TractionTest {
 protected:
  AllTrainNodesTestBase() {
    // create_allocated_alias();
    inject_allocated_alias(0x440);
    inject_allocated_alias(0x441);
    inject_allocated_alias(0x442);
    inject_allocated_alias(0x443);
    inject_allocated_alias(0x33A);
    inject_allocated_alias(0x33B);
  }

  void wait() {
    wait_for_event_thread();
    openlcb::TractionTest::wait();
  }

  void expect_train_start(openlcb::NodeAlias alias, int addr, dcc::TrainAddressType type = dcc::TrainAddressType::DCC_LONG_ADDRESS) {
    openlcb::NodeID address = openlcb::TractionDefs::train_node_id_from_legacy(type, addr);
    expect_packet(StringPrintf(":X10701%03XN%012" PRIX64 ";", alias, address));
    expect_packet(StringPrintf(":X19100%03XN%012" PRIX64 ";", alias, address));
    expect_packet(
        StringPrintf(":X19547%03XN0101000000000303;", alias));
    expect_packet(
        StringPrintf(":X19524%03XN090099FF00000000;", alias));
  }

  /// Sends an addressed message to the bus. Performs
  /// synchronous (dynamic) memory allocation so use it sparingly and when
  /// there is sufficient amount of RAM available.
  /// @param mti is the message to send
  /// @param dst is the node to send message to.
  /// @param payload is the contents of the message
  void send_message_to(openlcb::Defs::MTI mti, openlcb::NodeHandle dst,
                       const string& payload = openlcb::EMPTY_PAYLOAD) {
    auto *b = node_->iface()->addressed_message_write_flow()->alloc();
    b->data()->reset(mti, node_->node_id(), dst, payload);
    node_->iface()->addressed_message_write_flow()->send(b);
  }

  openlcb::ConfigUpdateFlow updateFlow_{ifCan_.get()};
  openlcb::SimpleInfoFlow infoFlow_{&g_service};
  openlcb::CanDatagramService datagramService_{ifCan_.get(), 5, 2};
  openlcb::MemoryConfigHandler memoryConfigHandler_{&datagramService_, nullptr,
                                                    5};
  std::unique_ptr<AllTrainNodesInterface> trainNodes_;
};

class AllTrainNodesTest : public AllTrainNodesTestBase {
 protected:
  AllTrainNodesTest() {
    wait();
    // print_all_packets();
    expect_train_start(0x440, const_lokdb[0].address);
    expect_train_start(0x441, const_lokdb[1].address);
    expect_train_start(0x442, const_lokdb[2].address);
    BlockExecutor b(nullptr);
    trainNodes_.reset(new AllTrainNodes{&trainDb_, &trainService_, &infoFlow_,
	  &memoryConfigHandler_});
    b.release_block();
    // TODO: there is a race condition here but I'm not sure why.
    wait();
  }

  ~AllTrainNodesTest() { wait(); }

  TrainDb trainDb_;
};

// Test helper class that uses storage-based train DB.
class StoredTrainNodesTest : public AllTrainNodesTestBase {
 protected:
  StoredTrainNodesTest() {
    string empty;
    empty.resize(cfg_.size());
    configFile_.write(empty);
  }

  void start() {
    updateFlow_.TEST_set_fd(configFile_.fd());
    wait();
    // print_all_packets();
    expect_train_start(0x440, const_lokdb[0].address);
    expect_train_start(0x441, const_lokdb[1].address);
    expect_train_start(0x442, const_lokdb[2].address);
    BlockExecutor b(nullptr);
    trainNodes_.reset(new AllTrainNodes{&trainDb_, &trainService_, &infoFlow_,
	  &memoryConfigHandler_});
    b.release_block();
    // TODO: there is a race condition here but I'm not sure why.
    wait();
    wait();
  }
  
  ~StoredTrainNodesTest() { wait(); }

  TrainDbConfig cfg_{0};
  TrainDb trainDb_{cfg_};
  TempFile configFile_{*TempDir::instance(), "db_cdi_file"};
};


} // namespace commandstation
