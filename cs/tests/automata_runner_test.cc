#include <string>
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "src/automata_runner.h"

#include "../automata/system.hxx"
#include "../automata/operations.hxx"
#include "../automata/variables.hxx"

#include "nmranet_config.h"

#include "pipe/pipe.hxx"
#include "nmranet_can.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Mock;

using automata::Board;
using automata::StateRef;

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

class GMockBit : public ReadWriteBit {
 public:
  MOCK_METHOD2(Read, bool(node_t node, Automata* aut));
  MOCK_METHOD3(Write, void(node_t node, Automata* aut, bool value));
};


class AutomataTests : public testing::Test {
protected:
  class FakeBitPointer : public ReadWriteBit {
   public:
    FakeBitPointer(bool* backend)
        : backend_(backend) {}
    virtual ~FakeBitPointer() {}
    virtual bool Read(node_t node, Automata* aut) {
      return *backend_;
    }
    virtual void Write(node_t node, Automata* aut, bool value) {
      *backend_ = value;
    }

   private:
    bool* backend_;
  };

  class InjectableVar : public automata::GlobalVariable {
   public:
    InjectableVar(AutomataTests* parent) {
      global_var_offset_ = parent->RegisterInjectableBit(this);
      fprintf(stderr, "new mock bit: %p\n", this);
    }

    ~InjectableVar() {
      fprintf(stderr, "destroying mock bit: %p\n", this);
    }

    virtual automata::GlobalVariableId GetId() {
      automata::GlobalVariableId ret;
      ret.id = global_var_offset_;
      return ret;
    }

    virtual void Render(string* /*unused*/) {
      // Mock bits do not have any code in the board preamble.
    }

    // Creates a ReadWriteBit that accesses the mock bit stored in
    // *this. Transfers ownership of the object to the caller.
    virtual ReadWriteBit* CreateBit() = 0;

    int GetGlobalOffset() {
      return global_var_offset_;
    }
   private:
    int global_var_offset_;
  };

  class MockBit : public InjectableVar {
   public:
    MockBit(AutomataTests* parent) : InjectableVar(parent) {
      owned_mockbit_ = new StrictMock<GMockBit>;
      mock_bit_ = owned_mockbit_;
    }

    ~MockBit() {
      delete owned_mockbit_;
    }

    virtual ReadWriteBit* CreateBit() {
      owned_mockbit_ = NULL;
      return mock_bit_;
    }

    GMockBit& mock() {
      return *mock_bit_;
    }

   private:
    GMockBit* owned_mockbit_;
    GMockBit* mock_bit_;
  };

  class FakeBit : public InjectableVar {
  public:
    FakeBit(AutomataTests* parent) : InjectableVar(parent) {}

    ~FakeBit() {
    }

    // Creates a ReadWriteBit that accesses the mock bit stored in
    // *this. Transfers ownership of the object to the caller. There can be an
    // arbitrary number of pointers, they will all modify the same state (also
    // accessed by Get and Set).
    virtual ReadWriteBit* CreateBit() {
      return new FakeBitPointer(&bit_);
    }

    bool Get() { return bit_; }
    void Set(bool b) { bit_ = b; }

  private:
    bool bit_;
  };

  virtual void SetUp() {
    next_bit_offset_ = 4242;
  }

  void SetupRunner(automata::Board* brd) {
    string output;
    brd->Render(&output);
    memcpy(program_area_, output.data(), output.size());
    runner_ = new AutomataRunner(NULL, program_area_);
    for (auto bit : mock_bits_) {
      runner_->InjectBit(bit->GetGlobalOffset(), bit->CreateBit());
    }
  }

  AutomataTests() {
  }

  ~AutomataTests() {
    delete runner_;
  }

  // Registers a new variable for injection into the current runner. Returns a
  // unique global offset that this variable will be injected as.
  int RegisterInjectableBit(InjectableVar* var) {
    mock_bits_.push_back(var);
    return next_bit_offset_++;
  }

protected:
  FakeBit* CreateFakeBit() {
    FakeBit* bit = new FakeBit(next_bit_offset_++);
    mock_bits_.push_back(bit);
    return bit;
  }

  AutomataRunner* runner_;

private:
  // Stores the global variable offset for the next mockbit.
  int next_bit_offset_;
  // Declared mock bits. Not owned.
  vector<InjectableVar*> mock_bits_;
  insn_t program_area_[10000];
};

TEST(RunnerTest, SingleEmptyAutomataBoard) {
  static automata::Board brd;
  DefAut(testaut1, brd, {});
  string output;
  brd.Render(&output);
  {
    AutomataRunner r(NULL, output.data());
    const auto& all_automata = r.GetAllAutomatas();
    EXPECT_EQ(1, all_automata.size());
    EXPECT_EQ(0, all_automata[0]->GetId());
    EXPECT_EQ(5, all_automata[0]->GetStartingOffset());
    r.RunAllAutomata();
    r.RunAllAutomata();
    r.RunAllAutomata();
  }
}

TEST_F(AutomataTests, DefAct0) {
  automata::Board brd;
  static MockBit mb(this);
  EXPECT_CALL(mb.mock(), Write(_, _, false)).Times(1);
  EXPECT_CALL(mb.mock(), Read(_, _)).Times(0);
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
  EXPECT_CALL(mb.mock(), Write(_, _, true)).Times(1);
  EXPECT_CALL(mb.mock(), Read(_, _)).Times(0);
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
  static MockBit rrb(this);
  static MockBit wb(this);
  EXPECT_CALL(wb.mock(), Write(_, _, true));
  EXPECT_CALL(rb.mock(), Read(_, _)).WillOnce(Return(true));
  EXPECT_CALL(rrb.mock(), Read(_, _)).WillOnce(Return(false));
  DefAut(testaut1, brd, {
      auto rv = ImportVariable(&rb);
      auto rrv = ImportVariable(&rrb);
      auto wv = ImportVariable(&wb);
      Def().IfReg1(rv).ActReg1(wv);
      Def().IfReg1(rrv).ActReg1(wv);
    });
  SetupRunner(&brd);
  runner_->RunAllAutomata();
}

TEST_F(AutomataTests, DefIfReg0) {
  automata::Board brd;
  static MockBit rb(this);
  static MockBit rrb(this);
  static MockBit wb(this);
  EXPECT_CALL(wb.mock(), Write(_, _, true));
  EXPECT_CALL(rb.mock(), Read(_, _)).WillOnce(Return(true));
  EXPECT_CALL(rrb.mock(), Read(_, _)).WillOnce(Return(false));
  DefAut(testaut1, brd, {
      auto rv = ImportVariable(&rb);
      auto rrv = ImportVariable(&rrb);
      auto wv = ImportVariable(&wb);
      Def().IfReg0(rv).ActReg1(wv);
      Def().IfReg0(rrv).ActReg1(wv);
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
      Def().IfReg1(v1).ActReg1(v2);
      Def().IfReg0(v1).ActReg0(v2);
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

  EXPECT_CALL(wb.mock(), Write(_, _, false));
  aut->SetState(11);
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  EXPECT_CALL(wb.mock(), Write(_, _, false));
  runner_->RunAllAutomata();
  Mock::VerifyAndClear(&wb.mock());

  EXPECT_CALL(wb.mock(), Write(_, _, true));
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
  EXPECT_CALL(wb.mock(), Write(_, _, true));
  EXPECT_CALL(wb.mock(), Write(_, _, false));

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



int appl_main(int argc, char* argv[]) {
  //__libc_init_array();
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
