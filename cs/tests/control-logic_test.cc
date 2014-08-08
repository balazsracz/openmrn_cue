#include <unistd.h>

#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "automata_tests_helper.hxx"
#include "../automata/control-logic.hxx"

using ::testing::HasSubstr;

namespace automata {

class LogicTest : public AutomataNodeTests {
 protected:
  LogicTest()
      : block_(&brd, BRACZ_LAYOUT|0xC000, "blk"),
        magnet_aut_(&brd, alloc()) {
    // We ignore all event messages on the CAN bus. THese are checked with more
    // high-level constructs.
    EXPECT_CALL(canBus_, mwrite(HasSubstr(":X195B422AN"))).Times(AtLeast(0));
    EXPECT_CALL(canBus_, mwrite(HasSubstr(":X1991422AN"))).Times(AtLeast(0));
    EXPECT_CALL(canBus_, mwrite(HasSubstr(":X1954522AN"))).Times(AtLeast(0));
  }

  const EventBlock::Allocator& alloc() { return *block_.allocator(); }

  friend struct TestBlock;

  struct TestBlock {
    TestBlock(LogicTest* test, const string& name)
        : inverted_detector(test),
          signal_green(test),
          physical_signal(&inverted_detector, &signal_green, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
          b(&test->brd, &physical_signal, EventBlock::Allocator(&test->alloc(), name, 80)) {
      inverted_detector.Set(true);
      signal_green.Set(false);
    }
    FakeBit inverted_detector;
    FakeBit signal_green;
    PhysicalSignal physical_signal;
    StandardBlock b;
  };

  struct TestDetectorBlock {
    TestDetectorBlock(LogicTest* test, const string& name)
        : detector(test),
          b(EventBlock::Allocator(&test->alloc(), name, 32), &detector) {}
    // TODO: do we need to add an automata for this?
    FakeBit detector;
    StraightTrackWithDetector b;
  };

  struct TestShortTrack {
    TestShortTrack(LogicTest* test, const string& name)
        : b(EventBlock::Allocator(&test->alloc(), name, 24)),
          aut_body(name, &test->brd, &b) {}
    StraightTrackShort b;
    StandardPluginAutomata aut_body;
  };

  struct TestFixedTurnout {
    TestFixedTurnout(LogicTest* test, FixedTurnout::State state, const string& name)
        : b(state, EventBlock::Allocator(&test->alloc(), name, 48, 32)),
          aut_body(name, &test->brd, &b) {}
    FixedTurnout b;
    StandardPluginAutomata aut_body;
  };

  struct TestMovableTurnout {
    TestMovableTurnout(LogicTest* test, const string& name)
        : set_0(test),
          set_1(test),
          magnet(&test->magnet_aut_, name + ".mgn", &set_0, &set_1),
          b(EventBlock::Allocator(&test->alloc(), name, 48, 32), &magnet),
          aut_body(name, &test->brd, &b) {}
    FakeBit set_0;
    FakeBit set_1;
    MagnetDef magnet;
    MovableTurnout b;
    StandardPluginAutomata aut_body;
  };

  Board brd;
  EventBlock block_;
  MagnetCommandAutomata magnet_aut_;
};

TEST_F(LogicTest, TestFramework) {
  static EventBasedVariable ev_var(
      &brd, "test", BRACZ_LAYOUT | 0xf000, BRACZ_LAYOUT | 0xf001, 0);
  static FakeBit input(this);
  DefAut(testaut, brd, {
    DefCopy(ImportVariable(input), ImportVariable(&ev_var));
  });
  EventListener listen_event(node_, ev_var);
  SetupRunner(&brd);
  Run();
  Run();

  input.Set(false);
  Run();
  EXPECT_FALSE(listen_event.value());

  input.Set(true);
  Run();
  EXPECT_TRUE(listen_event.value());
}

TEST_F(LogicTest, TestFramework2) {
  static EventBasedVariable ev_var(
      &brd, "test", BRACZ_LAYOUT | 0xf000, BRACZ_LAYOUT | 0xf001, 0);
  static FakeBit bout(this);
  DefAut(testaut2, brd, {
    DefCopy(ImportVariable(ev_var), ImportVariable(&bout));
  });
  SetupRunner(&brd);
  Run();
  Run();

  SetVar(ev_var, false);
  Run();
  EXPECT_FALSE(bout.Get());

  SetVar(ev_var, true);
  Run();
  EXPECT_TRUE(bout.Get());

  SetVar(ev_var, false);
  Run();
  EXPECT_FALSE(bout.Get());
}

TEST_F(LogicTest, TestFramework3) {
  std::unique_ptr<GlobalVariable> h(alloc().Allocate("testbit"));
  std::unique_ptr<GlobalVariable> hh(alloc().Allocate("testbit2"));
  static GlobalVariable* v = h.get();
  static GlobalVariable* w = hh.get();
  EventListener ldst(node_, *v);
  EventListener lsrc(node_, *w);
  DefAut(testaut2, brd, {
    DefCopy(ImportVariable(*w), ImportVariable(v));
  });
  SetupRunner(&brd);
  Run();
  Run();

  SetVar(*w, false);
  Run();
  EXPECT_FALSE(lsrc.value());
  EXPECT_FALSE(ldst.value());
 
  SetVar(*w, true);
  Run();
  EXPECT_TRUE(lsrc.value());
  EXPECT_TRUE(ldst.value());

  SetVar(*w, false);
  Run();
  EXPECT_FALSE(lsrc.value());
  EXPECT_FALSE(ldst.value());
}

TEST_F(LogicTest, SimulatedOccupancy_SingleShortPiece) {
  static StraightTrackShort piece(alloc());
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      alloc(), &previous_detector);
  StraightTrackWithDetector after(
      alloc(), &next_detector);
  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(after.side_a());

  EventListener sim_occ(node_, *piece.simulated_occupancy_);
  EventListener seen_train(node_, *piece.tmp_seen_train_in_next_);
  EventListener released(node_, *piece.side_b()->out_route_released);

  DefAut(strategyaut, brd, { piece.SimulateAllOccupancy(this); });

  EXPECT_FALSE(sim_occ.value());
  SetupRunner(&brd);
  Run();
  previous_detector.Set(true);
  next_detector.Set(false);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(seen_train.value());
  SetVar(*piece.route_set_ab_, true);
  Run();
  EXPECT_TRUE(sim_occ.value());
  EXPECT_FALSE(seen_train.value());
  EXPECT_FALSE(released.value());
  // Now the train reaches the next detector.
  next_detector.Set(true);
  Run();
  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(seen_train.value());
  // And then leaves the next detector.
  next_detector.Set(false);
  Run();
  EXPECT_TRUE(seen_train.value());
  EXPECT_TRUE(released.value());
  EXPECT_FALSE(sim_occ.value());
}

TEST_F(LogicTest, SimulatedOccupancy_MultipleShortPiece) {
  static StraightTrackShort piece(alloc());
  static StraightTrackShort piece2(alloc());
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      alloc(), &previous_detector);
  StraightTrackWithDetector after(
      alloc(), &next_detector);
  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(piece2.side_a());
  piece2.side_b()->Bind(after.side_a());

  EventListener sim_occ(node_, *piece.simulated_occupancy_);
  EventListener sim_occ2(node_, *piece2.simulated_occupancy_);

  DefAut(strategyaut, brd, {
    piece.SimulateAllOccupancy(this);
    piece2.SimulateAllOccupancy(this);
  });

