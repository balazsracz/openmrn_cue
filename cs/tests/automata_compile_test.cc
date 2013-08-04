#include <string>


#include "gtest/gtest.h"

#include "os/os.h"

#include "../automata/system.hxx"
#include "../automata/operations.hxx"
#include "../automata/variables.hxx"

using namespace automata;

#define OFS_GLOBAL_BITS 24


Board empty;

TEST(BoardCompile, EmptyBoard) {
  string output;
  empty.Render(&output);
  // The empty board will have a double-zero for terminating the automata list,
  // then a zero for terminating the global variable declarations. Then no
  // actual automatas.
  EXPECT_EQ(3UL, output.size());
  string expected(3, '\0');
  EXPECT_EQ(expected, output);
}


class EmptyAutomata : public Automata {
 protected:
  virtual void Body() {}
  virtual Board* board() { return NULL; }
};

TEST(AutomataCompile, EmptyAut) {
  EmptyAutomata eaut;
  string output;

  eaut.Render(&output);

  // The empty automata will have one zero byte for termination.
  EXPECT_EQ(1UL, output.size());
  string expected(1, '\0');
  EXPECT_EQ(expected, output);
}


TEST(OpCompile, RenderOp) {
  vector<uint8_t> ifs({8, 1, 7, 32});
  vector<uint8_t> acts({12, 7, 42});
  string output;

  Automata::Op::CreateOperation(&output, ifs, acts);
  EXPECT_EQ(8U, output.size());

  EXPECT_EQ(0x34, output[0]);
  EXPECT_EQ(string({0x34, 8, 1, 7, 32, 12, 7, 42}), output);
}

TEST(OpCompile, AddIfAct) {
  EmptyAutomata eaut;
  // We put some garbage before the output string to ensure it is kept safe.
  string output({11,12,13});
  {
    Automata::Op op(&eaut, &output);
    op.AddIf(3);
    op.AddAct(42);
    op.AddAct(43);
    op.AddIf(7);
    op.AddIf(8);
    // Should not be rendered yet.
    EXPECT_EQ(string({11, 12, 13}), output);
  }
  EXPECT_EQ(string({11, 12, 13, 0x23, 3, 7, 8, 42, 43}), output);
}

TEST(OpCompileDeathTest, TooLongOp) {
  vector<uint8_t> ifs(16, 3);
  vector<uint8_t> acts(2, 1);
  EXPECT_DEATH({
      string output;
      Automata::Op::CreateOperation(&output, ifs, acts);
    }, "if.*< 16");
  EXPECT_DEATH({
      string output;
      Automata::Op::CreateOperation(&output, acts, ifs);
    }, "act.*< 16");
}

TEST(StateRefDeathTest, TooBigState) {
  ASSERT_EQ((~31)&0xff, _IF_STATE_MASK);
  StateRef ref(31);
  EXPECT_EQ(31, ref.state);
  EXPECT_DEATH({
      StateRef nref(32);
    }, "== id");
}

TEST(OpCompile, IfActState) {
  EmptyAutomata eaut;
  string output;
  StateRef ref1(17);
  StateRef ref2(23);
  {
    Automata::Op op(&eaut, &output);
    op.IfState(ref1);
    op.ActState(ref2);
  }
  string expected({0x11, _IF_STATE | 17, _ACT_STATE | 23});
  EXPECT_EQ(expected, output);
}

Board simple;
StateRef simplestate1(11);
StateRef simplestate2(22);
DefAut(simpletestaut1, simple, {
    Def().IfState(simplestate1).ActState(simplestate2);
    Def().IfState(simplestate2).ActState(simplestate1);
  });
DefAut(simpletestaut2, simple, {
    Def().IfState(simplestate2).ActState(simplestate1);
  });

TEST(BoardCompile, SimpleBoard) {
  string output;
  simple.Render(&output);
  string expected =
      {
        7, 0,  // pointer 1
        14, 0, // pointer 2
        0, 0,  // end of automatas
        0,     // end of preamble
        0x11, _IF_STATE | 11, _ACT_STATE | 22,
        0x11, _IF_STATE | 22, _ACT_STATE | 11,
        0,     // end of autoamta 1
        0x11, _IF_STATE | 22, _ACT_STATE | 11,
        0      // end of automata 2
      };
  EXPECT_EQ(expected, output);
}




Board testevent;

EventBasedVariable intev(&testevent,
                         0x0502010202650022ULL,
                         0x0502010202650023ULL,
                         0, OFS_GLOBAL_BITS, 3);

DefAut(testaut, testevent, {
    auto& lintev = ImportVariable(&intev);
    Def().IfReg0(lintev).ActReg1(lintev);
    });

TEST(BoardCompile, SingleEventBoard) {
  string output;
  testevent.Render(&output);
  string expected =
      {
        31, 0,  // pointer to aut
        0, 0,  // end of automatas
        0xA0, _ACT_SET_EVENTID, 0b01010111, 5, 2, 1, 2, 2, 0x65, 0, 0x22,
        0xA0, _ACT_SET_EVENTID, 0b00000111, 5, 2, 1, 2, 2, 0x65, 0, 0x23,
        0x30, _ACT_DEF_VAR, 0b0000000, (24<<3) | 3,
        0,     // end of preamble
        0x40, _ACT_IMPORT_VAR, 1, 28, 0,
        0x11, _IF_REG_0 | 1, _ACT_REG_1 | 1,
        0,     // end of autoamta 1
      };
  EXPECT_EQ(expected, output);
}

int appl_main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
