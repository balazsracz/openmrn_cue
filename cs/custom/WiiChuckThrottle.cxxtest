#include "utils/async_traction_test_helper.hxx"
#include "nmranet/TractionTestTrain.hxx"
#include "custom/WiiChuckThrottle.hxx"

using namespace bracz_custom;

namespace nmranet {

class ChuckThrottleTest : public TractionTest {
 protected:
  ChuckThrottleTest() : trainImpl_(1732), throttle_(node_, 0x0601000006C4ULL) {
    create_allocated_alias();
    expect_next_alias_allocation();
    // alias reservation
    expect_packet(":X1070133AN0601000006C4;");
    // initialized
    expect_packet(":X1910033AN0601000006C4;");
    trainNode_.reset(new TrainNode(&trainService_, &trainImpl_));
    wait();
  }

  ~ChuckThrottleTest() { wait(); }

  void send_measurement(uint8_t value) {
    auto* b = throttle_.alloc();
    memset(b->data()->data, 0, 6);
    b->data()->data[0] = value;
    throttle_.send(b);
    wait();
  }

  LoggingTrain trainImpl_;
  std::unique_ptr<TrainNode> trainNode_;
  bracz_custom::WiiChuckThrottle throttle_;
};

/* There are no expectations in these tests; they are just for illustrating how
 * to use the loggintrain and ensure that it compiles and does not crash. If
 * you run the test, the actions should be printed to stderr. */
TEST_F(ChuckThrottleTest, CreateDestroy) {}

TEST_F(ChuckThrottleTest, SetSpeed) { send_packet(":X195EA551N033A0050B0;"); }

TEST_F(ChuckThrottleTest, SetFn) {
  send_packet(":X195EA551N033A010000050001;");
  send_packet(":X195EA551N033A010000050000;");
}

TEST_F(ChuckThrottleTest, ChuckCenter) {
  send_measurement(0x7b);
}

TEST_F(ChuckThrottleTest, ChuckLeftMax) {
  send_measurement(0x0);
}



}  // namespace nmranet
