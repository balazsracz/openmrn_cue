#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "src/host_packet.h"


class MockHostPacketQueue : public PacketQueue {
 public:
  MockHostPacketQueue() {
    ASSERT(!instance_);
    instance_ = this;
  }

  ~MockHostPacketQueue() {
    ASSERT(instance_ == this);
    instance_ = nullptr;
  }

  MOCK_METHOD1(TransmitPacket, void (PacketBase&));
};


class HostPacketTestHelper {
#define EXPECT_HOST_PACKET(v...) \
  EXPECT_CALL(host_packet_queue_, TransmitPacket(::testing::Property(&PacketBase::as_vector, ::testing::ContainerEq(vector<uint8_t>(v)))))

 protected:
  StrictMock<MockHostPacketQueue> host_packet_queue_;
};
