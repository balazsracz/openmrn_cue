#include <unistd.h>

#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"


#include "automata_tests_helper.hxx"

#include "../automata/control-logic.hxx"

namespace automata {

TEST_F(AutomataNodeTests, TestFramework) {
  Board brd;
  static EventBasedVariable ev_var(&brd, BRACZ_LAYOUT | 0xf000,
                                   BRACZ_LAYOUT | 0xf001,
                                   0);
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
  static EventBasedVariable ev_var(&brd, BRACZ_LAYOUT | 0xf000,
                                   BRACZ_LAYOUT | 0xf001,
                                   0);
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
  StraightTrackWithDetector before(&brd, BRACZ_LAYOUT | 0x5100, 64,
                                   &previous_detector);
  StraightTrackWithDetector after(&brd, BRACZ_LAYOUT | 0x5100, 96,
                                  &next_detector);
  piece.side_a()->Bind(before.side_b());
  piece.side_b()->Bind(after.side_a());

  EventListener sim_occ(piece.simulated_occupancy_);
  EventListener seen_train(piece.tmp_seen_train_in_next_);
  EventListener released(piece.side_b()->out_route_released);

  DefAut(strategyaut, brd, {
      piece.SimulateAllOccupancy(this);
    });

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
  static StraightTrackShort piece2(&brd, BRACZ_LAYOUT | 0x5010, 128);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(&brd, BRACZ_LAYOUT | 0x5100, 64,
                                   &previous_detector);
  StraightTrackWithDetector after(&brd, BRACZ_LAYOUT | 0x5100, 96,
                                  &next_detector);
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
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 1*32);
  static StraightTrackShort piece2(&brd, BRACZ_LAYOUT | 0x5040, 4*32);
  static StraightTrackLong piece3(&brd, BRACZ_LAYOUT | 0x5050, 5*32);
  static StraightTrackShort piece4(&brd, BRACZ_LAYOUT | 0x5060, 6*32);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector before(&brd, BRACZ_LAYOUT | 0x5020, 2*32,
                                   &previous_detector);
  StraightTrackWithDetector after(&brd, BRACZ_LAYOUT | 0x5030, 3*32,
                                  &next_detector);
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
  EXPECT_TRUE(sim_occ2.value());
  EXPECT_TRUE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());

  previous_detector.Set(false);
  Run();
  EXPECT_FALSE(sim_occ.value());
  EXPECT_FALSE(sim_occ2.value());
  EXPECT_FALSE(sim_occ3.value());
  EXPECT_FALSE(sim_occ4.value());
}

}  // namespace automata

int appl_main(int argc, char* argv[]) {
  //__libc_init_array();
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
