#include "utils/async_if_test_helper.hxx"

#include "utils/Buffer.hxx"
#include "dcc/Packet.hxx"
#include "custom/TrackInterface.hxx"
#include "mock_packet_handler.hxx"

#include "src/usb_proto.h"
#include "custom/HostLogging.hxx"

class HostPacketQueueTest : public ::testing::Test, public HostPacketTestHelper {};

TEST_F(HostPacketQueueTest, TestSendLog) {
  EXPECT_HOST_PACKET({CMD_CAN_LOG, '%'});
  send_host_log_event(bracz_custom::HostLogEvent::MOSTA_DISCOVER);
}

class MockEmptyQueue : public PacketFlowInterface {
 public:
  MOCK_METHOD2(arrived, void(Buffer<dcc::Packet>*, unsigned));

  void send(Buffer<dcc::Packet>* b, unsigned prio) {
    arrived(b, prio);
    b->unref();
  }
};

class TrackIfTest : public nmranet::AsyncCanTest, public HostPacketTestHelper {
 protected:
  TrackIfTest()
      : can_if_(&g_service, &can_hub0),
        if_send_(&can_hub0),
        if_recv_(&can_if_, &empty_packets_) {}

  ~TrackIfTest() {
    wait();
  }


  CanIf can_if_;
  bracz_custom::TrackIfSend if_send_;
  StrictMock<MockEmptyQueue> empty_packets_;
  bracz_custom::TrackIfReceive if_recv_;
};

TEST_F(TrackIfTest, CreateDestroy) {}

TEST_F(TrackIfTest, SendSomePacket) {
  EXPECT_HOST_PACKET({CMD_CAN_LOG, '@'});
  auto* b = if_send_.alloc();
  b->data()->dlc = 2;
  b->data()->payload[0] = 0xA5;
  b->data()->payload[1] = 0x5A;
  b->data()->header_raw_data = 0x33;
  expect_packet(":S701N33A55A;");
  if_send_.send(b);
  wait();
}

TEST_F(TrackIfTest, SendADccPacket) {
  EXPECT_HOST_PACKET({CMD_CAN_LOG, '@'});
  auto* b = if_send_.alloc();
  b->data()->set_dcc_speed28(dcc::DccShortAddress(0x55), true, 28);
  expect_packet(":S701N04557F2A;");
  if_send_.send(b);
  wait();
}

TEST_F(TrackIfTest, IncomingUnknownCommand) {
  send_packet(":S700N33;");
}

TEST_F(TrackIfTest, IncomingKeepaliveNoSpace) {
  //EXPECT_HOST_PACKET({CMD_CAN_LOG, '@'});
  send_packet_and_expect_response(":S700N010000;",
                                  ":S701N01;");
}

TEST_F(TrackIfTest, IncomingKeepaliveNoSpaceAlive) {
  EXPECT_HOST_PACKET({CMD_CAN_LOG, '*'});
  send_packet_and_expect_response(":S700N010001;",
                                  ":S701N01;");
}

TEST_F(TrackIfTest, IncomingKeepaliveOneSpaceAlive) {
  EXPECT_HOST_PACKET({CMD_CAN_LOG, '*'});
  EXPECT_CALL(empty_packets_, arrived(_, _));
  send_packet_and_expect_response(":S700N010101;",
                                  ":S701N01;");
}

TEST_F(TrackIfTest, IncomingKeepaliveTwoSpaceAlive) {
  EXPECT_HOST_PACKET({CMD_CAN_LOG, '*'});
  EXPECT_CALL(empty_packets_, arrived(_, _)).Times(2);
  send_packet_and_expect_response(":S700N010201;",
                                  ":S701N01;");
}
