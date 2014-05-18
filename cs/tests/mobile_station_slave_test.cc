#include "utils/async_if_test_helper.hxx"

#include "tests/mock_packet_handler.hxx"
#include "mobilestation/MobileStationSlave.hxx"

class MostaSlaveTest : public NMRAnet::AsyncCanTest {
 protected:
  MostaSlaveTest() 
      : device_(&g_service, &can_hub0),
        slave_(&g_executor, &device_) {
    // Ignores all packets to the host packet queue.
    EXPECT_CALL(host_packet_queue_, TransmitPacket(_)).Times(AtLeast(0));
  }

  ~MostaSlaveTest() {
    wait();
  }

  MockHostPacketQueue host_packet_queue_;
  CanIf device_;
  mobilestation::MobileStationSlave slave_;
};


TEST_F(MostaSlaveTest, CreateDestroy) {}
