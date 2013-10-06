#include <unistd.h>

#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"


#include "automata_tests_helper.hxx"

#include "../automata/control-logic.hxx"

namespace automata {

TEST_F(AutomataTests, SimulatedOccupancy) {
  Board brd;
  static StraightTrack piece(&brd, BRACZ_LAYOUT | 0x5000, 0);
  StraightTrack other(&brd, BRACZ_LAYOUT | 0x5100, 32);
  piece.BindA(other.side_b());
  piece.BindB(other.side_a());
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
