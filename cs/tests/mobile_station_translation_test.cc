#include "utils/async_traction_test_helper.hxx"

#include "mobilestation/MobileStationTraction.hxx"
#include "mobilestation/TrainDb.hxx"

namespace nmranet {
::std::ostream& operator<<(::std::ostream& os, const Velocity& v) {
  return os << "Velocity " << (v.direction() == Velocity::FORWARD ? 'F' : 'R')
            << " " << v.speed() << " m/s or " << v.mph() << " mph.";
}

}

namespace mobilestation {

class MostaTranslationTest : public nmranet::TractionTest {
 protected:
  static void SetUpTestCase() {
    nmranet::AsyncNodeTest::SetUpTestCase();
    g_gc_adapter1 =
        GCAdapterBase::CreateGridConnectAdapter(&gc_hub1, &can_hub1, false);
  }

  static void TearDownTestCase() {
    nmranet::AsyncNodeTest::TearDownTestCase();
    delete g_gc_adapter1;
  }

  MostaTranslationTest()
      : can_if1_(&g_service, &can_hub1),
        mosta_traction_(&can_if1_, ifCan_.get(), &train_db_, node_) {
    gc_hub1.register_port(&canBus1_);
    // Sets up the traction train node.
    create_allocated_alias();
    expect_next_alias_allocation();
    EXPECT_CALL(m1_, legacy_address()).Times(AtLeast(0)).WillRepeatedly(
        Return(0x00000056U));
    // alias reservation
    expect_packet(":X1070133AN060100000056;");
    // initialized
    expect_packet(":X1910033AN060100000056;");
    trainNode_.reset(new nmranet::TrainNode(&trainService_, &m1_));
    wait();
  }

  ~MostaTranslationTest() { gc_hub1.unregister_port(&canBus1_); }

#define expect_packet1(gc_packet) \
  EXPECT_CALL(canBus1_, mwrite(StrCaseEq(gc_packet)))

  void send_packet1(const string &gc_packet) {
    Buffer<HubData> *packet;
    mainBufferPool->alloc(&packet);
    packet->data()->assign(gc_packet);
    packet->data()->skipMember_ = &canBus1_;
    gc_hub1.send(packet);
  }

  /// Helper object for setting expectations on the packets sent on the bus.
  NiceMock<MockSend> canBus1_;
  CanIf can_if1_;

  std::unique_ptr<nmranet::TrainNode> trainNode_;

  TrainDb train_db_;
  MobileStationTraction mosta_traction_;
};


const struct const_loco_db_t const_lokdb[] = {
  // 0
  { 0x57, { 0, 1, 3, 4,  0xff, }, { LIGHT, TELEX, FNT11, ABV,  0xff, },
    "BR 260417", DCC_28 },
  // 1
  { 0x56, {0, 1, 3, 4, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 0xff} , { LIGHT, ENGINE, HONK, SPEECH, SPEECH, SPEECH, SPEECH, LIGHT1, FNP, ABV, HONK, SOUNDP, SOUNDP, SOUNDP, HONK, HONK, HONK, HONK, HONK, HONK, 0xff }, "ICN", DCC_28 | PUSHPULL },

};

const size_t const_lokdb_size = sizeof(const_lokdb) / sizeof(const_lokdb[0]);

TEST_F(MostaTranslationTest, Create) {}

MATCHER_P(VApprox, velocity, "") {
  return (arg - velocity).speed() < 1e-3;
}

TEST_F(MostaTranslationTest, SpeedSetXlate) {
  nmranet::Velocity v(0);
  v.reverse();
  EXPECT_CALL(m1_, set_speed(v));
  send_packet1(":X08080500N010080;");

  wait();

  v.set_mph(13);
  v.forward();
  EXPECT_CALL(m1_, set_speed(VApprox(v)));
  send_packet1(":X08080500N01000d;");

  wait();
}

TEST_F(MostaTranslationTest, FnSetXlate) {
  EXPECT_CALL(m1_, set_fn(4, 1));
  send_packet1(":X08080500N050001;");
  wait();

  EXPECT_CALL(m1_, set_fn(0, 0));
  send_packet1(":X08080500N020000;");
  wait();
}

TEST_F(MostaTranslationTest, FnGetXlate) {
  EXPECT_CALL(m1_, get_fn(11)).WillOnce(Return(0x13));
  expect_packet1(":X08080500N0D0001;");
  send_packet1(":X08080500N0D00;");
  wait();
}

TEST_F(MostaTranslationTest, DISABLED_FnGetSpeed) {
  nmranet::Velocity v(0);
  v.set_mph(0x13);
  EXPECT_CALL(m1_, get_speed()).WillOnce(Return(v));
  expect_packet1(":X08080100N010013");
  send_packet1(":X08080100N0100;");
  wait();
}

}  // namespace mobilestation
