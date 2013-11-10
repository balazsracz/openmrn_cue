#include <unistd.h>

#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "utils/macros.h"

#include "../automata/system.hxx"
#include "../automata/operations.hxx"
#include "../automata/variables.hxx"

#include "src/automata_runner.h"
#include "nmranet_config.h"



using automata::Board;
using automata::StateRef;

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
  EXPECT_TRUE(bit->Read(0, NULL, &a));
  a.SetTimer(0);
  EXPECT_EQ(0, a.GetTimer());
  EXPECT_FALSE(bit->Read(0, NULL, &a));
  a.SetTimer(1);
  EXPECT_TRUE(bit->Read(0, NULL, &a));
}

TEST(TimerBitTest, Tick) {
  Automata a(1, 0);
  ReadWriteBit* bit = a.GetTimerBit();
  ASSERT_TRUE(bit);
  a.SetTimer(3);
  EXPECT_TRUE(bit->Read(0, NULL, &a));
  a.Tick();
  EXPECT_EQ(2, a.GetTimer());
  EXPECT_TRUE(bit->Read(0, NULL, &a));
  a.Tick();
  EXPECT_EQ(1, a.GetTimer());
  EXPECT_TRUE(bit->Read(0, NULL, &a));
  a.Tick();
  EXPECT_EQ(0, a.GetTimer());
  EXPECT_FALSE(bit->Read(0, NULL, &a));
  a.Tick();
  EXPECT_EQ(0, a.GetTimer());
  EXPECT_FALSE(bit->Read(0, NULL, &a));
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
      bit->Write(0, NULL, &a, false);
    }, "802AAC2A");
}

TEST(AutObjTest, IdAndStartingOffset) {
  Automata a(33, 1882);
  EXPECT_EQ(33, a.GetId());
  EXPECT_EQ(1882, a.GetStartingOffset());
}

TEST(AutObjTest, State) {
  Automata a(33, 1882);
  EXPECT_EQ(0, a.GetState());
  EXPECT_EQ(0, a.GetState());
  a.SetState(17);
  EXPECT_EQ(17, a.GetState());
  EXPECT_EQ(17, a.GetState());
}

const insn_t* AD(const string& data) {
  return (const insn_t*)(data.data());
}

TEST(RunnerTest, TrivialRunnerCreateDestroy) {
  automata::Board brd;
  string output;
  brd.Render(&output);
  {
    AutomataRunner (NULL, AD(output));
  }
}

TEST(RunnerTest, TrivialRunnerRun) {
  automata::Board brd;
  string output;
  brd.Render(&output);
  {
    AutomataRunner r(NULL, AD(output));
    r.RunAllAutomata();
  }
}

TEST(RunnerTest, SingleEmptyAutomataBoard) {
  static automata::Board brd;
  DefAut(testaut1, brd, {});
  string output;
  brd.Render(&output);
  {
    AutomataRunner r(NULL, AD(output));
    const auto& all_automata = r.GetAllAutomatas();
    EXPECT_EQ(1U, all_automata.size());
    EXPECT_EQ(0, all_automata[0]->GetId());
    EXPECT_EQ(5, all_automata[0]->GetStartingOffset());
    r.RunAllAutomata();
    r.RunAllAutomata();
    r.RunAllAutomata();
  }
}

#include "automata_tests_helper.hxx"

