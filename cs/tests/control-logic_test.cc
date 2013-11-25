#include <unistd.h>

#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "automata_tests_helper.hxx"
#include "../automata/control-logic.hxx"

#include "utils/test_main.hxx"

namespace automata {

class LogicTest : public AutomataNodeTests {
 protected:
  LogicTest()
      : block_(&brd, BRACZ_LAYOUT|0xC000, "blk") {}

  const EventBlock::Allocator& alloc() { return *block_.allocator(); }

  friend struct TestBlock;

  struct TestBlock {
    TestBlock(LogicTest* test, const string& name)
        : inverted_detector(test),
          signal_green(test),
          physical_signal(&inverted_detector, &signal_green),
          b(&test->brd, &physical_signal, EventBlock::Allocator(&test->alloc(), name, 80)) {}
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

  Board brd;
  EventBlock block_;
};

TEST_F(LogicTest, TestFramework) {
  static EventBasedVariable ev_var(
      &brd, "test", BRACZ_LAYOUT | 0xf000, BRACZ_LAYOUT | 0xf001, 0);
  static FakeBit input(this);
  DefAut(testaut, brd, {
    DefCopy(ImportVariable(input), ImportVariable(&ev_var));
  });
  EventListener listen_event(ev_var);
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
  EventListener ldst(*v);
  EventListener lsrc(*w);
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

  EventListener sim_occ(*piece.simulated_occupancy_);
  EventListener seen_train(*piece.tmp_seen_train_in_next_);
  EventListener released(*piece.side_b()->out_route_released);

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

  EventListener sim_occ(*piece.simulated_occupancy_);
  EventListener sim_occ2(*piece2.simulated_occupancy_);

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

  EventListener sim_occ(*piece.simulated_occupancy_);
  EventListener sim_occ2(*piece2.simulated_occupancy_);
  EventListener sim_occ3(*piece3.simulated_occupancy_);
  EventListener sim_occ4(*piece4.simulated_occupancy_);

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



TEST_F(LogicTest, FixedTurnout) {
  


}

/*
TEST_F(LogicTest, 100trainz) {
  FakeBit previous_detector(this);
  FakeBit request_green(this);
  FakeBit first.signal_green(this);
  FakeBit second.detector(this);
  FakeBit request_green2(this);
  FakeBit signal_green2(this);
  static StraightTrackLong first.b.body_(alloc());
  static StraightTrackWithDetector first.b.body_det_(
      alloc(), &previous_detector);
  static SignalPiece signal(
      alloc(), &request_green, &first.signal_green);
  static StraightTrackLong mid(alloc());
  static StraightTrackLong second_body(alloc());
  static StraightTrackWithDetector second_det(
      alloc(), &second.detector);
  static SignalPiece second_signal(
      alloc(), &request_green2, &signal_green2);
  static StraightTrackLong after(alloc());

  BindSequence({&first.b.body_,    &first.b.body_det_,   &signal,
                               &mid,           &second_body, &second_det,
                               &second_signal, &after});

#define DefPiece(pp) \
  StandardPluginAutomata aut##pp(&brd, &pp);

  DefPiece(first.b.body_det_);
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

  // The initial state is a train at both blocks.
  first.inverted_detector.Set(true);
  second.inverted_detector.Set(true);

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
    second.request_green.Set(true);
    Run(10);
    EXPECT_FALSE(second.request_green.Get());
    EXPECT_TRUE(second.signal_green.Get());

    // The rear train still cannot move forward.
    SetVar(*first.b.request_green(), true);
    Run(10);
    EXPECT_FALSE(QueryVar(*first.b.request_green()));
    EXPECT_FALSE(first.signal_green.Get());
    EXPECT_FALSE(QueryVar(*mid.route_set_ab_));

    // Makes the front train leave.
    second.inverted_detector.Set(false);
    Run(10);
    EXPECT_FALSE(second.request_green.Get());
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
    first.inverted_detector.Set(false);
    Run(10);
    EXPECT_FALSE(QueryVar(*first.b.signal_.route_set_ab_));  // route gone.
    EXPECT_FALSE(QueryVar(*first.b.body_det_.route_set_ab_));

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
    second.inverted_detector.Set(true);
    Run(10);
    EXPECT_FALSE(QueryVar(*mid.route_set_ab_));
    EXPECT_TRUE(QueryVar(*second.b.body_det_.route_set_ab_));
    EXPECT_FALSE(QueryVar(*second.b.signal_.route_set_ab_));

    // Third train reaches second block.
    first.inverted_detector.Set(true);
    Run(10);
  }
}
*/
}  // namespace automata
