#include "utils/async_if_test_helper.hxx"

#include "dcc/Loco.hxx"
#include "commandstation/UpdateProcessor.hxx"

using ::testing::ElementsAre;

class MockPacketQueue : public PacketFlowInterface {
 public:
  MOCK_METHOD2(arrived, void(uint8_t, vector<uint8_t>));

  void send(Buffer<dcc::Packet>* b, unsigned prio) {
    dcc::Packet* pkt = b->data();
    arrived(pkt->header_raw_data,
            vector<uint8_t>(pkt->payload + 0, pkt->payload + pkt->dlc));
    b->unref();
  }
};

class UpdateProcessorTest : public NMRAnet::AsyncCanTest {
 protected:
  UpdateProcessorTest() : updateProcessor_(&g_service, &trackSendQueue_) {}

  ~UpdateProcessorTest() { wait(); }

  void send_empty_packet() {
    Buffer<dcc::Packet>* b;
    mainBufferPool->alloc(&b, nullptr);
    updateProcessor_.send(b);
  }

  StrictMock<MockPacketQueue> trackSendQueue_;
  commandstation::UpdateProcessor updateProcessor_;
};

TEST_F(UpdateProcessorTest, CreateDestroy) {}

TEST_F(UpdateProcessorTest, SendManyIdles) {
  EXPECT_CALL(trackSendQueue_, arrived(0x04, ElementsAre(0xFF, 0, 0xFF)))
      .Times(100);
  for (int i = 0; i < 100; ++i) send_empty_packet();
}

TEST_F(UpdateProcessorTest, TrainRefresh) {
  dcc::Dcc28Train t(dcc::DccShortAddress(55));
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b01100000, _)));
  send_empty_packet();
  wait();
}

TEST_F(UpdateProcessorTest, OneTrainManyRefresh) {
  dcc::Dcc28Train t(dcc::DccShortAddress(55));
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b01100000, _)));
  send_empty_packet();wait();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10000000, _)));
  send_empty_packet();wait();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10110000, _)));
  send_empty_packet();wait();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10100000, _)));
  send_empty_packet();wait();

  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b01100000, _)));
  send_empty_packet();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10000000, _)));
  send_empty_packet();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10110000, _)));
  send_empty_packet();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10100000, _)));
  send_empty_packet();

  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b01100000, _)));
  send_empty_packet();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10000000, _)));
  send_empty_packet();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10110000, _)));
  send_empty_packet();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10100000, _)));
  send_empty_packet();

  wait();
}

TEST_F(UpdateProcessorTest, TwoTrainRefresh) {
  dcc::Dcc28Train t(dcc::DccShortAddress(55));
  dcc::Dcc28Train tt(dcc::DccShortAddress(33));
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b01100000, _)));
  send_empty_packet(); wait();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(33, 0b01100000, _)));
  send_empty_packet(); wait();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10000000, _)));
  send_empty_packet(); wait();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(33, 0b10000000, _)));
  send_empty_packet(); wait();
}


TEST_F(UpdateProcessorTest, OneTrainRefreshAndUpdate) {
  dcc::Dcc28Train t(dcc::DccShortAddress(55));
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b01100000, _)));
  send_empty_packet(); wait();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10000000, _)));
  send_empty_packet(); wait();
  // A notify will cause a speed packet to be injected...
  t.set_speed(-37.5);
  // ... with increased repeat count ----v
  EXPECT_CALL(trackSendQueue_, arrived(0x44, ElementsAre(55, 0b01001011, _)));
  send_empty_packet(); wait();
  // And then we continue to go on with background update.
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10110000, _)));
  send_empty_packet(); wait();
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b10100000, _)));
  send_empty_packet(); wait();
  // and when we get back to speed, we see the updated value there.
  EXPECT_CALL(trackSendQueue_, arrived(0x4, ElementsAre(55, 0b01001011, _)));
  send_empty_packet(); wait();
}