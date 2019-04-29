#include <unistd.h>

#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "automata_tests_helper.hxx"
#include "../automata/control-logic.hxx"

using ::testing::HasSubstr;

extern int debug_variables;

namespace automata {

class LogicTest : public AutomataNodeTests {
 protected:
  LogicTest()
      : block_(&brd, BRACZ_LAYOUT|0xC000, "blk"),
        magnet_aut_(&brd, alloc()) {
    debug_variables = 0;
    // We ignore all event messages on the CAN bus. THese are checked with more
    // high-level constructs.
    EXPECT_CALL(canBus_, mwrite(HasSubstr(":X195B422AN"))).Times(AtLeast(0));
    EXPECT_CALL(canBus_, mwrite(HasSubstr(":X1991422AN"))).Times(AtLeast(0));
    EXPECT_CALL(canBus_, mwrite(HasSubstr(":X1954522AN"))).Times(AtLeast(0));
  }

  const AllocatorPtr& alloc() { return block_.allocator(); }

  friend struct TestBlock;

  class BlockInterface {
   public:
    virtual ~BlockInterface() {}
    virtual StandardBlock* b() = 0;
    virtual FakeROBit* inverted_detector() = 0;
    virtual FakeBit* signal_green() = 0;
  };

  struct TestBlock : public BlockInterface {
    TestBlock(LogicTest* test, const string& name)
        : inverted_detector_(test),
          signal_green_(test),
          physical_signal_(&inverted_detector_, &signal_green_, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
          b_(&test->brd, &physical_signal_, test->alloc(), name) {
      inverted_detector_.Set(true);
      signal_green_.Set(false);
    }
    virtual StandardBlock* b() { return &b_; }
    virtual FakeROBit* inverted_detector() { return &inverted_detector_; }
    virtual FakeBit* signal_green() { return &signal_green_; }

    FakeROBit inverted_detector_;
    FakeBit signal_green_;
    PhysicalSignal physical_signal_;
    StandardBlock b_;
  };

  struct TestStubBlock : public BlockInterface {
    TestStubBlock(LogicTest* test, const string& name)
        : inverted_detector_(test),
          inverted_entry_detector_(test),
          signal_green_(test),
          physical_signal_(&inverted_detector_, &signal_green_, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
          b_(&test->brd, &physical_signal_, &inverted_entry_detector_, test->alloc(), name) {
      inverted_entry_detector_.Set(true);
      inverted_detector_.Set(true);
      signal_green_.Set(false);
    }
    virtual StandardBlock* b() { return &b_.b_; }
    virtual FakeROBit* inverted_detector() { return &inverted_detector_; }
    virtual FakeBit* signal_green() { return &signal_green_; }

    FakeROBit* inverted_entry_detector() { return &inverted_entry_detector_; }

    FakeROBit inverted_detector_, inverted_entry_detector_;
    FakeBit signal_green_;
    PhysicalSignal physical_signal_;
    StubBlock b_;
  };

  struct TestDetectorBlock {
    TestDetectorBlock(LogicTest* test, const string& name)
        : detector(test),
          b(test->alloc()->Allocate(name, 32), &detector) {}
    // TODO: do we need to add an automata for this?
    FakeBit detector;
    StraightTrackWithDetector b;
  };

  struct TestShortTrack {
    TestShortTrack(LogicTest* test, const string& name)
        : b(test->alloc()->Allocate(name, 24)),
          aut_body(name, &test->brd, &b) {}
    StraightTrackShort b;
    StandardPluginAutomata aut_body;
  };

  struct TestFixedTurnout {
    TestFixedTurnout(LogicTest* test, FixedTurnout::State state, const string& name)
        : b(state, test->alloc()->Allocate(name, 48, 32)),
          aut_body(name, &test->brd, &b) {}
    FixedTurnout b;
    StandardPluginAutomata aut_body;
  };

  struct TestMovableTurnout {
    TestMovableTurnout(LogicTest* test, const string& name)
        : set_0(test),
          set_1(test),
          magnet(&test->magnet_aut_, name + ".mgn", &set_0, &set_1, false),
          b(test->alloc()->Allocate(name, 48, 32), &magnet),
          aut_body(name, &test->brd, &b) {}
    FakeBit set_0;
    FakeBit set_1;
    MagnetDef magnet;
    MovableTurnout b;
    StandardPluginAutomata aut_body;
  };

  struct TestMovableDKW {
    TestMovableDKW(LogicTest* test, const string& name)
        : set_0(test),
          set_1(test),
          magnet(&test->magnet_aut_, name + ".mgn", &set_0, &set_1, false),
          b(test->alloc()->Allocate(name, 48, 32), &magnet),
          aut_body(name, &test->brd, &b) {}
    FakeBit set_0;
    FakeBit set_1;
    MagnetDef magnet;
    MovableDKW b;
    StandardPluginAutomata aut_body;
  };

