#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "src/host_packet.h"


class MockPacketQueue : public PacketQueue {
 public:
  MockPacketQueue() {
    ASSERT(!instance_);
    instance_ = this;
  }

  ~MockPacketQueue() {
    ASSERT(instance_ == this);
    instance_ = nullptr;
  }

  MOCK_METHOD1(TransmitPacket, void (PacketBase&));
};


class PacketTestHelper {
#define EXPECT_HOST_PACKET(v...) \
  EXPECT_CALL(packet_queue_, TransmitPacket(::testing::Property(&PacketBase::as_vector, ::testing::ContainerEq(vector<uint8_t>(v)))))

 protected:
  StrictMock<MockPacketQueue> packet_queue_;
};