  SetupRunner(&brd);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  previous_detector.Set(true);
  next_detector.Set(false);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  SetVar(*piece.route_set_ab_, true);
  SetVar(*piece2.route_set_ab_, true);
  Run();
  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(sim_occ2.value());
  // Now the train reaches the next detector.
  next_detector.Set(true);
  Run();
  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(sim_occ2.value());
  // And then leaves the next detector.
  next_detector.Set(false);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());

  SetVar(*piece.route_set_ab_, false);
  SetVar(*piece2.route_set_ab_, false);
  previous_detector.Set(false);
  Run();

  // Now we go backwards!
  SetVar(*piece.route_set_ba_, true);
  SetVar(*piece2.route_set_ba_, true);
  next_detector.Set(true);
  Run();
  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(sim_occ2.value());

  previous_detector.Set(true);
  Run();
  next_detector.Set(false);
  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(sim_occ2.value());
  Run();
  previous_detector.Set(false);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
}

TEST_F(LogicTest, SimulatedOccupancy_ShortAndLongPieces) {
  static StraightTrackShort piece(EventBlock::Allocator(&alloc(), "p1", 32, 32));
  static StraightTrackShort piece2(EventBlock::Allocator(&alloc(), "p2", 32, 32));
  static StraightTrackLong piece3(EventBlock::Allocator(&alloc(), "p3", 32, 32));
  static StraightTrackShort piece4(EventBlock::Allocator(&alloc(), "p4", 32, 32));
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      alloc(), &previous_detector);
  StraightTrackWithDetector after(
      alloc(), &next_detector);
  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(piece2.side_a());
  piece2.side_b()->Bind(piece3.side_a());
  piece3.side_b()->Bind(piece4.side_a());
  piece4.side_b()->Bind(after.side_a());

  EventListener sim_occ(node_, *piece.simulated_occupancy_);
  EventListener sim_occ2(node_, *piece2.simulated_occupancy_);
  EventListener sim_occ3(node_, *piece3.simulated_occupancy_);
  EventListener sim_occ4(node_, *piece4.simulated_occupancy_);

  DefAut(strategyaut, brd, {
    piece.SimulateAllOccupancy(this);
    piece2.SimulateAllOccupancy(this);
    piece3.SimulateAllOccupancy(this);
    piece4.SimulateAllOccupancy(this);
  });

  SetupRunner(&brd);

  EXPECT_EQ(&next_detector, piece4.side_b()->binding()->LookupNextDetector());
  EXPECT_EQ(&next_detector, piece3.side_b()->binding()->LookupNextDetector());
  EXPECT_EQ(&next_detector, piece3.side_b()->binding()->LookupCloseDetector());
  EXPECT_EQ(&next_detector, piece2.side_b()->binding()->LookupFarDetector());

  EXPECT_EQ(&previous_detector,
            piece4.side_a()->binding()->LookupFarDetector());
  EXPECT_EQ(&previous_detector,
            piece3.side_a()->binding()->LookupCloseDetector());

  // Now run the train forwards.
  SetVar(*piece.route_set_ab_, true);
  SetVar(*piece2.route_set_ab_, true);
  SetVar(*piece3.route_set_ab_, true);
  SetVar(*piece4.route_set_ab_, true);
  Run(3);
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_FALSE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());

  previous_detector.Set(true);
  next_detector.Set(false);
  Run(3);

  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(sim_occ2.value());
  EXPECT_TRUE(QueryVar(*piece2.simulated_occupancy_));
  EXPECT_TRUE(sim_occ3.value());
  EXPECT_TRUE(QueryVar(*piece3.simulated_occupancy_));
  EXPECT_TRUE(sim_occ4.value());
  EXPECT_TRUE(QueryVar(*piece4.simulated_occupancy_));

  previous_detector.Set(false);
  next_detector.Set(true);
  Run(3);

  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_TRUE(sim_occ3.value());
  EXPECT_TRUE(sim_occ4.value());

  next_detector.Set(false);
  Run(3);

  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_FALSE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());

  // Run backwards.
  SetVar(*piece.route_set_ab_, false);
  SetVar(*piece2.route_set_ab_, false);
  SetVar(*piece3.route_set_ab_, false);
  SetVar(*piece4.route_set_ab_, false);
  Run();
  SetVar(*piece.route_set_ba_, true);
  SetVar(*piece2.route_set_ba_, true);
  SetVar(*piece3.route_set_ba_, true);
  SetVar(*piece4.route_set_ba_, true);
  Run();

  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_FALSE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());

  next_detector.Set(true);
  Run();
  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(sim_occ2.value());
  EXPECT_TRUE(sim_occ3.value());
  EXPECT_TRUE(sim_occ4.value());

  next_detector.Set(false);
  Run();
  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(sim_occ2.value());
  EXPECT_TRUE(sim_occ3.value());
  EXPECT_TRUE(sim_occ4.value());

  previous_detector.Set(true);
  Run();
  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(QueryVar(*piece.simulated_occupancy_));
  EXPECT_TRUE(sim_occ2.value());
  EXPECT_TRUE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());

  previous_detector.Set(false);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(QueryVar(*piece.simulated_occupancy_));
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_FALSE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());
}

