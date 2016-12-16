// This base can be used when the AllTrainNodes needs to be instantiated in a
// test.

#include "utils/async_traction_test_helper.hxx"

#include "nmranet/DatagramCan.hxx"
#include "commandstation/AllTrainNodes.hxx"
#include "nmranet/SimpleInfoProtocol.hxx"
#include "nmranet/MemoryConfig.hxx"
#include "nmranet/ConfigUpdateFlow.hxx"
#include "dcc/FakeTrackIf.hxx"
#include "dcc/SimpleUpdateLoop.hxx"

namespace openlcb {
extern Pool *const g_incoming_datagram_allocator = init_main_buffer_pool();

extern const char *const SNIP_DYNAMIC_FILENAME = "/dev/zero";
} // namespace openlcb

namespace commandstation {

extern const char TRAINCDI_DATA[] = "Test cdi data";
extern const size_t TRAINCDI_SIZE = sizeof(TRAINCDI_DATA);
extern const char TRAINTMPCDI_DATA[] = "Test cdi tmp data";
extern const size_t TRAINTMPCDI_SIZE = sizeof(TRAINTMPCDI_DATA);


dcc::FakeTrackIf fakeTrack{&g_service, 3};
dcc::SimpleUpdateLoop updateLoop{&g_service, &fakeTrack};

extern const unsigned TRAINDB_TEST_NUM_ALIAS;


class TrainDbTestBase : public openlcb::TractionTest {
 protected:
  TrainDbTestBase() {
    wait();
    for (unsigned i = 0; i < TRAINDB_TEST_NUM_ALIAS; ++i) {
      inject_allocated_alias(0x340 + i);
    }
  }

  ~TrainDbTestBase() { wait(); }

  openlcb::ConfigUpdateFlow cfgflow{ifCan_.get()};
};


class TrainDbTest : public TrainDbTestBase {
 protected:
  TrainDbTest() { wait(); }
  ~TrainDbTest() { wait(); }

  openlcb::CanDatagramService dgHandler_{ifCan_.get(), 5, 2};
  openlcb::MemoryConfigHandler memCfgHandler_{&dgHandler_, node_, 5};
  openlcb::SimpleInfoFlow infoFlow_{&g_service};
  TrainDb db_;
  AllTrainNodes allTrainNodes_{&db_, &trainService_, &infoFlow_,
                               &memCfgHandler_};
};

} // namespace commandstation
