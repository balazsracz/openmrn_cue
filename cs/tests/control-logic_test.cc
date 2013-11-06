#include <unistd.h>

#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "automata_tests_helper.hxx"

#include "../automata/control-logic.hxx"

namespace automata {

TEST_F(AutomataNodeTests, TestFramework) {
  Board brd;
  static EventBasedVariable ev_var(
      &brd, BRACZ_LAYOUT | 0xf000, BRACZ_LAYOUT | 0xf001, 0);
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

TEST_F(AutomataNodeTests, TestFramework2) {
  Board brd;
  static EventBasedVariable ev_var(
      &brd, BRACZ_LAYOUT | 0xf000, BRACZ_LAYOUT | 0xf001, 0);
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

TEST_F(AutomataNodeTests, SimulatedOccupancy_SingleShortPiece) {
  Board brd;
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 32);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      &brd, BRACZ_LAYOUT | 0x5100, 64, &previous_detector);
  StraightTrackWithDetector after(
      &brd, BRACZ_LAYOUT | 0x5140, 96, &next_detector);
  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(after.side_a());

  EventListener sim_occ(piece.simulated_occupancy_);
  EventListener seen_train(piece.tmp_seen_train_in_next_);
  EventListener released(piece.side_b()->out_route_released);

  DefAut(strategyaut, brd, { piece.SimulateAllOccupancy(this); });

  EXPECT_FALSE(sim_occ.value());
  SetupRunner(&brd);
  Run();
  previous_detector.Set(true);
  next_detector.Set(false);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(seen_train.value());
  SetVar(piece.route_set_ab_, true);
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

TEST_F(AutomataNodeTests, SimulatedOccupancy_MultipleShortPiece) {
  Board brd;
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 32);
  static StraightTrackShort piece2(&brd, BRACZ_LAYOUT | 0x5040, 128);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      &brd, BRACZ_LAYOUT | 0x5100, 64, &previous_detector);
  StraightTrackWithDetector after(
      &brd, BRACZ_LAYOUT | 0x5140, 96, &next_detector);
  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(piece2.side_a());
  piece2.side_b()->Bind(after.side_a());

  EventListener sim_occ(piece.simulated_occupancy_);
  EventListener sim_occ2(piece2.simulated_occupancy_);

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
  SetVar(piece.route_set_ab_, true);
  SetVar(piece2.route_set_ab_, true);
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

  SetVar(piece.route_set_ab_, false);
  SetVar(piece2.route_set_ab_, false);
  previous_detector.Set(false);
  Run();

  // Now we go backwards!
  SetVar(piece.route_set_ba_, true);
  SetVar(piece2.route_set_ba_, true);
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

TEST_F(AutomataNodeTests, SimulatedOccupancy_ShortAndLongPieces) {
  Board brd;
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 1 * 32);
  static StraightTrackShort piece2(&brd, BRACZ_LAYOUT | 0x5040, 4 * 32);
  static StraightTrackLong piece3(&brd, BRACZ_LAYOUT | 0x5080, 5 * 32);
  static StraightTrackShort piece4(&brd, BRACZ_LAYOUT | 0x50C0, 6 * 32);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      &brd, BRACZ_LAYOUT | 0x5100, 2 * 32, &previous_detector);
  StraightTrackWithDetector after(
      &brd, BRACZ_LAYOUT | 0x5140, 3 * 32, &next_detector);
  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(piece2.side_a());
  piece2.side_b()->Bind(piece3.side_a());
  piece3.side_b()->Bind(piece4.side_a());
  piece4.side_b()->Bind(after.side_a());

  EventListener sim_occ(piece.simulated_occupancy_);
  EventListener sim_occ2(piece2.simulated_occupancy_);
  EventListener sim_occ3(piece3.simulated_occupancy_);
  EventListener sim_occ4(piece4.simulated_occupancy_);

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
  SetVar(piece.route_set_ab_, true);
  SetVar(piece2.route_set_ab_, true);
  SetVar(piece3.route_set_ab_, true);
  SetVar(piece4.route_set_ab_, true);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_FALSE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());

  previous_detector.Set(true);
  next_detector.Set(false);
  Run();

  EXPECT_TRUE(sim_occ.value());
  EXPECT_TRUE(sim_occ2.value());
  EXPECT_TRUE(sim_occ3.value());
  EXPECT_TRUE(sim_occ4.value());

  previous_detector.Set(false);
  next_detector.Set(true);
  Run();

  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_TRUE(sim_occ3.value());
  EXPECT_TRUE(sim_occ4.value());

  next_detector.Set(false);
  Run();

  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_FALSE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());

  // Run backwards.
  SetVar(piece.route_set_ab_, false);
  SetVar(piece2.route_set_ab_, false);
  SetVar(piece3.route_set_ab_, false);
  SetVar(piece4.route_set_ab_, false);
  Run();
  SetVar(piece.route_set_ba_, true);
  SetVar(piece2.route_set_ba_, true);
  SetVar(piece3.route_set_ba_, true);
  SetVar(piece4.route_set_ba_, true);
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
  EXPECT_TRUE(QueryVar(piece.simulated_occupancy_));
  EXPECT_TRUE(sim_occ2.value());
  EXPECT_TRUE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());

  previous_detector.Set(false);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(QueryVar(piece.simulated_occupancy_));
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_FALSE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());
}

