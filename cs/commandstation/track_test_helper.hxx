#include "utils/async_if_test_helper.hxx"

#include "commandstation/UpdateProcessor.hxx"
#include "dcc/Loco.hxx"
#include "dcc/UpdateLoop.hxx"

#include "dcc/dcc_test_utils.hxx"

using ::testing::ElementsAre;

class MockPacketQueue : public dcc::PacketFlowInterface {
 public:
  MOCK_METHOD1(arrived, void(const dcc::Packet&));

  void send(Buffer<dcc::Packet>* b, unsigned prio) {
    dcc::Packet* pkt = b->data();
    arrived(*pkt);
    b->unref();
  }
};

OVERRIDE_CONST(dcc_packet_min_refresh_delay_ms, 50);

class UpdateProcessorTest : public openlcb::AsyncNodeTest {
 protected:
  UpdateProcessorTest() : updateProcessor_(&g_service, &trackSendQueue_) {}

  ~UpdateProcessorTest() { wait(); }

  void send_empty_packet() {
    Buffer<dcc::Packet>* b;
    mainBufferPool->alloc(&b, nullptr);
    updateProcessor_.send(b);
  }

  void wait() {
    usleep(70000);
    AsyncCanTest::wait();
    Mock::VerifyAndClear(&updateProcessor_);
  }

  void qwait() {
    AsyncCanTest::wait();
    Mock::VerifyAndClear(&updateProcessor_);
  }

  StrictMock<MockPacketQueue> trackSendQueue_;
  commandstation::UpdateProcessor updateProcessor_;
};
