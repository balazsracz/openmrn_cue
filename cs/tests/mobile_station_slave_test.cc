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

  void run_init() {
    send_packet_and_expect_response(":X00000380N;",
                                    ":X1c00007eN;");
    expect_packet(":X1c14d87eN;");
    expect_packet(":X1f99587eN;");
    expect_packet(":X1d75d87eN;");
    expect_packet(":X1c02587eN0005e65e00020003;");
    expect_packet(":X1c02D87eN000000970200c011;");
    expect_packet(":X1c03587eN010102140000c011;");
    expect_packet(":X1c03d87eN000040110107002a;");
    send_packet(":X1c0058feN;");
    wait();
  }

  MockHostPacketQueue host_packet_queue_;
  CanIf device_;
  mobilestation::MobileStationSlave slave_;
};


TEST_F(MostaSlaveTest, CreateDestroy) {}

OVERRIDE_CONST(mosta_slave_keepalive_timeout_ms, 100);

TEST_F(MostaSlaveTest, DiscoverRepeated) {
  // A discover frame should show up
  expect_packet(":X1C00007EN;");
  usleep(110000);
  wait();
  expect_packet(":X1C00007EN;");
  usleep(110000);
  wait();
  expect_packet(":X1C00007EN;");
  usleep(110000);
  wait();
}

TEST_F(MostaSlaveTest, InitSequence) {
  expect_packet(":X1C00007EN;");
  usleep(120000);
  send_packet_and_expect_response(":X00000380N;",
                                  ":X1c00007eN;");
  ::testing::InSequence s;
  expect_packet(":X1c14d87eN;");
  expect_packet(":X1f99587eN;");
  expect_packet(":X1d75d87eN;");
  expect_packet(":X1c02587eN0005e65e00020003;");
  expect_packet(":X1c02D87eN000000970200c011;");
  expect_packet(":X1c03587eN010102140000c011;");
  expect_packet(":X1c03d87eN000040110107002a;");
  send_packet(":X1c0058feN;");
  //expect_any_packet();
  wait();
}

TEST_F(MostaSlaveTest, KeepAliveAfterInit) {
  run_init();
  wait();
  //send_packet_and_expect_response();
}
