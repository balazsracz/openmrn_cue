#include <string>
#include "gtest/gtest.h"

#include "src/automata_runner.h"

#include "../automata/system.hxx"
#include "../automata/operations.hxx"
#include "../automata/variables.hxx"

#include "nmranet_config.h"

#include "pipe/pipe.hxx"
#include "nmranet_can.h"

DEFINE_PIPE(can_pipe0, sizeof(struct can_frame));

const char *nmranet_manufacturer = "Stuart W. Baker";
const char *nmranet_hardware_rev = "N/A";
const char *nmranet_software_rev = "0.1";

const size_t main_stack_size = 2560;
const size_t ALIAS_POOL_SIZE = 2;
const size_t DOWNSTREAM_ALIAS_CACHE_SIZE = 2;
const size_t UPSTREAM_ALIAS_CACHE_SIZE = 2;
const size_t DATAGRAM_POOL_SIZE = 10;
const size_t CAN_RX_BUFFER_SIZE = 1;
const size_t CAN_TX_BUFFER_SIZE = 32;
const size_t SERIAL_RX_BUFFER_SIZE = 16;
const size_t SERIAL_TX_BUFFER_SIZE = 16;
const size_t DATAGRAM_THREAD_STACK_SIZE = 512;
const size_t CAN_IF_READ_THREAD_STACK_SIZE = 1024;



TEST(TimerBitTest, Get) {
  Automata a(1, 0);
  ReadWriteBit* bit = a.GetTimerBit();
  EXPECT_TRUE(bit);
}

TEST(TimerBitTest, ShowsZeroNonZeroState) {
  Automata a(1, 0);
  ReadWriteBit* bit = a.GetTimerBit();
  ASSERT_TRUE(bit);
  a.SetTimer(13);
  EXPECT_EQ(13, a.GetTimer());
  EXPECT_TRUE(bit->Read(NULL, &a));
  a.SetTimer(0);
  EXPECT_EQ(0, a.GetTimer());
  EXPECT_FALSE(bit->Read(NULL, &a));
  a.SetTimer(1);
  EXPECT_TRUE(bit->Read(NULL, &a));
}

TEST(TimerBitTest, Tick) {
  Automata a(1, 0);
  ReadWriteBit* bit = a.GetTimerBit();
  ASSERT_TRUE(bit);
  a.SetTimer(3);
  EXPECT_TRUE(bit->Read(NULL, &a));
  a.Tick();
  EXPECT_EQ(2, a.GetTimer());
  EXPECT_TRUE(bit->Read(NULL, &a));
  a.Tick();
  EXPECT_EQ(1, a.GetTimer());
  EXPECT_TRUE(bit->Read(NULL, &a));
  a.Tick();
  EXPECT_EQ(0, a.GetTimer());
  EXPECT_FALSE(bit->Read(NULL, &a));
  a.Tick();
  EXPECT_EQ(0, a.GetTimer());
  EXPECT_FALSE(bit->Read(NULL, &a));
}

TEST(TimerBitTest, DifferentInitialize) {
  Automata a(10, 0);
  Automata b(30, 0);
  EXPECT_EQ(31, (0xff & (~_ACT_TIMER_MASK)));
  a.SetTimer(31);
  b.SetTimer(31);
  EXPECT_NE(a.GetTimer(), b.GetTimer());
}

TEST(TimerBitTest, DiesOnWrite) {
  Automata a(10, 0);
  ReadWriteBit* bit = a.GetTimerBit();
  EXPECT_DEATH({
      bit->Write(NULL, &a, false);
    }, "802AAC2A");
}

TEST(RunnerTest, TrivialRunnerCreateDestroy) {
  automata::Board brd;
  string output;
  brd.Render(&output);
  {
    AutomataRunner (NULL, output.data());
  }
}


TEST(RunnerTest, TrivialRunnerRun) {
  automata::Board brd;
  string output;
  brd.Render(&output);
  {
    AutomataRunner r(NULL, output.data());
    r.RunAllAutomata();
  }
}


TEST(RunnerTest, SingleEmptyAutomataBoard) {
  static automata::Board brd;
  DefAut(testaut1, brd, {});
  string output;
  brd.Render(&output);
  {
    AutomataRunner r(NULL, output.data());
    r.RunAllAutomata();
    r.RunAllAutomata();
    r.RunAllAutomata();
  }
}




int appl_main(int argc, char* argv[]) {
  //__libc_init_array();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