TEST_F(AutomataTests, DefAct0) {
  automata::Board brd;
  static MockBit mb(this);
  EXPECT_CALL(mb.mock(), Write(_, _, _, false)).Times(1);
  EXPECT_CALL(mb.mock(), Read(_, _, _)).Times(0);
  DefAut(testaut1, brd, {
      auto v1 = ImportVariable(&mb);
      Def().ActReg0(v1);
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, DefAct1) {
  automata::Board brd;
  static MockBit mb(this);
  EXPECT_CALL(mb.mock(), Write(_, _, _, true)).Times(1);
  EXPECT_CALL(mb.mock(), Read(_, _, _)).Times(0);
  DefAut(testaut1, brd, {
      auto v1 = ImportVariable(&mb);
      Def().ActReg1(v1);
    });
  SetupRunner(&brd);

  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, DefIfReg1) {
  automata::Board brd;
  static MockBit rb(this);
  rb.SetArg(42);
  static MockBit rrb(this);
  rrb.SetArg(37);
  static MockBit wb(this);
  EXPECT_CALL(wb.mock(), Write(0, _, _, true));
  EXPECT_CALL(rb.mock(), Read(42, _, _)).WillOnce(Return(true));
  EXPECT_CALL(rrb.mock(), Read(37, _, _)).WillOnce(Return(false));
  DefAut(testaut1, brd, {
      auto rv = ImportVariable(&rb);
      auto rrv = ImportVariable(&rrb);
      auto wv = ImportVariable(&wb);
      Def().IfReg1(*rv).ActReg1(wv);
      Def().IfReg1(*rrv).ActReg1(wv);
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, DefIfReg0) {
  automata::Board brd;
  static MockBit rb(this);
  static MockBit rrb(this);
  static MockBit wb(this);
  EXPECT_CALL(wb.mock(), Write(_, _, _, true));
  EXPECT_CALL(rb.mock(), Read(_, _, _)).WillOnce(Return(true));
  EXPECT_CALL(rrb.mock(), Read(_, _, _)).WillOnce(Return(false));
  DefAut(testaut1, brd, {
      auto rv = ImportVariable(&rb);
      auto rrv = ImportVariable(&rrb);
      auto wv = ImportVariable(&wb);
      Def().IfReg0(*rv).ActReg1(wv);
      Def().IfReg0(*rrv).ActReg1(wv);
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, CopyAutomataBoard) {
  automata::Board brd;
  static FakeBit mbit1(this);
  static FakeBit mbit2(this);
  DefAut(testaut1, brd, {
      auto v1 = ImportVariable(&mbit1);
      auto v2 = ImportVariable(&mbit2);
      Def().IfReg1(*v1).ActReg1(v2);
      Def().IfReg0(*v1).ActReg0(v2);
    });
  SetupRunner(&brd);
  mbit1.Set(false);
  mbit2.Set(true);
  EXPECT_FALSE(mbit1.Get());
  EXPECT_TRUE(mbit2.Get());
  runner_->RunAllAutomata();
  EXPECT_FALSE(mbit1.Get());
  EXPECT_FALSE(mbit2.Get());

  // Second run should not change anything.
  runner_->RunAllAutomata();
  EXPECT_FALSE(mbit1.Get());
  EXPECT_FALSE(mbit2.Get());

  mbit1.Set(true);
  EXPECT_TRUE(mbit1.Get());
  EXPECT_FALSE(mbit2.Get());

  // Now it should be copied back to zero.
  runner_->RunAllAutomata();
  EXPECT_TRUE(mbit1.Get());
  EXPECT_TRUE(mbit2.Get());
}

TEST_F(AutomataTests, ActState) {
  Board brd;
  DefAut(testaut1, brd, {
      StateRef st1(11);
      Def().ActState(st1);
    });
  SetupRunner(&brd);
  Automata* aut = runner_->GetAllAutomatas()[0];
  aut->SetState(0);
  EXPECT_EQ(0, aut->GetState());
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
}

TEST_F(AutomataTests, IfState) {
  Board brd;
  static MockBit wb(this);
  DefAut(testaut1, brd, {
      auto wv = ImportVariable(&wb);
      StateRef st1(11);
      StateRef st2(12);
      Def().IfState(st1).ActReg0(wv);
      Def().IfState(st2).ActReg1(wv);
    });
  SetupRunner(&brd);
  Automata* aut = runner_->GetAllAutomatas()[0];
  aut->SetState(0);
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  EXPECT_CALL(wb.mock(), Write(_, _, _, false));
  aut->SetState(11);
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  EXPECT_CALL(wb.mock(), Write(_, _, _, false));
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  EXPECT_CALL(wb.mock(), Write(_, _, _, true));
  aut->SetState(12);
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  aut->SetState(3);
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());
}

TEST_F(AutomataTests, MultipleAutomata) {
  Board brd;
  static MockBit wb(this);
  DefAut(testaut1, brd, {
      auto wv = ImportVariable(&wb);
      Def().ActReg0(wv);
    });
  DefAut(testaut2, brd, {
      auto wv = ImportVariable(&wb);
      Def().ActReg1(wv);
    });
  SetupRunner(&brd);
  EXPECT_CALL(wb.mock(), Write(_, _, _, true));
  EXPECT_CALL(wb.mock(), Write(_, _, _, false));

  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, StateFlipFlop) {
  Board brd;
  static MockBit wb(this);
  DefAut(testaut1, brd, {
      StateRef st1(11);
      StateRef st2(12);
      // In order to flip-flop between states we need a temporary state.
      StateRef sttmp(13);
      Def().IfState(st2).ActState(sttmp);
      Def().IfState(st1).ActState(st2);
      Def().IfState(sttmp).ActState(st1);
    });
  SetupRunner(&brd);
  Automata* aut = runner_->GetAllAutomatas()[0];
  aut->SetState(0);

  runner_->RunAllAutomata();
  EXPECT_EQ(0, aut->GetState());

  aut->SetState(11);
  runner_->RunAllAutomata();
  EXPECT_EQ(12, aut->GetState());
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  runner_->RunAllAutomata();
  EXPECT_EQ(12, aut->GetState());
}

TEST_F(AutomataTests, TimerBitCountdown) {
  Board brd;
  DefAut(testaut1, brd, {
      StateRef st1(11);
      StateRef st2(12);
      Def().IfState(st1).IfTimerDone().ActState(st2);
      Def().IfState(st2).ActTimer(3).ActState(st1);
    });
  SetupRunner(&brd);
  Automata* aut = runner_->GetAllAutomatas()[0];
  aut->SetState(0);

  runner_->RunAllAutomata();
  EXPECT_EQ(0, aut->GetState());
  EXPECT_EQ(0, aut->GetTimer());
  
  aut->SetState(11);
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(3, aut->GetTimer());
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(3, aut->GetTimer());
  aut->Tick();
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(2, aut->GetTimer());
  aut->Tick();
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(1, aut->GetTimer());
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(1, aut->GetTimer());
  aut->Tick();
  runner_->RunAllAutomata();
  EXPECT_EQ(11, aut->GetState());
  EXPECT_EQ(3, aut->GetTimer());
}

TEST_F(AutomataTests, LoadEventId) {
  Board brd;
  DefAut(testaut1, brd, {
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
  Automata* aut = runner_->GetAllAutomatas()[0];
  runner_->RunAllAutomata();
  string temp;
  automata::EventBasedVariable::CreateEventId(0, 0x554433221166ULL, &temp);
  temp.push_back(0);
  memcpy(program_area_ + aut->GetStartingOffset(),
         temp.data(),
         temp.size());
  runner_->RunAllAutomata();
  EXPECT_EQ(0x554433221166ULL, runner_->GetEventId(0));
  temp.clear();
  automata::EventBasedVariable::CreateEventId(1, 0x443322116655ULL, &temp);
  temp.push_back(0);
  memcpy(program_area_ + aut->GetStartingOffset(),
         temp.data(),
         temp.size());
  runner_->RunAllAutomata();
  EXPECT_EQ(0x443322116655ULL, runner_->GetEventId(1));
}

TEST_F(AutomataTests, EventVar) {
#ifdef CPP_EVENT_HANDLER
  extern void EnsureCompatEventHandlerExists();
  EnsureCompatEventHandlerExists();
#endif
  node_ = nmranet_node_create(0x02010d000001ULL, nmranet_if_,
                              "Test Node", NULL);
  ASSERT_TRUE(node_);
  nmranet_node_user_description(node_, "Test Node");
  nmranet_node_initialized(node_);

  Board brd;
  using automata::EventBasedVariable;
  EventBasedVariable led(&brd,
                         0x0502010202650012ULL,
                         0x0502010202650013ULL,
                         0, OFS_GLOBAL_BITS, 1);
  static automata::GlobalVariable* var;
  var = &led;
  DefAut(testaut1, brd, {
      
ImportVariable(var);
    });
  string output;
  //brd.Render(&output);
  //EXPECT_EQ("", output);
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}


class CanDebugPipeMember : public PipeMember {
 public:
  CanDebugPipeMember(Pipe* parent)
      : parent_(parent) {
    parent_->RegisterMember(this);
  }

  virtual ~CanDebugPipeMember() {
    parent_->UnregisterMember(this);
  }

  virtual void write(const void* buf, size_t count) {
    if (!count) return;
    char outbuf[100];
    const struct can_frame* frame = static_cast<const struct can_frame*>(buf);
    while (count) {
      assert(count >= sizeof(struct can_frame));
      *gc_format_generate(frame, outbuf, 0) = '\0';
      fprintf(stdout,"%s\n", outbuf);
      count -= sizeof(*frame);
    }
  }
 private:
  Pipe* parent_;
};

CanDebugPipeMember printer(&can_pipe0);






TEST_F(AutomataTests, EventVar2) {
  node_ = nmranet_node_create(0x02010d000002ULL, nmranet_if_,
                              "Test Node2", NULL);
  ASSERT_TRUE(node_);
  fprintf(stderr,"node_=%p\n", node_);
  nmranet_node_user_description(node_, "Test Node2");

  nmranet_event_producer(node_, 0x0502010202650012ULL, EVENT_STATE_INVALID);
  nmranet_event_producer(node_, 0x0502010202650013ULL, EVENT_STATE_VALID);
  nmranet_node_initialized(node_);
  WaitForEventThread();

  os_thread_t thread;
  os_thread_create(&thread, "event_process_thread",
                   0, 2048, &AutomataTests::DispatchThread,
                   node_);

  nmranet_event_produce(node_, 0x0502010202650012ULL, EVENT_STATE_INVALID);
  nmranet_event_produce(node_, 0x0502010202650012ULL, EVENT_STATE_VALID);
  nmranet_event_produce(node_, 0x0502010202650013ULL, EVENT_STATE_INVALID);
  nmranet_event_produce(node_, 0x0502010202650013ULL, EVENT_STATE_VALID);
  WaitForEventThread();

  Board brd;
  using automata::EventBasedVariable;
  EventBasedVariable led(&brd,
                         0x0502010202650012ULL,
                         0x0502010202650013ULL,
                         0, OFS_GLOBAL_BITS, 1);
  static automata::GlobalVariable* var;
  WaitForEventThread();
  var = &led;
  DefAut(testaut1, brd, {
      auto wv = ImportVariable(var);
      Def().ActReg0(wv);
      Def().ActReg1(wv);
    });
  string output;
  //brd.Render(&output);
  //EXPECT_EQ("", output);
  SetupRunner(&brd);
  runner_->RunAllAutomata();
  WaitForEventThread();
}


TEST_F(AutomataTests, EmptyTest) {
  WaitForEventThread();
}


int appl_main(int argc, char* argv[]) {
  //__libc_init_array();
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
