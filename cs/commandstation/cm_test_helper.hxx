#include "commandstation/AllTrainNodes.hxx"
#include "commandstation/TrainDb.hxx"
#include "utils/async_traction_test_helper.hxx"

#include "nmranet/SimpleInfoProtocol.hxx"
#include "nmranet/SimpleNodeInfoMockUserFile.hxx"
#include "nmranet/ConfigUpdateFlow.hxx"
#include "nmranet/DatagramCan.hxx"
#include "dcc/PacketFlowInterface.hxx"

nmranet::MockSNIPUserFile snip_user_file("Default user name",
                                         "Default user description");
const char* const nmranet::SNIP_DYNAMIC_FILENAME =
    nmranet::MockSNIPUserFile::snip_user_file_path;

namespace nmranet {
extern Pool* const g_incoming_datagram_allocator = init_main_buffer_pool();
}

namespace commandstation {

const struct const_traindb_entry_t const_lokdb[] = {
    // 0
    {43,
     {
      LIGHT, TELEX, FNT11, ABV, 0xff,
     },
     "Am 843 093-6",
     FAKE_DRIVE},
    {22,
     {
      LIGHT, FNT11, ABV, 0xff,
     },
     "RE 460 TSR",
     FAKE_DRIVE},  // todo: there is no beamer here // LD-32 decoder
    {465,
     {
      LIGHT, SPEECH, 0xff,
     },
     "Jim's steam",
     FAKE_DRIVE | PUSHPULL},
    {0,
     {
      0,
     },
     "",
     0},
};

extern const size_t const_lokdb_size =
    sizeof(const_lokdb) / sizeof(const_lokdb[0]);

class DccPacketSink : public dcc::PacketFlowInterface {
 public:
  void send(Buffer<dcc::Packet>* b, unsigned prio) {
    b->unref();
  }
};

extern const char TRAINCDI_DATA[] = "Test cdi data";
extern const size_t TRAINCDI_SIZE = sizeof(TRAINCDI_DATA);
extern const char TRAINTMPCDI_DATA[] = "Test cdi tmp data";
extern const size_t TRAINTMPCDI_SIZE = sizeof(TRAINTMPCDI_DATA);



class AllTrainNodesTestBase : public nmranet::TractionTest {
 protected:
  AllTrainNodesTestBase() {
    // create_allocated_alias();
    inject_allocated_alias(0x440);
    inject_allocated_alias(0x441);
    inject_allocated_alias(0x442);
    inject_allocated_alias(0x443);
    inject_allocated_alias(0x33A);
  }

  nmranet::ConfigUpdateFlow cfgflow{ifCan_.get()};
};

class AllTrainNodesTest : public AllTrainNodesTestBase {
 protected:
  AllTrainNodesTest() {
    // print_all_packets();
    expect_train_start(0x440, const_lokdb[0].address);
    expect_train_start(0x441, const_lokdb[1].address);
    expect_train_start(0x442, const_lokdb[2].address);
    trainNodes_.reset(new AllTrainNodes{&trainDb_, &trainService_, &infoFlow_,
                                        &memoryConfigHandler_});
    wait();
  }

  ~AllTrainNodesTest() { wait(); }

  void expect_train_start(nmranet::NodeAlias alias, int addr, dcc::TrainAddressType type = dcc::TrainAddressType::DCC_LONG_ADDRESS) {
    nmranet::NodeID address = nmranet::TractionDefs::train_node_id_from_legacy(type, addr);
    expect_packet(StringPrintf(":X10701%03XN%012" PRIX64 ";", alias, address));
    expect_packet(StringPrintf(":X19100%03XN%012" PRIX64 ";", alias, address));
    expect_packet(
        StringPrintf(":X19547%03XN0101000000000303;", alias));
    expect_packet(
        StringPrintf(":X19524%03XN090099FF00000000;", alias));
  }

  TrainDb trainDb_;
  nmranet::SimpleInfoFlow infoFlow_{&g_service};
  nmranet::CanDatagramService datagramService_{ifCan_.get(), 5, 2};
  nmranet::MemoryConfigHandler memoryConfigHandler_{&datagramService_, nullptr,
                                                    5};
  std::unique_ptr<AllTrainNodes> trainNodes_;
};

} // namespace commandstation