TEST_F(AutomataNodeTests, SimulatedOccupancy_RouteSetting) {
  Board brd;
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 1 * 32);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      &brd, BRACZ_LAYOUT | 0x5040, 2 * 32, &previous_detector);
  StraightTrackWithDetector after(
      &brd, BRACZ_LAYOUT | 0x5080, 3 * 32, &next_detector);

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

  EXPECT_FALSE(QueryVar(piece.route_set_ba_));
  EXPECT_FALSE(QueryVar(piece.route_set_ab_));
  EXPECT_FALSE(QueryVar(piece.tmp_route_setting_in_progress_));
  // An incoming route request here.
  SetVar(before.side_b()->out_try_set_route, true);

  Run();

  // Should be propagated.
  EXPECT_TRUE(QueryVar(piece.side_b()->out_try_set_route));
  EXPECT_FALSE(QueryVar(piece.side_a()->in_route_set_success));
  EXPECT_TRUE(QueryVar(piece.tmp_route_setting_in_progress_));

  Run();
  EXPECT_TRUE(QueryVar(piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(QueryVar(before.side_b()->out_try_set_route));
  Run();

  EXPECT_EQ(after.side_a()->binding(), piece.side_b());
  EXPECT_EQ(before.side_b()->binding(), piece.side_a());
  EXPECT_EQ(after.side_a(), piece.side_b()->binding());
  EXPECT_EQ(before.side_b(), piece.side_a()->binding());
  EXPECT_TRUE(QueryVar(after.side_a()->binding()->out_try_set_route));
  EXPECT_FALSE(QueryVar(before.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(before.side_b()->binding()->in_route_set_failure));
  EXPECT_TRUE(QueryVar(piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(QueryVar(piece.simulated_occupancy_));
  EXPECT_FALSE(QueryVar(piece.route_set_ab_));
  EXPECT_FALSE(QueryVar(piece.route_set_ba_));

  SetVar(after.side_a()->in_route_set_success, true);
  SetVar(after.side_a()->binding()->out_try_set_route, false);
  Run();
  Run();

  EXPECT_FALSE(QueryVar(piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(SQueryVar(after.side_a()->binding()->out_try_set_route));
  EXPECT_TRUE(QueryVar(before.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(before.side_b()->binding()->in_route_set_failure));
  EXPECT_FALSE(QueryVar(before.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(piece.route_set_ab_));
  EXPECT_FALSE(QueryVar(piece.route_set_ba_));

  // Now a second route setting should fail.
  SetVar(before.side_b()->out_try_set_route, true);

  Run();
  Run();

  EXPECT_FALSE(QueryVar(before.side_b()->out_try_set_route));
  EXPECT_FALSE(SQueryVar(before.side_b()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(before.side_b()->binding()->in_route_set_failure));

  // And an opposite route should fail too.
  SetVar(after.side_a()->out_try_set_route, true);

  Run();
  Run();

  EXPECT_FALSE(QueryVar(after.side_a()->out_try_set_route));
  EXPECT_FALSE(SQueryVar(after.side_a()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(after.side_a()->binding()->in_route_set_failure));
}

TEST_F(AutomataNodeTests, ReverseRoute) {
  Board brd;
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 1 * 32);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      &brd, BRACZ_LAYOUT | 0x5040, 2 * 32, &previous_detector);
  StraightTrackWithDetector after(
      &brd, BRACZ_LAYOUT | 0x5080, 3 * 32, &next_detector);

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
  SetVar(after.side_a()->out_try_set_route, true);

  Run();

  // Debug
  EXPECT_FALSE(QueryVar(before.side_b()->out_try_set_route));
  EXPECT_FALSE(QueryVar(piece.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(QueryVar(piece.route_pending_ab_));
  EXPECT_TRUE(QueryVar(piece.route_pending_ba_));

  // Should be propagated.
  EXPECT_TRUE(QueryVar(piece.side_a()->out_try_set_route));
  EXPECT_FALSE(QueryVar(piece.side_b()->in_route_set_success));
  EXPECT_FALSE(QueryVar(piece.side_b()->in_route_set_failure));

  Run();

  SetVar(before.side_b()->in_route_set_success, true);
  SetVar(before.side_b()->binding()->out_try_set_route, false);
  Run();
  Run();

  EXPECT_TRUE(SQueryVar(piece.side_b()->in_route_set_success));
  EXPECT_FALSE(QueryVar(piece.side_b()->in_route_set_failure));
  EXPECT_TRUE(QueryVar(piece.route_set_ba_));
  EXPECT_FALSE(QueryVar(piece.route_set_ab_));
}

TEST_F(AutomataNodeTests, SimulatedOccupancy_SimultSetting) {
  Board brd;
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 1 * 32);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(
      &brd, BRACZ_LAYOUT | 0x5040, 2 * 32, &previous_detector);
  StraightTrackWithDetector after(
      &brd, BRACZ_LAYOUT | 0x5080, 3 * 32, &next_detector);

  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(after.side_a());

  DefAut(strategyaut, brd, {
    piece.RunAll(this);
    /*piece.SimulateAllOccupancy(this);
      piece.SimulateAllRoutes(this);*/
  });

  SetupRunner(&brd);

  Run();
  Run();

  // An incoming route request here.
  SetVar(after.side_a()->out_try_set_route, true);
  Run();

  // Should be propagated.
  EXPECT_TRUE(SQueryVar(piece.side_a()->out_try_set_route));
  EXPECT_FALSE(QueryVar(piece.side_b()->in_route_set_success));
  EXPECT_FALSE(QueryVar(piece.side_b()->in_route_set_failure));
  EXPECT_TRUE(QueryVar(piece.tmp_route_setting_in_progress_));
  EXPECT_FALSE(QueryVar(after.side_a()->out_try_set_route));
  Run();

  // A conflicting route request,
  SetVar(before.side_b()->out_try_set_route, true);
  Run();
  // which is rejected immediately.
  EXPECT_FALSE(QueryVar(before.side_b()->out_try_set_route));
  EXPECT_FALSE(SQueryVar(before.side_b()->binding()->in_route_set_success));
  EXPECT_TRUE(QueryVar(before.side_b()->binding()->in_route_set_failure));

  // Then the first route set request returns.
  SetVar(before.side_b()->in_route_set_success, true);
  SetVar(before.side_b()->binding()->out_try_set_route, false);
  Run();

  // And then the first route sets properly.
  EXPECT_TRUE(QueryVar(after.side_a()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(after.side_a()->binding()->in_route_set_failure));
  EXPECT_FALSE(QueryVar(after.side_a()->out_try_set_route));
  EXPECT_TRUE(QueryVar(piece.route_set_ba_));
  EXPECT_FALSE(QueryVar(piece.route_set_ab_));
}

TEST_F(AutomataNodeTests, MultiRoute) {
  Board brd;
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 1 * 32);
  static StraightTrackShort piece2(&brd, BRACZ_LAYOUT | 0x5040, 4 * 32);
  static StraightTrackLong piece3(&brd, BRACZ_LAYOUT | 0x5080, 5 * 32);
  static StraightTrackShort piece4(&brd, BRACZ_LAYOUT | 0x50C0, 6 * 32);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  static StraightTrackWithDetector before(
      &brd, BRACZ_LAYOUT | 0x5100, 2 * 32, &previous_detector);
  static StraightTrackWithDetector after(
      &brd, BRACZ_LAYOUT | 0x5140, 3 * 32, &next_detector);
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
        ImportVariable(&after.side_a()->binding()->out_try_set_route);
    LocalVariable* route_set_succcess =
        ImportVariable(&after.side_a()->in_route_set_success);
    Def().IfReg1(*try_set_route).ActReg0(try_set_route).ActReg1(
        route_set_succcess);
  });

  SetupRunner(&brd);

  Run(10);
  SetVar(before.side_b()->out_try_set_route, true);
  Run(10);
  EXPECT_FALSE(QueryVar(before.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(before.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(before.side_b()->binding()->in_route_set_failure));

  SetVar(before.side_b()->binding()->in_route_set_success, false);
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

  EXPECT_FALSE(SQueryVar(piece.simulated_occupancy_));
  EXPECT_FALSE(SQueryVar(piece.route_set_ab_));

  // Route should be done, we set another one.
  Run(10);
  SetVar(before.side_b()->out_try_set_route, true);
  Run(10);
  EXPECT_FALSE(QueryVar(before.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(before.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(before.side_b()->binding()->in_route_set_failure));
}

TEST_F(AutomataNodeTests, Signal) {
  Board brd;
  FakeBit previous_detector(this);
  FakeBit request_green(this);
  FakeBit signal_green(this);
  FakeBit next_detector(this);
  FakeBit request_green2(this);
  FakeBit signal_green2(this);
  static StraightTrackLong first_body(&brd, BRACZ_LAYOUT | 0x5180, 1 * 32);
  static StraightTrackWithDetector first_det(
      &brd, BRACZ_LAYOUT | 0x5100, 2 * 32, &previous_detector);
  static SignalPiece signal(
      &brd, BRACZ_LAYOUT | 0x5000, 3 * 32, &request_green, &signal_green);
  static StraightTrackLong mid(&brd, BRACZ_LAYOUT | 0x5040, 4 * 32);
  static StraightTrackLong second_body(&brd, BRACZ_LAYOUT | 0x51C0, 5 * 32);
  static StraightTrackWithDetector second_det(
      &brd, BRACZ_LAYOUT | 0x5080, 6 * 32, &next_detector);
  static SignalPiece second_signal(
      &brd, BRACZ_LAYOUT | 0x50C0, 7 * 32, &request_green2, &signal_green2);
  static StraightTrackLong after(&brd, BRACZ_LAYOUT | 0x5140, 8 * 32);

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
        ImportVariable(&after.side_a()->binding()->out_try_set_route);
    LocalVariable* route_set_succcess =
        ImportVariable(&after.side_a()->in_route_set_success);
    Def().IfReg1(*try_set_route).ActReg0(try_set_route).ActReg1(
        route_set_succcess);
  });

  SetupRunner(&brd);
  Run(2);

  // Sets the first train's route until the first signal.
  SetVar(first_body.side_b()->out_try_set_route, true);
  Run(5);
  EXPECT_FALSE(QueryVar(first_body.side_b()->out_try_set_route));
  EXPECT_TRUE(QueryVar(first_body.side_b()->binding()->in_route_set_success));
  EXPECT_FALSE(QueryVar(first_body.side_b()->binding()->in_route_set_failure));

  EXPECT_TRUE(QueryVar(signal.route_set_ab_));
  EXPECT_FALSE(signal_green.Get());

  // First train shows up at first signal.
  previous_detector.Set(true);
  Run(5);
  EXPECT_TRUE(QueryVar(signal.route_set_ab_));
  EXPECT_FALSE(signal_green.Get());
  EXPECT_TRUE(QueryVar(first_det.route_set_ab_));

  EXPECT_FALSE(QueryVar(mid.route_set_ab_));

  // and gets a green
  request_green.Set(true);
  Run(1);
  EXPECT_TRUE(request_green.Get());
  EXPECT_TRUE(QueryVar(signal.side_b()->out_try_set_route));
  Run(10);

  EXPECT_FALSE(request_green.Get());
  EXPECT_TRUE(QueryVar(mid.route_set_ab_));
  EXPECT_TRUE(signal_green.Get());

  // and leaves the block
  previous_detector.Set(false);
  Run(10);
  EXPECT_TRUE(QueryVar(mid.route_set_ab_));
  EXPECT_FALSE(QueryVar(signal.route_set_ab_));
  EXPECT_FALSE(QueryVar(first_det.route_set_ab_));
  EXPECT_FALSE(signal_green.Get());

  EXPECT_FALSE(QueryVar(first_body.side_b()->out_try_set_route));

  // We run a second route until the first signal
  SetVar(first_body.side_b()->out_try_set_route, true);
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

  EXPECT_TRUE(QueryVar(signal.route_set_ab_));
  EXPECT_FALSE(signal_green.Get());
  EXPECT_TRUE(QueryVar(first_det.route_set_ab_));

  // We try to set another green, but the previous train blocks this.
  request_green.Set(true);
  Run(1);
  EXPECT_TRUE(request_green.Get());
  EXPECT_TRUE(QueryVar(signal.side_b()->out_try_set_route));
  Run(10);

  EXPECT_FALSE(request_green.Get());
  EXPECT_FALSE(QueryVar(signal.side_b()->out_try_set_route));
  EXPECT_FALSE(signal_green.Get());

  // First train reaches second signal.
  next_detector.Set(true);
  request_green.Set(true);
  Run(10);
  EXPECT_FALSE(QueryVar(mid.route_set_ab_));
  EXPECT_TRUE(QueryVar(second_signal.route_set_ab_));
  EXPECT_FALSE(signal_green2.Get());

  EXPECT_FALSE(request_green.Get());
  EXPECT_FALSE(signal_green.Get());

  request_green2.Set(true);
  Run(5);
  EXPECT_TRUE(signal_green2.Get());
  EXPECT_TRUE(QueryVar(second_signal.route_set_ab_));

  next_detector.Set(false);
  Run(10);
  EXPECT_FALSE(signal_green2.Get());
  EXPECT_FALSE(QueryVar(second_signal.route_set_ab_));

  // Now the second train can go.
  request_green.Set(true);
  Run(10);
  EXPECT_FALSE(request_green.Get());
  EXPECT_TRUE(signal_green.Get());
  EXPECT_TRUE(QueryVar(mid.route_set_ab_));
}

TEST_F(AutomataNodeTests, 100trainz) {
  Board brd;
  FakeBit previous_detector(this);
  FakeBit request_green(this);
  FakeBit signal_green(this);
  FakeBit next_detector(this);
  FakeBit request_green2(this);
  FakeBit signal_green2(this);
  static StraightTrackLong first_body(&brd, BRACZ_LAYOUT | 0x5180, 1 * 32);
  static StraightTrackWithDetector first_det(
      &brd, BRACZ_LAYOUT | 0x5100, 2 * 32, &previous_detector);
  static SignalPiece signal(
      &brd, BRACZ_LAYOUT | 0x5000, 3 * 32, &request_green, &signal_green);
  static StraightTrackLong mid(&brd, BRACZ_LAYOUT | 0x5040, 4 * 32);
  static StraightTrackLong second_body(&brd, BRACZ_LAYOUT | 0x51C0, 5 * 32);
  static StraightTrackWithDetector second_det(
      &brd, BRACZ_LAYOUT | 0x5080, 6 * 32, &next_detector);
  static SignalPiece second_signal(
      &brd, BRACZ_LAYOUT | 0x50C0, 7 * 32, &request_green2, &signal_green2);
  static StraightTrackLong after(&brd, BRACZ_LAYOUT | 0x5140, 8 * 32);

  BindSequence({&first_body,    &first_det,   &signal,
                               &mid,           &second_body, &second_det,
                               &second_signal, &after});

#define DefPiece(pp) \
  StandardPluginAutomata aut##pp(&brd, &pp);

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
        ImportVariable(&after.side_a()->binding()->out_try_set_route);
    LocalVariable* route_set_succcess =
        ImportVariable(&after.side_a()->in_route_set_success);
    Def().IfReg1(*try_set_route).ActReg0(try_set_route).ActReg1(
        route_set_succcess);
  });

  SetupRunner(&brd);
  Run(2);

  // The initial state is a train at both blocks.
  previous_detector.Set(true);
  next_detector.Set(true);

  for (int i = 0; i < 100; ++i) {
    Run(10);
    EXPECT_FALSE(signal_green.Get());
    EXPECT_FALSE(signal_green2.Get());

    // The rear train cannot move forward.
    request_green.Set(true);
    Run(10);
    EXPECT_FALSE(request_green.Get());
    EXPECT_FALSE(signal_green.Get());
    EXPECT_FALSE(QueryVar(mid.route_set_ab_));

    // Makes the second train leave.
    request_green2.Set(true);
    Run(10);
    EXPECT_FALSE(request_green2.Get());
    EXPECT_TRUE(signal_green2.Get());

    // The rear train still cannot move forward.
    request_green.Set(true);
    Run(10);
    EXPECT_FALSE(request_green.Get());
    EXPECT_FALSE(signal_green.Get());
    EXPECT_FALSE(QueryVar(mid.route_set_ab_));

    // Makes the front train leave.
    next_detector.Set(false);
    Run(10);
    EXPECT_FALSE(request_green2.Get());
    EXPECT_FALSE(signal_green2.Get());
    EXPECT_FALSE(QueryVar(second_signal.route_set_ab_));

    // Now we can run the rear train.
    request_green.Set(true);
    Run(10);
    EXPECT_FALSE(request_green.Get());
    EXPECT_TRUE(signal_green.Get());
    EXPECT_TRUE(QueryVar(mid.route_set_ab_));
    EXPECT_TRUE(QueryVar(second_signal.route_set_ab_));

    // But the third train cannot get in yet,
    SetVar(first_body.side_b()->out_try_set_route, true);
    Run(5);
    EXPECT_FALSE(QueryVar(first_body.side_b()->out_try_set_route));
    EXPECT_TRUE(QueryVar(first_body.side_b()->binding()->in_route_set_failure));
    if (i != 0) {
      EXPECT_TRUE(
          QueryVar(signal.route_set_ab_));  // because the route is still there.
    }

    // Rear train leaves its block.
    previous_detector.Set(false);
    Run(10);
    EXPECT_FALSE(QueryVar(signal.route_set_ab_));  // route gone.

    // The third train can now come.
    SetVar(first_body.side_b()->out_try_set_route, true);
    Run(5);
    EXPECT_FALSE(QueryVar(first_body.side_b()->out_try_set_route));
    EXPECT_FALSE(
        QueryVar(first_body.side_b()->binding()->in_route_set_failure));
    EXPECT_TRUE(QueryVar(first_body.side_b()->binding()->in_route_set_success));
    EXPECT_TRUE(QueryVar(signal.route_set_ab_));

    // Second train reaches rear block.
    next_detector.Set(true);
    Run(10);
    EXPECT_FALSE(QueryVar(mid.route_set_ab_));
    EXPECT_TRUE(QueryVar(second_signal.route_set_ab_));

    // Third train reaches second block.
    previous_detector.Set(true);
    Run(10);
  }
}

}  // namespace automata

int appl_main(int argc, char* argv[]) {
  //__libc_init_array();
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