TEST_F(LogicTest, SimulatedOccupancy_RouteSetting) {
  static StraightTrackShort piece(EventBlock::Allocator(&alloc(), "p", 32, 32));
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      EventBlock::Allocator(&alloc(), "before", 32, 32), &previous_detector);
  StraightTrackWithDetector after(
      EventBlock::Allocator(&alloc(), "after", 32, 32), &next_detector);

  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(after.side_a());

  DefAut(strategyaut, brd, {
    piece.SimulateAllOccupancy(this);
    piece.SimulateAllRoutes(this);
    HandleInitState(this);
  });

  SetupRunner(&brd);

  Run();
  Run();

  EXPECT_EQ(StBase.state, runner_->GetAllAutomatas()[0]->GetState());

  EXPECT_FALSE(QueryVar(*piece.route_set_ba_));
  EXPECT_FALSE(QueryVar(*piece.route_set_ab_));
  EXPECT_FALSE(QueryVar(*piece.tmp_route_setting_in_progress_));
  // An incoming route request here.
  SetVar(*before.side_b()->out_try_set_route, true);

  Run();

  // Should be propagated.
  EXPECT_TRUE(QueryVar(*piece.side_b()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*piece.side_a()->in_route_set_success));
  EXPECT_TRUE(QueryVar(*piece.tmp_route_setting_in_progress_));

  Run();
  EXPECT_TRUE(QueryVar(*piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(QueryVar(*before.side_b()->out_try_set_route));
  Run();

  EXPECT_EQ(after.side_a()->binding(), piece.side_b());
  EXPECT_EQ(before.side_b()->binding(), piece.side_a());
  EXPECT_EQ(after.side_a(), piece.side_b()->binding());
  EXPECT_EQ(before.side_b(), piece.side_a()->binding());
  EXPECT_TRUE(QueryVar(*after.side_a()->binding()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*before.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*before.side_b()->binding()->in_route_set_failure));
  EXPECT_TRUE(QueryVar(*piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(QueryVar(*piece.simulated_occupancy_));
  EXPECT_FALSE(QueryVar(*piece.route_set_ab_));
  EXPECT_FALSE(QueryVar(*piece.route_set_ba_));

  SetVar(*after.side_a()->in_route_set_success, true);
  SetVar(*after.side_a()->binding()->out_try_set_route, false);
  Run();
  Run();

  EXPECT_FALSE(QueryVar(*piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(SQueryVar(*after.side_a()->binding()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*before.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*before.side_b()->binding()->in_route_set_failure));
  EXPECT_FALSE(QueryVar(*before.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*piece.route_set_ab_));
  EXPECT_FALSE(QueryVar(*piece.route_set_ba_));

  // Now a second route setting should fail.
  SetVar(*before.side_b()->out_try_set_route, true);

  Run();
  Run();

  EXPECT_FALSE(QueryVar(*before.side_b()->out_try_set_route));
  EXPECT_FALSE(SQueryVar(*before.side_b()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(*before.side_b()->binding()->in_route_set_failure));

  // And an opposite route should fail too.
  SetVar(*after.side_a()->out_try_set_route, true);

  Run();
  Run();

  EXPECT_FALSE(QueryVar(*after.side_a()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*after.side_a()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(*after.side_a()->binding()->in_route_set_failure));
}
TEST_F(LogicTest, ReverseRoute) {
  static StraightTrackShort piece(alloc());
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      alloc(), &previous_detector);
  StraightTrackWithDetector after(
      alloc(), &next_detector);

  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(after.side_a());

  DefAut(strategyaut, brd, {
    piece.SimulateAllOccupancy(this);
    piece.SimulateAllRoutes(this);
    HandleInitState(this);
  });

  SetupRunner(&brd);

  Run();
  Run();

  // An incoming route request here.
  SetVar(*after.side_a()->out_try_set_route, true);

  Run();

  // Debug
  EXPECT_FALSE(QueryVar(*before.side_b()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*piece.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(QueryVar(*piece.route_pending_ab_));
  EXPECT_TRUE(QueryVar(*piece.route_pending_ba_));

  // Should be propagated.
  EXPECT_TRUE(QueryVar(*piece.side_a()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*piece.side_b()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*piece.side_b()->in_route_set_failure));

  Run();

  SetVar(*before.side_b()->in_route_set_success, true);
  SetVar(*before.side_b()->binding()->out_try_set_route, false);
  Run();
  Run();

  EXPECT_TRUE(SQueryVar(*piece.side_b()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*piece.side_b()->in_route_set_failure));
  EXPECT_TRUE(QueryVar(*piece.route_set_ba_));
  EXPECT_FALSE(QueryVar(*piece.route_set_ab_));
}

TEST_F(LogicTest, SimulatedOccupancy_SimultSetting) {
  static StraightTrackShort piece(alloc());
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      alloc(), &previous_detector);
  StraightTrackWithDetector after(
      alloc(), &next_detector);

  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(after.side_a());

  DefAut(strategyaut, brd, {
    piece.RunAll(this);
    //piece.SimulateAllOccupancy(this);
    //piece.SimulateAllRoutes(this);
  });

  SetupRunner(&brd);

  Run();
  Run();

  // An incoming route request here.
  SetVar(*after.side_a()->out_try_set_route, true);
  Run();

  // Should be propagated.
  EXPECT_TRUE(SQueryVar(*piece.side_a()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*piece.side_b()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*piece.side_b()->in_route_set_failure));
  EXPECT_TRUE(QueryVar(*piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(QueryVar(*after.side_a()->out_try_set_route));
  Run();

  // A conflicting route request,
  SetVar(*before.side_b()->out_try_set_route, true);
  Run();
  // which is rejected immediately.
  EXPECT_FALSE(QueryVar(*before.side_b()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*before.side_b()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(*before.side_b()->binding()->in_route_set_failure));

  // Then the first route set request returns.
  SetVar(*before.side_b()->in_route_set_success, true);
  SetVar(*before.side_b()->binding()->out_try_set_route, false);
  Run();

  // And then the first route sets properly.
  EXPECT_TRUE(QueryVar(*after.side_a()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*after.side_a()->binding()->in_route_set_failure));
  EXPECT_FALSE(QueryVar(*after.side_a()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*piece.route_set_ba_));
  EXPECT_FALSE(QueryVar(*piece.route_set_ab_));
}

TEST_F(LogicTest, MultiRoute) {
  static StraightTrackShort piece(alloc());
  static StraightTrackShort piece2(alloc());
  static StraightTrackLong piece3(alloc());
  static StraightTrackShort piece4(alloc());
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  static StraightTrackWithDetector before(
      alloc(), &previous_detector);
  static StraightTrackWithDetector after(
      alloc(), &next_detector);
  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(piece2.side_a());
  piece2.side_b()->Bind(piece3.side_a());
  piece3.side_b()->Bind(piece4.side_a());
  piece4.side_b()->Bind(after.side_a());

#define DefPiece(pp)               \
  DefAut(aut##pp, brd, {           \
    pp.SimulateAllOccupancy(this); \
    pp.SimulateAllRoutes(this);    \
    HandleInitState(this);         \
  })

  DefPiece(piece);
  DefPiece(piece2);
  DefPiece(piece3);
  DefPiece(piece4);
#undef DefPiece

  // This will immediately accept a route at the end stop.
  DefAut(strategyaut, brd, {
    LocalVariable* try_set_route =
        ImportVariable(after.side_a()->binding()->out_try_set_route.get());
    LocalVariable* route_set_succcess =
        ImportVariable(after.side_a()->in_route_set_success.get());
    Def().IfReg1(*try_set_route).ActReg0(try_set_route).ActReg1(
        route_set_succcess);
  });

  SetupRunner(&brd);

  Run(10);
  SetVar(*before.side_b()->out_try_set_route, true);
  Run(10);
  EXPECT_FALSE(QueryVar(*before.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*before.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*before.side_b()->binding()->in_route_set_failure));

  SetVar(*before.side_b()->binding()->in_route_set_success, false);
  Run(10);

  // Now we run the route.
  previous_detector.Set(true);
  Run(10);
  previous_detector.Set(false);
  Run(10);
  next_detector.Set(true);
  Run(10);
  next_detector.Set(false);
  Run(10);

  EXPECT_FALSE(SQueryVar(*piece.simulated_occupancy_));
  EXPECT_FALSE(SQueryVar(*piece.route_set_ab_));

  // Route should be done, we set another one.
  Run(10);
  SetVar(*before.side_b()->out_try_set_route, true);
  Run(10);
  EXPECT_FALSE(QueryVar(*before.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*before.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*before.side_b()->binding()->in_route_set_failure));
}


TEST_F(LogicTest, Signal0) {
  FakeBit previous_detector(this);
  FakeBit request_green(this);
  FakeBit signal_green(this);
  FakeBit next_detector(this);
  FakeBit request_green2(this);
  FakeBit signal_green2(this);
  static StraightTrackLong first_body(alloc());
  static StraightTrackWithDetector first_det(
      alloc(), &previous_detector);
  static SignalPiece signal(
      alloc(), &request_green, &signal_green);
  static StraightTrackLong mid(alloc());
  static StraightTrackLong second_body(alloc());
  static StraightTrackWithDetector second_det(
      alloc(), &next_detector);
  static SignalPiece second_signal(
      alloc(), &request_green2, &signal_green2);
  static StraightTrackLong after(alloc());

  BindSequence({&first_body,    &first_det,   &signal,
          &mid,           &second_body, &second_det,
          &second_signal, &after});

#define DefPiece(pp) \
  DefAut(aut##pp, brd, { pp.RunAll(this); })

  DefPiece(first_det);
  DefPiece(mid);
  DefPiece(signal);
  DefPiece(second_body);
  DefPiece(second_det);
  DefPiece(second_signal);

#undef DefPiece

  // This will immediately accept a route at the end stop.
  DefAut(strategyaut, brd, {
    LocalVariable* try_set_route =
        ImportVariable(after.side_a()->binding()->out_try_set_route.get());
    LocalVariable* route_set_succcess =
        ImportVariable(after.side_a()->in_route_set_success.get());
    Def().IfReg1(*try_set_route).ActReg0(try_set_route).ActReg1(
        route_set_succcess);
  });

  SetupRunner(&brd);
  Run(2);

  // Sets the first train's route until the first signal.
  SetVar(*first_body.side_b()->out_try_set_route, true);
  Run(5);
  EXPECT_FALSE(QueryVar(*first_body.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*first_body.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*first_body.side_b()->binding()->in_route_set_failure));

  EXPECT_TRUE(QueryVar(*first_det.route_set_ab_));
  // the route is not yet beyond the signal
  EXPECT_FALSE(QueryVar(*signal.route_set_ab_)); 
  EXPECT_FALSE(signal_green.Get());

  // First train shows up at first signal.
  previous_detector.Set(true);
  Run(5);
  EXPECT_TRUE(QueryVar(*first_det.route_set_ab_));
  EXPECT_FALSE(signal_green.Get());

  EXPECT_FALSE(QueryVar(*signal.route_set_ab_));
  EXPECT_FALSE(QueryVar(*mid.route_set_ab_));

  // and gets a green
  request_green.Set(true);
  Run(1);
  EXPECT_TRUE(request_green.Get());
  EXPECT_TRUE(QueryVar(*signal.side_b()->out_try_set_route));
  Run(10);

  EXPECT_FALSE(request_green.Get());
  EXPECT_TRUE(QueryVar(*mid.route_set_ab_));
  EXPECT_TRUE(QueryVar(*signal.route_set_ab_));
  EXPECT_TRUE(signal_green.Get());

  // Checks that signal green gets reset.
  signal_green.Set(false);
  EXPECT_FALSE(signal_green.Get());
  Run(5);
  EXPECT_TRUE(QueryVar(*signal.route_set_ab_));
  EXPECT_TRUE(signal_green.Get());

  // and leaves the block
  previous_detector.Set(false);
  Run(10);
  EXPECT_TRUE(QueryVar(*mid.route_set_ab_));
  EXPECT_FALSE(QueryVar(*signal.route_set_ab_));
  EXPECT_FALSE(QueryVar(*first_det.route_set_ab_));
  EXPECT_FALSE(signal_green.Get());

  EXPECT_FALSE(QueryVar(*first_body.side_b()->out_try_set_route));

  // We run a second route until the first signal
  SetVar(*first_body.side_b()->out_try_set_route, true);
  Run(5);
  // let's dump all bits that are set
  for (int c = 0; c < 256; c++) {
    int client, offset, bit;
    DecodeOffset(c, &client, &offset, &bit);
    if ((1 << bit) & *get_state_byte(client, offset)) {
      printf("bit %d [%d * 32 + %d * 8 + %d] (c %d o %d bit %d)\n",
             c,
             c / 32,
             (c % 32) / 8,
             c % 8,
             client,
             offset,
             bit);
    }
  }

  EXPECT_FALSE(QueryVar(*signal.route_set_ab_));
  EXPECT_FALSE(signal_green.Get());
  EXPECT_TRUE(QueryVar(*first_det.route_set_ab_));

  // We try to set another green, but the previous train blocks this.
  request_green.Set(true);
  Run(1);
  EXPECT_TRUE(request_green.Get());
  EXPECT_TRUE(QueryVar(*signal.side_b()->out_try_set_route));
  Run(10);

  EXPECT_FALSE(request_green.Get());
  EXPECT_FALSE(QueryVar(*signal.side_b()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*signal.route_set_ab_));
  EXPECT_FALSE(signal_green.Get());

  // First train reaches second signal.
  next_detector.Set(true);
  request_green.Set(true);
  Run(10);
  EXPECT_FALSE(QueryVar(*mid.route_set_ab_));
  EXPECT_TRUE(QueryVar(*second_det.route_set_ab_));
  EXPECT_FALSE(QueryVar(*second_signal.route_set_ab_));
  EXPECT_FALSE(signal_green2.Get());

  EXPECT_FALSE(request_green.Get());
  EXPECT_FALSE(signal_green.Get());

  request_green2.Set(true);
  Run(5);
  EXPECT_TRUE(signal_green2.Get());
  EXPECT_TRUE(QueryVar(*second_signal.route_set_ab_));

  next_detector.Set(false);
  Run(10);
  EXPECT_FALSE(signal_green2.Get());
  EXPECT_FALSE(QueryVar(*second_signal.route_set_ab_));

  // Now the second train can go.
  request_green.Set(true);
  Run(10);
  EXPECT_FALSE(request_green.Get());
  EXPECT_TRUE(signal_green.Get());
  EXPECT_TRUE(QueryVar(*signal.route_set_ab_));
  EXPECT_TRUE(QueryVar(*mid.route_set_ab_));
}


TEST_F(LogicTest, Signal) {
  TestDetectorBlock before(this, "before");
  static StraightTrackLong mid(alloc());
  DefAut(autmid, brd, { mid.RunAll(this); });
  TestBlock first(this, "first");
  TestBlock second(this, "second");
  static StraightTrackLong after(alloc());

  BindSequence({&before.b, &first.b, &mid, &second.b, &after});

  ASSERT_EQ(first.b.signal_.side_b(), mid.side_a()->binding());


  // This will immediately accept a route at the end stop.
  DefAut(strategyaut, brd, {
    LocalVariable* try_set_route =
        ImportVariable(after.side_a()->binding()->out_try_set_route.get());
    LocalVariable* route_set_succcess =
        ImportVariable(after.side_a()->in_route_set_success.get());
    Def().IfReg1(*try_set_route).ActReg0(try_set_route).ActReg1(
        route_set_succcess);
  });

  SetupRunner(&brd);

  // No trains initially.
  first.inverted_detector.Set(true);
  second.inverted_detector.Set(true);
  Run(1);

  // No trains initially.
  first.inverted_detector.Set(true);
  second.inverted_detector.Set(true);
  Run(20);
  EXPECT_FALSE(QueryVar(*first.b.body_det_.simulated_occupancy_));
  EXPECT_FALSE(QueryVar(*second.b.body_det_.simulated_occupancy_));


  // Sets the first train's route until the first signal.
  LOG(INFO, "Setting route to first signal.");
  before.detector.Set(true);
  SetVar(*before.b.side_b()->out_try_set_route, true);
  Run(5);
  EXPECT_FALSE(QueryVar(*before.b.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*first.b.body_.side_a()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*first.b.body_.side_a()->in_route_set_failure));

  EXPECT_TRUE(QueryVar(*first.b.body_det_.route_set_ab_));
  // the route is not yet beyond the signal
  EXPECT_FALSE(QueryVar(*first.b.signal_.route_set_ab_));
  EXPECT_FALSE(first.signal_green.Get());

  EXPECT_FALSE(QueryVar(*first.b.body_det_.simulated_occupancy_));
  // First train shows up at first signal.
  LOG(INFO, "First train shows up at first signal.");
  before.detector.Set(false);
  Run(1);
  first.inverted_detector.Set(false);
  Run(12);
  EXPECT_TRUE(QueryVar(*first.b.body_det_.simulated_occupancy_));

  EXPECT_TRUE(QueryVar(*first.b.body_det_.route_set_ab_));
  EXPECT_FALSE(first.signal_green.Get());

  EXPECT_FALSE(QueryVar(*first.b.signal_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*mid.route_set_ab_));

  // and gets a green
  LOG(INFO, "First train gets a green at first signal.");
  SetVar(*first.b.request_green(), true);
  Run(1);
  EXPECT_TRUE(QueryVar(*first.b.request_green()));
  EXPECT_TRUE(QueryVar(*first.b.signal_.side_b()->out_try_set_route));
  Run(10);
  EXPECT_FALSE(QueryVar(*first.b.signal_.side_b()->out_try_set_route));

  EXPECT_FALSE(QueryVar(*first.b.request_green()));
  EXPECT_TRUE(QueryVar(*mid.route_set_ab_));
  EXPECT_TRUE(QueryVar(*first.b.signal_.route_set_ab_));
  EXPECT_TRUE(first.signal_green.Get());

  // Checks that signal green gets reset.
  first.signal_green.Set(false);
  EXPECT_FALSE(first.signal_green.Get());
  Run(5);
  EXPECT_TRUE(QueryVar(*first.b.signal_.route_set_ab_));
  EXPECT_TRUE(first.signal_green.Get());


  EXPECT_TRUE(QueryVar(*first.b.body_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*first.b.body_det_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*first.b.body_.simulated_occupancy_));


  EXPECT_TRUE(QueryVar(*first.b.body_det_.simulated_occupancy_));
  // and leaves the block
  LOG(INFO, "train left first block.");
  first.inverted_detector.Set(true);
  Run(15);
  EXPECT_FALSE(QueryVar(*first.b.body_.simulated_occupancy_));
  EXPECT_FALSE(QueryVar(*first.b.body_det_.simulated_occupancy_));
  EXPECT_FALSE(QueryVar(*first.b.signal_.simulated_occupancy_));
  EXPECT_TRUE(QueryVar(*mid.route_set_ab_));
  EXPECT_FALSE(QueryVar(*first.b.signal_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*first.b.body_det_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*first.b.body_.route_set_ab_));
  EXPECT_FALSE(first.signal_green.Get());

  EXPECT_FALSE(QueryVar(*first.b.body_.side_b()->out_try_set_route));

  // We run a second route until the first signal
  LOG(INFO, "Second train: sets route to first signal.");
  before.detector.Set(true);
  Run(4);
  SetVar(*first.b.body_.side_b()->out_try_set_route, true);
  Run(5);
  // let's dump all bits that are set
  for (int c = 0; c < 256; c++) {
    int client, offset, bit;
    DecodeOffset(c, &client, &offset, &bit);
    if ((1 << bit) & *get_state_byte(client, offset)) {
      printf("bit %d [%d * 32 + %d * 8 + %d] (c %d o %d bit %d)\n",
             c,
             c / 32,
             (c % 32) / 8,
             c % 8,
             client,
             offset,
             bit);
    }
  }

  EXPECT_FALSE(QueryVar(*first.b.signal_.route_set_ab_));
  EXPECT_FALSE(first.signal_green.Get());
  EXPECT_TRUE(QueryVar(*first.b.body_det_.route_set_ab_));

  // We try to set another green, but the previous train blocks this.
  LOG(INFO, "Second train: try get green (and fail).");
  SetVar(*first.b.request_green(), true);
  Run(1);
  EXPECT_TRUE(QueryVar(*first.b.request_green()));
  EXPECT_TRUE(QueryVar(*first.b.signal_.side_b()->out_try_set_route));
  Run(10);

  EXPECT_FALSE(QueryVar(*first.b.request_green()));
  EXPECT_FALSE(QueryVar(*first.b.signal_.side_b()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*first.b.signal_.route_set_ab_));
  EXPECT_FALSE(first.signal_green.Get());

  // First train reaches second signal.
  LOG(INFO, "First train reaches second signal and first train tries to get green again(fail).");
  second.inverted_detector.Set(false);
  SetVar(*first.b.request_green(), true);
  Run(14);
  EXPECT_FALSE(QueryVar(*mid.route_set_ab_));
  EXPECT_TRUE(QueryVar(*second.b.body_det_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*second.b.signal_.route_set_ab_));
  EXPECT_FALSE(second.signal_green.Get());

  EXPECT_FALSE(QueryVar(*first.b.request_green()));
  EXPECT_FALSE(first.signal_green.Get());

  LOG(INFO, "First train gets green at second signal.");
  SetVar(*second.b.request_green(), true);
  Run(5);
  EXPECT_TRUE(second.signal_green.Get());
  EXPECT_TRUE(QueryVar(*second.b.signal_.route_set_ab_));

  LOG(INFO, "First train leaves from second signal.");
  second.inverted_detector.Set(true);
  Run(10);
  EXPECT_FALSE(second.signal_green.Get());
  EXPECT_FALSE(QueryVar(*second.b.signal_.route_set_ab_));

  // Now the second train can go.
  LOG(INFO, "Second train gets green to second signal.");
  SetVar(*first.b.request_green(), true);
  Run(10);
  EXPECT_FALSE(QueryVar(*first.b.request_green()));
  EXPECT_TRUE(first.signal_green.Get());
  EXPECT_TRUE(QueryVar(*first.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*mid.route_set_ab_));
}

//              ------station_lr>>---
// ----left>---/----<<station_rl-----\----<right---

TEST_F(LogicTest, FixedTurnout) {
  static TestDetectorBlock end_left(this, "end_left");
  static TestDetectorBlock end_right(this, "end_right");
  TestShortTrack mid_left(this, "mid_left");
  TestShortTrack mid_right(this, "mid_right");
  TestBlock left(this, "left");
  TestBlock right(this, "right");
  TestBlock station_lr(this, "station_lr");
  TestBlock station_rl(this, "station_rl");
  TestFixedTurnout turnout_l(this, FixedTurnout::TURNOUT_THROWN, "turnout_l");
  TestFixedTurnout turnout_r(this, FixedTurnout::TURNOUT_CLOSED, "turnout_r");

  BindSequence({&end_left.b, &mid_left.b, &left.b});
  BindSequence({&end_right.b, &mid_right.b, &right.b});
  left.b.side_b()->Bind(turnout_l.b.side_points());
  right.b.side_b()->Bind(turnout_r.b.side_points());

  station_lr.b.side_a()->Bind(turnout_l.b.side_thrown());
  station_lr.b.side_b()->Bind(turnout_r.b.side_thrown());

  station_rl.b.side_b()->Bind(turnout_l.b.side_closed());
  station_rl.b.side_a()->Bind(turnout_r.b.side_closed());

  // This will immediately accept a route at the end stop.
  DefAut(strategyaut, brd, {
    LocalVariable* try_set_route =
        ImportVariable(end_left.b.side_b()->binding()->out_try_set_route.get());
    LocalVariable* route_set_succcess =
        ImportVariable(end_left.b.side_b()->in_route_set_success.get());
    Def().IfReg1(*try_set_route).ActReg0(try_set_route).ActReg1(
        route_set_succcess);
    try_set_route = ImportVariable(
        end_right.b.side_b()->binding()->out_try_set_route.get());
    route_set_succcess =
        ImportVariable(end_right.b.side_b()->in_route_set_success.get());
    Def().IfReg1(*try_set_route).ActReg0(try_set_route).ActReg1(
        route_set_succcess);
  });

  SetupRunner(&brd);
  // No trains anywhere.
  end_left.detector.Set(false);
  end_right.detector.Set(false);
  left.inverted_detector.Set(true);
  right.inverted_detector.Set(true);
  station_lr.inverted_detector.Set(true);
  station_rl.inverted_detector.Set(true);
  Run(1);
  left.inverted_detector.Set(true);
  right.inverted_detector.Set(true);
  station_lr.inverted_detector.Set(true);
  station_rl.inverted_detector.Set(true);
  Run(15);

  EXPECT_FALSE(QueryVar(*left.b.body_det_.simulated_occupancy_));

  // Source a train from left and right.
  end_left.detector.Set(true);
  end_right.detector.Set(true);
  SetVar(*end_left.b.side_b()->out_try_set_route, true);
  SetVar(*end_right.b.side_b()->out_try_set_route, true);
  Run(12);
  EXPECT_FALSE(QueryVar(*end_left.b.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*end_left.b.side_b()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(*left.b.body_det_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*mid_left.b.route_set_ab_));
  EXPECT_FALSE(QueryVar(*end_right.b.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*end_right.b.side_b()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(*mid_right.b.route_set_ab_));
  EXPECT_TRUE(QueryVar(*right.b.body_det_.route_set_ab_));

  // Both trains reach their stops on open track outside of the station.
  end_left.detector.Set(false);
  end_right.detector.Set(false);
  left.inverted_detector.Set(false);
  right.inverted_detector.Set(false);
  Run(12);

  EXPECT_FALSE(QueryVar(*mid_left.b.route_set_ab_));
  EXPECT_FALSE(QueryVar(*mid_right.b.route_set_ab_));

  EXPECT_FALSE(QueryVar(*left.b.signal_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*right.b.signal_.route_set_ab_));

  // Give them green.
  LOG(INFO, "Giving green to left train");
  EXPECT_TRUE(QueryVar(*turnout_l.b.turnout_state_));
  EXPECT_FALSE(QueryVar(*turnout_r.b.turnout_state_));
  SetVar(*left.b.request_green(), true);
  Run(5);
  EXPECT_TRUE(left.signal_green.Get());
  EXPECT_TRUE(QueryVar(*left.b.signal_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*right.b.signal_.route_set_ab_));

  EXPECT_TRUE(QueryVar(*left.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*turnout_l.b.route_set_PT_));
  EXPECT_TRUE(QueryVar(*turnout_l.b.any_route_set_));

  EXPECT_FALSE(QueryVar(*turnout_r.b.any_route_set_));
  EXPECT_TRUE(QueryVar(*station_lr.b.body_det_.route_set_ab_));

  left.inverted_detector.Set(true);
  Run(15);

  EXPECT_FALSE(QueryVar(*left.b.body_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*turnout_l.b.any_route_set_));

  // Arrives in the station.
  station_lr.inverted_detector.Set(false);
  Run(15);

  EXPECT_FALSE(QueryVar(*left.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*station_lr.b.body_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*turnout_l.b.any_route_set_));

  // But cannot go out to the right because there is a train there.
  SetVar(*station_lr.b.request_green(), true);
  Run(10);
  EXPECT_FALSE(QueryVar(*station_lr.b.signal_.route_set_ab_));

  // Right train comes in.
  SetVar(*right.b.request_green(), true);
  Run(5);
  EXPECT_TRUE(QueryVar(*right.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*turnout_r.b.any_route_set_));
  EXPECT_TRUE(QueryVar(*turnout_r.b.route_set_PC_));
  EXPECT_TRUE(QueryVar(*station_rl.b.body_.route_set_ab_));
  right.inverted_detector.Set(true);
  station_rl.inverted_detector.Set(false);
  Run(15);
  EXPECT_FALSE(QueryVar(*turnout_r.b.any_route_set_));
  EXPECT_FALSE(QueryVar(*turnout_r.b.route_set_PC_));
  EXPECT_FALSE(QueryVar(*right.b.signal_.route_set_ab_));

  // Left->Right train goes out.
  SetVar(*station_lr.b.request_green(), true);
  Run(10);
  EXPECT_TRUE(QueryVar(*station_lr.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*mid_right.b.route_set_ba_));
  EXPECT_TRUE(QueryVar(*right.b.body_.route_set_ba_));

  right.inverted_detector.Set(false);
  Run(15);
  EXPECT_TRUE(QueryVar(*turnout_r.b.any_route_set_));

  right.inverted_detector.Set(true);
  Run(15);
  EXPECT_FALSE(QueryVar(*turnout_r.b.any_route_set_));
  EXPECT_TRUE(QueryVar(*mid_right.b.route_set_ba_));
  EXPECT_TRUE(QueryVar(*right.b.body_.route_set_ba_));
  end_right.detector.Set(true);
  Run(15);
  EXPECT_TRUE(QueryVar(*mid_right.b.route_set_ba_));
  EXPECT_TRUE(QueryVar(*right.b.body_.route_set_ba_));
  right.inverted_detector.Set(false);
  Run(15);
  EXPECT_TRUE(QueryVar(*mid_right.b.route_set_ba_));
  EXPECT_TRUE(QueryVar(*right.b.body_.route_set_ba_));
  end_right.detector.Set(false);
  Run(15);
  EXPECT_FALSE(QueryVar(*mid_right.b.route_set_ba_));
  EXPECT_FALSE(QueryVar(*right.b.body_.route_set_ba_));
}

//                  b         a
//     b   a    ----<<station_1-----     b    a
// ----<left---/----<<station_2-----\----<right---
TEST_F(LogicTest, MovableTurnout) {
  static TestDetectorBlock end_left(this, "end_left");
  static TestDetectorBlock end_right(this, "end_right");
  TestShortTrack mid_right(this, "mid_right");
  TestBlock left(this, "left");
  TestBlock right(this, "right");
  TestBlock station_1(this, "station_1");
  TestBlock station_2(this, "station_2");
  TestMovableTurnout turnout_r(this, "turnout_r");
  TestFixedTurnout turnout_l(this, FixedTurnout::TURNOUT_CLOSED, "turnout_l");

  BindSequence({&left.b, &end_left.b});
  BindSequence({&end_right.b, &mid_right.b, &right.b});
  station_1.b.side_b()->Bind(turnout_l.b.side_thrown());
  station_1.b.side_a()->Bind(turnout_r.b.side_thrown());
  station_2.b.side_b()->Bind(turnout_l.b.side_closed());
  station_2.b.side_a()->Bind(turnout_r.b.side_closed());

  left.b.side_a()->Bind(turnout_l.b.side_points());
  right.b.side_b()->Bind(turnout_r.b.side_points());

  SetupRunner(&brd);
  // No trains anywhere.
  end_right.detector.Set(false);
  end_left.detector.Set(false);
  left.inverted_detector.Set(true);
  right.inverted_detector.Set(true);
  station_1.inverted_detector.Set(true);
  station_2.inverted_detector.Set(true);
  // Turnout closed.
  SetVar(*turnout_r.magnet.command, false);
  SetVar(*turnout_r.magnet.current_state, false);
  Run(1);
  left.inverted_detector.Set(true);
  right.inverted_detector.Set(true);
  station_1.inverted_detector.Set(true);
  station_2.inverted_detector.Set(true);
  Run(15);

  // Test turnout detector proxy.
  EXPECT_FALSE(QueryVar(*turnout_r.b.side_points()->LookupNextDetector()));
  EXPECT_FALSE(QueryVar(*turnout_r.b.side_points()->LookupFarDetector()));
  station_2.inverted_detector.Set(false);
  Run(15);
  EXPECT_TRUE(QueryVar(*turnout_r.b.side_points()->LookupNextDetector()));
  EXPECT_TRUE(QueryVar(*turnout_r.b.side_points()->LookupFarDetector()));
  station_2.inverted_detector.Set(true);
  Run(15);
  EXPECT_FALSE(QueryVar(*turnout_r.b.side_points()->LookupNextDetector()));
  EXPECT_FALSE(QueryVar(*turnout_r.b.side_points()->LookupFarDetector()));


  for (int i = 0; i < 2; i++) {
  // Source a train from the right.
  end_right.detector.Set(true);
  SetVar(*end_right.b.side_b()->out_try_set_route, true);
  Run(12);
  EXPECT_FALSE(QueryVar(*end_right.b.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*end_right.b.side_b()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(*mid_right.b.route_set_ab_));
  EXPECT_TRUE(QueryVar(*right.b.body_det_.route_set_ab_));

  // Train reaches its stops on open track outside of the station.
  end_right.detector.Set(false);
  right.inverted_detector.Set(false);
  Run(12);

  EXPECT_FALSE(QueryVar(*mid_right.b.route_set_ab_));
  EXPECT_FALSE(QueryVar(*right.b.signal_.route_set_ab_));

  LOG(INFO, "Giving green to first train");
  SetVar(*right.b.request_green(), true);
  Run(5);
  EXPECT_FALSE(QueryVar(*right.b.request_green()));
  EXPECT_TRUE(QueryVar(*right.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*turnout_r.b.any_route_set_));
  EXPECT_TRUE(QueryVar(*turnout_r.b.route_set_PC_));
  EXPECT_TRUE(QueryVar(*station_2.b.body_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*station_2.b.body_det_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*station_2.b.signal_.route_set_ab_));
  right.inverted_detector.Set(true);
  station_2.inverted_detector.Set(false);
  Run(15);
  EXPECT_FALSE(QueryVar(*turnout_r.b.any_route_set_));
  EXPECT_FALSE(QueryVar(*turnout_r.b.route_set_PC_));
  EXPECT_FALSE(QueryVar(*right.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*station_2.b.body_det_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*station_2.b.signal_.route_set_ab_));

  // Source another train.
  end_right.detector.Set(true);
  SetVar(*end_right.b.side_b()->out_try_set_route, true);
  Run(12);
  EXPECT_FALSE(QueryVar(*end_right.b.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(*end_right.b.side_b()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(*mid_right.b.route_set_ab_));
  EXPECT_TRUE(QueryVar(*right.b.body_det_.route_set_ab_));

  // Train 2 reaches its stop on open track outside of the station.
  end_right.detector.Set(false);
  right.inverted_detector.Set(false);
  Run(12);

  EXPECT_FALSE(QueryVar(*mid_right.b.route_set_ab_));
  EXPECT_FALSE(QueryVar(*right.b.signal_.route_set_ab_));

  EXPECT_TRUE(QueryVar(*station_2.b.body_det_.route_set_ab_));

  LOG(INFO, "Trying green to second train");
  SetVar(*right.b.request_green(), true);
  Run(5);
  EXPECT_FALSE(QueryVar(*right.b.side_b()->out_try_set_route));
  EXPECT_FALSE(QueryVar(*right.b.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(*right.b.signal_.route_set_ab_));

  // Switch the turnout.
  SetVar(*turnout_r.magnet.command, true);
  Run(5);
  EXPECT_TRUE(turnout_r.set_1.Get());
  Run(5);
  EXPECT_FALSE(turnout_r.set_1.Get());

  // Give green again, this time it works.
  SetVar(*right.b.request_green(), true);
  Run(5);
  EXPECT_FALSE(QueryVar(*right.b.request_green()));
  EXPECT_TRUE(QueryVar(*right.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*turnout_r.b.any_route_set_));
  EXPECT_TRUE(QueryVar(*turnout_r.b.route_set_PT_));
  EXPECT_TRUE(QueryVar(*station_1.b.body_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*station_1.b.body_det_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*station_1.b.signal_.route_set_ab_));
  right.inverted_detector.Set(true);
  station_1.inverted_detector.Set(false);
  Run(15);
  EXPECT_FALSE(QueryVar(*turnout_r.b.any_route_set_));
  EXPECT_FALSE(QueryVar(*turnout_r.b.route_set_PT_));
  EXPECT_FALSE(QueryVar(*right.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*station_1.b.body_det_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*station_1.b.signal_.route_set_ab_));

  // Get train 1 out.
  SetVar(*station_1.b.request_green(), true);
  Run(5);
  EXPECT_TRUE(QueryVar(*station_1.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*left.b.body_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*turnout_l.b.any_route_set_));
  left.inverted_detector.Set(false);
  station_1.inverted_detector.Set(true);
  Run(15);
  EXPECT_FALSE(QueryVar(*turnout_l.b.any_route_set_));

  SetVar(*station_2.b.request_green(), true);
  Run(5);
  EXPECT_TRUE(QueryVar(*left.b.body_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*station_2.b.signal_.route_set_ab_));

  // And we will make the train disappear from the left block.
  left.inverted_detector.Set(true);
  Run(15);
  EXPECT_FALSE(QueryVar(*left.b.body_.route_set_ab_));

  // So that train 2 can leave too
  SetVar(*station_2.b.request_green(), true);
  Run(5);
  EXPECT_TRUE(QueryVar(*left.b.body_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*station_2.b.signal_.route_set_ab_));
  EXPECT_TRUE(QueryVar(*turnout_l.b.any_route_set_));
  left.inverted_detector.Set(false);
  station_2.inverted_detector.Set(true);
  Run(15);

  EXPECT_TRUE(QueryVar(*left.b.body_.route_set_ab_));
  EXPECT_FALSE(QueryVar(*station_2.b.signal_.route_set_ab_));

  // Reset the turnout and make seocnd train disappear.
  left.inverted_detector.Set(true);
  SetVar(*turnout_r.magnet.command, false);
  Run(15);
  }  // rinse repeat.
}

TEST_F(LogicTest, DISABLED_100trainz) {
  TestDetectorBlock before(this, "before");
  static StraightTrackLong mid(alloc());
  DefAut(autmid, brd, { mid.RunAll(this); });
  TestBlock first(this, "first");
  TestBlock second(this, "second");
  static StraightTrackLong after(alloc());

  BindSequence({&before.b, &first.b, &mid, &second.b, &after});


  // This will immediately accept a route at the end stop.
  DefAut(strategyaut, brd, {
    LocalVariable* try_set_route =
        ImportVariable(after.side_a()->binding()->out_try_set_route.get());
    LocalVariable* route_set_succcess =
        ImportVariable(after.side_a()->in_route_set_success.get());
    Def().IfReg1(*try_set_route).ActReg0(try_set_route).ActReg1(
        route_set_succcess);
  });

  SetupRunner(&brd);
  before.detector.Set(true);
  // The initial state is a train at both blocks.
  first.inverted_detector.Set(false);
  second.inverted_detector.Set(false);
  Run(1);
  first.inverted_detector.Set(false);
  second.inverted_detector.Set(false);
  Run(20);
  EXPECT_TRUE(QueryVar(*first.b.body_det_.simulated_occupancy_));
  EXPECT_TRUE(QueryVar(*second.b.body_det_.simulated_occupancy_));

  for (int i = 0; i < 100; ++i) {
    Run(10);
    EXPECT_FALSE(first.signal_green.Get());
    EXPECT_FALSE(second.signal_green.Get());

    // The rear train cannot move forward.
    SetVar(*first.b.request_green(), true);
    Run(10);
    EXPECT_FALSE(QueryVar(*first.b.request_green()));
    EXPECT_FALSE(first.signal_green.Get());
    EXPECT_FALSE(QueryVar(*mid.route_set_ab_));

    // Makes the second train leave.
    SetVar(*second.b.request_green(), true);
    Run(10);
    EXPECT_FALSE(QueryVar(*second.b.request_green()));
    EXPECT_TRUE(second.signal_green.Get());

    // The rear train still cannot move forward.
    SetVar(*first.b.request_green(), true);
    Run(10);
    EXPECT_FALSE(QueryVar(*first.b.request_green()));
    EXPECT_FALSE(first.signal_green.Get());
    EXPECT_FALSE(QueryVar(*mid.route_set_ab_));

    // Makes the front train leave.
    second.inverted_detector.Set(true);
    Run(14);
    EXPECT_FALSE(QueryVar(*second.b.request_green()));
    EXPECT_FALSE(second.signal_green.Get());
    EXPECT_FALSE(QueryVar(*second.b.signal_.route_set_ab_));

    // Now we can run the rear train.
    SetVar(*first.b.request_green(), true);
    Run(10);
    EXPECT_FALSE(QueryVar(*first.b.request_green()));
    EXPECT_TRUE(QueryVar(*first.b.signal_.route_set_ab_));
    EXPECT_TRUE(first.signal_green.Get());
    EXPECT_TRUE(QueryVar(*mid.route_set_ab_));
    EXPECT_TRUE(QueryVar(*second.b.body_det_.route_set_ab_));
    EXPECT_FALSE(QueryVar(*second.b.signal_.route_set_ab_));

    // But the third train cannot get in yet,
    SetVar(*first.b.body_.side_b()->out_try_set_route, true);
    Run(5);
    EXPECT_FALSE(QueryVar(*first.b.body_.side_b()->out_try_set_route));
    EXPECT_TRUE(QueryVar(*first.b.body_.side_b()->binding()->in_route_set_failure));
    if (i != 0) {
      EXPECT_TRUE(
          QueryVar(*first.b.body_det_.route_set_ab_));  // because the route is still there.
    }

    // Rear train leaves its block.
    first.inverted_detector.Set(true);
    Run(15);
    EXPECT_FALSE(QueryVar(*first.b.signal_.route_set_ab_));  // route gone.
    EXPECT_FALSE(QueryVar(*first.b.body_det_.route_set_ab_));
    EXPECT_FALSE(QueryVar(*first.b.body_.route_set_ab_));

    // The third train can now come.
    SetVar(*first.b.body_.side_b()->out_try_set_route, true);
    Run(5);
    EXPECT_FALSE(QueryVar(*first.b.body_.side_b()->out_try_set_route));
    EXPECT_FALSE(
        QueryVar(*first.b.body_.side_b()->binding()->in_route_set_failure));
    EXPECT_TRUE(QueryVar(*first.b.body_.side_b()->binding()->in_route_set_success));
    EXPECT_FALSE(QueryVar(*first.b.signal_.route_set_ab_));
    EXPECT_TRUE(QueryVar(*first.b.body_det_.route_set_ab_));

    // Second train reaches rear block.
    second.inverted_detector.Set(false);
    Run(15);
    EXPECT_FALSE(QueryVar(*mid.route_set_ab_));
    EXPECT_TRUE(QueryVar(*second.b.body_det_.route_set_ab_));
    EXPECT_FALSE(QueryVar(*second.b.signal_.route_set_ab_));

    // Third train reaches second block.
    first.inverted_detector.Set(false);
    Run(15);
  }
}

TEST_F(LogicTest, Magnets) {
  FakeBit v0s0(this);
  FakeBit v0s1(this);
  FakeBit v1s0(this);
  FakeBit v1s1(this);

  MagnetCommandAutomata aut(&brd, alloc());
  MagnetDef def0(&aut, "v0", &v0s0, &v0s1);
  MagnetDef def1(&aut, "v1", &v1s0, &v1s1);

  v0s0.Set(true);
  v1s1.Set(true);

  SetVar(*def0.command, false);
  SetVar(*def1.command, false);

  SetupRunner(&brd);
  Run(40); // should be enough time for reset behavior.
  EXPECT_FALSE(v0s0.Get());
  EXPECT_FALSE(v0s1.Get());
  EXPECT_FALSE(v1s0.Get());
  EXPECT_FALSE(v1s1.Get());

  Run(5);  // adds one tick
  // No command
  EXPECT_FALSE(v0s0.Get());
  EXPECT_FALSE(v0s1.Get());
  EXPECT_FALSE(v1s0.Get());
  EXPECT_FALSE(v1s1.Get());
  
  SetVar(*def1.command, true);
  Run(2);
  // Tick is not done yet.
  EXPECT_FALSE(v0s0.Get());
  EXPECT_FALSE(v0s1.Get());
  EXPECT_FALSE(v1s0.Get());
  EXPECT_FALSE(v1s1.Get());

  Run(3);
  // Tick is here, command taken
  EXPECT_FALSE(v0s0.Get());
  EXPECT_FALSE(v0s1.Get());
  EXPECT_FALSE(v1s0.Get());
  EXPECT_TRUE(v1s1.Get());

  Run(2);
  // Next tick not here yet
  EXPECT_FALSE(v0s0.Get());
  EXPECT_FALSE(v0s1.Get());
  EXPECT_FALSE(v1s0.Get());
  EXPECT_TRUE(v1s1.Get());

  Run(3);
  // Next tick: turns off signal
  EXPECT_FALSE(v0s0.Get());
  EXPECT_FALSE(v0s1.Get());
  EXPECT_FALSE(v1s0.Get());
  EXPECT_FALSE(v1s1.Get());

  // Now we command both together.
  SetVar(*def0.command, true);
  SetVar(*def1.command, false);

  Run();
  EXPECT_FALSE(v0s0.Get());
  EXPECT_FALSE(v0s1.Get());
  EXPECT_FALSE(v1s0.Get());
  EXPECT_FALSE(v1s1.Get());

  Run(3);
  // First signal comes.
  EXPECT_FALSE(v0s0.Get());
  EXPECT_TRUE(v0s1.Get());
  EXPECT_FALSE(v1s0.Get());
  EXPECT_FALSE(v1s1.Get());

  Run(3);
  // second signal comes.
  EXPECT_FALSE(v0s0.Get());
  EXPECT_FALSE(v0s1.Get());
  EXPECT_TRUE(v1s0.Get());
  EXPECT_FALSE(v1s1.Get());

  Run(3);
  // All done.
  EXPECT_FALSE(v0s0.Get());
  EXPECT_FALSE(v0s1.Get());
  EXPECT_FALSE(v1s0.Get());
  EXPECT_FALSE(v1s1.Get());
}

class LogicTrainTest : public LogicTest, protected TrainTestHelper {
 protected:
  LogicTrainTest() : TrainTestHelper(this) {}
};


TEST_F(LogicTrainTest, ScheduleStraight) {
  TestDetectorBlock before(this, "before");
  static StraightTrackLong mid(alloc());
  DefAut(autmid, brd, { mid.RunAll(this); });
  static TestBlock first(this, "first");
  static TestBlock second(this, "second");
  static StraightTrackLong after(alloc());

  BindSequence({&before.b, &first.b, &mid, &second.b, &after});

  ASSERT_EQ(first.b.signal_.side_b(), mid.side_a()->binding());

  static FakeBit mcont(this);

  class MyTrain : public TrainSchedule {
   public:
    MyTrain(Board* b, EventBlock::Allocator* alloc)
        : TrainSchedule("mytrain", b, nmranet::TractionDefs::NODE_ID_DCC | 0x1384, alloc->Allocate("mytrain.pbits", 8),
                        alloc->Allocate("mytrain", 8)) {}

    void RunTransition(Automata* aut) OVERRIDE {
      auto* cv = aut->ImportVariable(&mcont);
      Def()
          .IfState(StWaiting)
          .IfReg1(*cv)
          .ActState(StRequestGreen)
          .ActReg0(cv);
      Def()
          .ActImportVariable(*first.b.request_green(),
                             current_block_request_green_)
          .ActImportVariable(first.b.route_out(),
                             current_block_route_out_)
          .ActImportVariable(second.b.detector(),
                             next_block_detector_);
    }

  } train_aut(&brd, block_.allocator());

  SetupRunner(&brd);
  
  // No trains initially.
  first.inverted_detector.Set(true);
  second.inverted_detector.Set(true);
  Run(1);

  // No trains initially.
  first.inverted_detector.Set(true);
  second.inverted_detector.Set(true);
  Run(20);
  EXPECT_FALSE(QueryVar(*first.b.body_det_.simulated_occupancy_));
  EXPECT_FALSE(QueryVar(*second.b.body_det_.simulated_occupancy_));

  // Train is at first. No green yet.
  mcont.Set(false);
  first.inverted_detector.Set(false);
  second.inverted_detector.Set(true);
  Run(30);
  LOG(INFO, "Sending off train.");

  EXPECT_EQ(0, trainImpl_.get_speed().speed());
  mcont.Set(true);
  Run(20);
  EXPECT_TRUE(first.signal_green.Get());
  EXPECT_FALSE(mcont.Get());
  wait();
  EXPECT_EQ(40, (int)(trainImpl_.get_speed().mph() + 0.5));

  LOG(INFO, "Train arriving at destination.");
  first.inverted_detector.Set(true);
  second.inverted_detector.Set(false);
  Run(20);

  wait();
  EXPECT_EQ(0, trainImpl_.get_speed().mph());
}

// This is the test layout. It's a dogbone (a loop) with 6 standard blocks.
//
//   >Rleft
//  /--\  TopA>TopB  /---\   x  
//  |   \-----------/    |   x
//  |   /-----------\    |   x
//  \--/  BotB<BotA  \---/   x
//                   <RRight
//
//

class SampleLayoutLogicTrainTest : public LogicTrainTest {
 protected:
  SampleLayoutLogicTrainTest()
      : RLeft(this, "RLeft"),
        TopA(this, "TopA"),
        TopB(this, "TopB"),
        RRight(this, "RRight"),
        BotA(this, "BotA"),
        BotB(this, "BotB") {}

  TestBlock RLeft, TopA, TopB, RRight, BotA, BotB;
};



}  // namespace automata
