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

TEST_F(AutomataNodeTests, SimulatedOccupancy) {
  Board brd;
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 32);
  FakeBit previous_detector(this);
  FakeBit next_detector(this);
  StraightTrackWithDetector front(&brd, BRACZ_LAYOUT | 0x5100, 64,
                                  &previous_detector);
  StraightTrackWithDetector back(&brd, BRACZ_LAYOUT | 0x5100, 96,
                                 &next_detector);
  piece.side_b()->Bind(front.side_a());
  piece.side_a()->Bind(back.side_b());
  // We do not bind "other" becase it is irrelevant.

  EventListener sim_occ(piece.simulated_occupancy_);

  DefAut(strategyaut, brd, {
      piece.SimulateAllOccupancy(this);
    });

  SetupRunner(&brd);
  Run();
  previous_detector.Set(true);
  Run();
  EXPECT_FALSE(sim_occ.value());
  SetVar(piece.route_set_ab_, true);
  Run();
}

}  // namespace automata



int appl_main(int argc, char* argv[]) {
  //__libc_init_array();
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