  struct DKWTestLayout;

  Board brd;
  EventBlock block_;
  MagnetCommandAutomata magnet_aut_;
};

// The DKW test layout is an 8-shaped layout, with signals leading into each
// end of the DKW, and being put back-to-back in pairs. The A1 and A2 are
// connected through one side of the 8, and B1 and B2 are connected through the
// other.
struct LogicTest::DKWTestLayout {
  TestBlock in_a1;
  TestBlock in_a2;
  TestBlock in_b1;
  TestBlock in_b2;
  StandardPluginAutomata dkw_body;
  DKWTestLayout(LogicTest* test, DKW* dkw) 
    : in_a1(test, "in_a1"),
      in_a2(test, "in_a2"),
      in_b1(test, "in_b1"),
      in_b2(test, "in_b2"),
      dkw_body("dkw", &test->brd, dkw)
  {
    BindPairs({{in_a1.b()->side_b(), dkw->point_a1()},
               {in_a2.b()->side_b(), dkw->point_a2()},
               {in_b1.b()->side_b(), dkw->point_b1()},
               {in_b2.b()->side_b(), dkw->point_b2()},
               {in_a1.b()->side_a(), in_a2.b()->side_a()},
               {in_b1.b()->side_a(), in_b2.b()->side_a()}});
  }

  DKW::Point BlockToPoint(const TestBlock* b) {
    if (b == &in_a1) return DKW::POINT_A1;
    if (b == &in_a2) return DKW::POINT_A2;
    if (b == &in_b1) return DKW::POINT_B1;
    if (b == &in_b2) return DKW::POINT_B2;
    HASSERT(0 && "unknown block");
  }
};

class LogicTrainTest : public LogicTest, protected TrainTestHelper {
 protected:
  LogicTrainTest() : TrainTestHelper(this) {}
};

void IfNotPaused(Automata::Op* op) {
}
auto g_not_paused_condition = NewCallback(&IfNotPaused);

// This is the test layout. It's a dogbone (a loop) with 6 standard blocks.
// There is a stub track in the turnaround loop. It has a moveableturnout as
// entry, then a fixedturnout to exit.
//
// There is a crossover of fixed turnouts to get back from TopB to BotB when
// going wrong-way.
//
//   >Rleft
//  /--\  TopA>TopB  /---------\   x
//  |   \------/----/--/--x    |   x
//  |   /-----/-----\-/  RStub |   x
//  \--/  BotB<BotA  \---------/   x
//                   <RRight
//
//

class SampleLayoutLogicTrainTest : public LogicTrainTest {
 public:
  SampleLayoutLogicTrainTest()

  {
    BindSequence({&WRStubIntoMain, BotA.b(), &WXoverBot, BotB.b(), RLeft.b(),
                  TopA.b(), &WXoverTop, TopB.b(), &WRStubEntry, RRight.b(),
                  &WRStubIntoMain});
    BindPairs({{RStubEntry.b.side_closed(), RStubExit.b.side_closed()},
               {RStubExit.b.side_thrown(), RStubIntoMain.b.side_closed()},
               {RStubExit.b.side_points(), RStub.b_.entry()},
               {XoverTop.b.side_thrown(), XoverBot.b.side_thrown()}});
  }

  TestBlock RLeft{this, "RLeft"}, TopA{this, "TopA"}, TopB{this, "TopB"},
      RRight{this, "RRight"}, BotA{this, "BotA"}, BotB{this, "BotB"};
  TestStubBlock RStub{this, "RStub"};
  TestMovableTurnout RStubEntry{this, "RStubEntry"};
  TurnoutWrap WRStubEntry{&RStubEntry.b, kPointToThrown};
  TestFixedTurnout RStubExit{this, FixedTurnout::TURNOUT_THROWN, "RStubExit"};
  TestFixedTurnout RStubIntoMain{this, FixedTurnout::TURNOUT_THROWN,
                                 "RStubIntoMain"};
  TurnoutWrap WRStubIntoMain{&RStubIntoMain.b, kThrownToPoint};
  TestFixedTurnout XoverTop{this, FixedTurnout::TURNOUT_THROWN, "XoverTop"};
  TurnoutWrap WXoverTop{&XoverTop.b, kClosedToPoint};
  TestFixedTurnout XoverBot{this, FixedTurnout::TURNOUT_THROWN, "XoverBot"};
  TurnoutWrap WXoverBot{&XoverBot.b, kClosedToPoint};
};

TEST_F(SampleLayoutLogicTrainTest, Construct) {}

TEST_F(SampleLayoutLogicTrainTest, SetupRunner) {
  SetupRunner(&brd);
}

}  // namespace automata
