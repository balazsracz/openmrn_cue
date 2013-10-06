#include <unistd.h>

#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"


#include "automata_tests_helper.hxx"

#include "../automata/control-logic.hxx"

namespace automata {

TEST_F(AutomataNodeTests, SimulatedOccupancy) {
  Board brd;
  static StraightTrackShort piece(&brd, BRACZ_LAYOUT | 0x5000, 32);
  EventBasedVariable detector(&brd,
                              BRACZ_LAYOUT | 0xf000, BRACZ_LAYOUT | 0xf001,
                              0);
  EventBasedVariable detect2(&brd,
                             BRACZ_LAYOUT | 0xf002, BRACZ_LAYOUT | 0xf003,
                             1);
  StraightTrackWithDetector front(&brd, BRACZ_LAYOUT | 0x5100, 64, &detector);
  StraightTrackWithDetector back(&brd, BRACZ_LAYOUT | 0x5100, 96, &detect2);
  piece.side_b()->Bind(front.side_a());
  piece.side_a()->Bind(back.side_b());
  // We do not bind "other" becase it is irrelevant.

  DefAut(strategyaut, brd, {
      piece.SimulateAllOccupancy(this);
    });

  SetupRunner(&brd);
  runner_->RunAllAutomata();
}

}  // namespace automata



int appl_main(int argc, char* argv[]) {
  //__libc_init_array();
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
